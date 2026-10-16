/* SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/photon_map.h"
#include "kernel/integrator/photon_grid.h"
#include "scene/integrator.h"
#include "scene/scene.h"
#include "util/log.h"
#include "util/math.h"
#include <cfloat>
#include <vector>

CCL_NAMESPACE_BEGIN

PhotonMap::PhotonMap() = default;

PhotonMap::~PhotonMap() = default;

void PhotonMap::reset(const float initial_radius,
                      const float alpha_val,
                      const uint max_photons_budget)
{
  initial_radius_ = initial_radius;
  current_radius_ = initial_radius;
  alpha_ = alpha_val;
  max_photons_ = max_photons_budget;
  iteration_ = 0;
  stored_count_ = 0;
  emitted_count_ = 0;
}

void PhotonMap::next_iteration()
{
  /* SPPM global radius reduction formula:
   * r^2_{i+1} = r^2_i * (i + alpha) / (i + 1) */
  const float factor = ((float)iteration_ + alpha_) / ((float)iteration_ + 1.0f);
  current_radius_ = current_radius_ * sqrtf(factor);
  iteration_++;

  LOG_INFO << "PhotonMap: Advanced to iteration " << iteration_
           << ", updated gather radius: " << current_radius_;
}

void PhotonMap::allocate(Device *device, const size_t count)
{
  if (!photons_ || photons_->device != device) {
    photons_ = make_unique<device_vector<PhotonRecord>>(device, "photon_map", MEM_READ_WRITE);
  }
  photons_->alloc(count);
}

void PhotonMap::device_free(Device * /*device*/)
{
  if (photons_) {
    photons_->free();
    photons_.reset();
  }
}

void PhotonMap::clear()
{
  stored_count_ = 0;
  emitted_count_ = 0;
}

bool PhotonMap::build_grid(Scene *scene, Device *device)
{
  if (!scene || !device) {
    return false;
  }

  const uint N = stored_count_;
  if (N == 0) {
    scene->dscene.data.integrator.photon_num_stored = 0;
    scene->dscene.photons.free();
    scene->dscene.photon_cell_offsets.free();
    scene->dscene.photon_indices.free();
    device->const_copy_to("data", &scene->dscene.data, sizeof(scene->dscene.data));
    return true;
  }

  const PhotonRecord *photon_data = data();
  if (!photon_data) {
    return false;
  }

  /* 1. Compute bounding box of stored photons. */
  float3 bounds_min = make_float3(FLT_MAX, FLT_MAX, FLT_MAX);
  float3 bounds_max = make_float3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  for (uint i = 0; i < N; ++i) {
    const float3 p = photon_data[i].get_position();
    bounds_min = min(bounds_min, p);
    bounds_max = max(bounds_max, p);
  }

  printf("PhotonMap::build_grid: stored=%u photons, bounds=[%.3f, %.3f, %.3f] to [%.3f, %.3f, %.3f]\n",
         N, bounds_min.x, bounds_min.y, bounds_min.z, bounds_max.x, bounds_max.y, bounds_max.z);
  if (N > 0) {
    const float3 p0 = photon_data[0].get_position();
    const float3 f0 = photon_data[0].get_flux();
    const float3 d0 = photon_data[0].get_direction();
    const uint32_t *raw0 = (const uint32_t *)&photon_data[0];
    printf("  Photon 0: pos=(%.6f, %.6f, %.6f), dir=(%.3f, %.3f, %.3f), flux=(%e, %e, %e)\n",
           p0.x, p0.y, p0.z, d0.x, d0.y, d0.z, f0.x, f0.y, f0.z);
    printf("  Photon 0 Raw Words: [0..3]=0x%08x 0x%08x 0x%08x 0x%08x, [4..7]=0x%08x 0x%08x 0x%08x 0x%08x\n",
           raw0[0], raw0[1], raw0[2], raw0[3], raw0[4], raw0[5], raw0[6], raw0[7]);
    const float *raw0_f = (const float *)&photon_data[0];
    printf("  Photon 0 Raw Floats: [%.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f]\n",
           raw0_f[0], raw0_f[1], raw0_f[2], raw0_f[3], raw0_f[4], raw0_f[5], raw0_f[6], raw0_f[7]);

    double total_flux_r = 0.0, total_flux_g = 0.0, total_flux_b = 0.0;
    float max_flux_mag = 0.0f;
    uint non_zero_count = 0;
    for (uint i = 0; i < N; ++i) {
      const float3 f = photon_data[i].get_flux();
      total_flux_r += f.x;
      total_flux_g += f.y;
      total_flux_b += f.z;
      const float mag = fmaxf(f.x, fmaxf(f.y, f.z));
      if (mag > 0.0f) {
        non_zero_count++;
      }
      if (mag > max_flux_mag) {
        max_flux_mag = mag;
      }
    }
    printf("  PhotonMap Flux Stats: non_zero=%u/%u, max_flux=%e, total_flux=(%e, %e, %e)\n",
           non_zero_count, N, max_flux_mag, total_flux_r, total_flux_g, total_flux_b);
  }
  fflush(stdout);

  /* 2. Grid sizing and cell dimensions. Cell size = 2 * R. */
  const float radius = current_radius_;
  const float cell_size = 2.0f * radius;
  const float inv_cell_size = (cell_size > 0.0f) ? (1.0f / cell_size) : 1.0f;

  /* Power of two bucket count between 1024 and 1048576 (2^20). */
  uint grid_size = 1024u;
  if (2u * N > 1024u) {
    grid_size = next_power_of_two(2u * N);
    if (grid_size > 1048576u) {
      grid_size = 1048576u;
    }
  }

  /* 3. Pass 1: Count photons per bucket. */
  std::vector<uint> cell_counts(grid_size, 0u);
  for (uint i = 0; i < N; ++i) {
    const float3 p = photon_data[i].get_position();
    const int3 coord = photon_grid_coord(p, bounds_min, inv_cell_size);
    const uint h = photon_grid_hash(coord.x, coord.y, coord.z, grid_size);
    cell_counts[h]++;
  }

  /* 4. Pass 2: Prefix sum for cell offsets (size grid_size + 1). */
  scene->dscene.photon_cell_offsets.alloc(grid_size + 1);
  uint *offsets = scene->dscene.photon_cell_offsets.data();
  offsets[0] = 0u;
  for (uint h = 0; h < grid_size; ++h) {
    offsets[h + 1] = offsets[h] + cell_counts[h];
  }

  /* 5. Pass 3: Scatter photon indices. */
  std::vector<uint> current_offsets(offsets, offsets + grid_size);
  scene->dscene.photon_indices.alloc(N);
  uint *indices = scene->dscene.photon_indices.data();
  for (uint i = 0; i < N; ++i) {
    const float3 p = photon_data[i].get_position();
    const int3 coord = photon_grid_coord(p, bounds_min, inv_cell_size);
    const uint h = photon_grid_hash(coord.x, coord.y, coord.z, grid_size);
    const uint target = current_offsets[h]++;
    indices[target] = i;
  }

  /* 6. Copy photon records to scene->dscene.photons. */
  scene->dscene.photons.alloc(N);
  memcpy(scene->dscene.photons.data(), photon_data, N * sizeof(PhotonRecord));

  /* 7. Upload to device memory. */
  scene->dscene.photons.copy_to_device();
  scene->dscene.photon_cell_offsets.copy_to_device();
  scene->dscene.photon_indices.copy_to_device();

  grid_size_ = grid_size;
  cell_size_ = cell_size;
  inv_cell_size_ = inv_cell_size;
  bounds_min_ = bounds_min;
  bounds_max_ = bounds_max;

  /* 8. Update integrator kernel data. */
  scene->dscene.data.integrator.photon_num_stored = (int)N;
  scene->dscene.data.integrator.photon_grid_size = (int)grid_size;
  scene->dscene.data.integrator.photon_cell_size = cell_size;
  scene->dscene.data.integrator.photon_inv_cell_size = inv_cell_size;
  scene->dscene.data.integrator.photon_caustics_radius = radius;
  scene->dscene.data.integrator.photon_caustics_dispersion =
      scene->integrator->get_photon_caustics_dispersion();
  scene->dscene.data.integrator.photon_caustics_alpha =
      scene->integrator->get_photon_caustics_alpha();
  scene->dscene.data.integrator.photon_bounds_min = make_float4(
      bounds_min.x, bounds_min.y, bounds_min.z, 0.0f);
  scene->dscene.data.integrator.photon_bounds_max = make_float4(
      bounds_max.x, bounds_max.y, bounds_max.z, 0.0f);

  device->const_copy_to("data", &scene->dscene.data, sizeof(scene->dscene.data));


  printf("  Device pointers: photons=0x%llx, offsets=0x%llx, indices=0x%llx, photon_num_stored=%d\n",
         (unsigned long long)scene->dscene.photons.device_pointer,
         (unsigned long long)scene->dscene.photon_cell_offsets.device_pointer,
         (unsigned long long)scene->dscene.photon_indices.device_pointer,
         scene->dscene.data.integrator.photon_num_stored);
  fflush(stdout);

  LOG_INFO << "PhotonMap: Hash grid built successfully: " << N << " photons, " << grid_size
           << " buckets, cell size " << cell_size << ", bounds [" << bounds_min.x << ","
           << bounds_min.y << "," << bounds_min.z << "] to [" << bounds_max.x << ","
           << bounds_max.y << "," << bounds_max.z << "]";

  return true;
}

size_t PhotonMap::get_stored_count() const
{
  return stored_count_;
}

void PhotonMap::set_stored_count(const size_t count)
{
  stored_count_ = (uint)count;
}

size_t PhotonMap::get_emitted_count() const
{
  return emitted_count_;
}

void PhotonMap::set_emitted_count(const size_t count)
{
  emitted_count_ = (uint)count;
}

float PhotonMap::get_current_radius() const
{
  return current_radius_;
}

void PhotonMap::set_current_radius(const float r)
{
  current_radius_ = r;
}

int PhotonMap::get_iteration() const
{
  return iteration_;
}

PhotonRecord *PhotonMap::data()
{
  return photons_ ? photons_->data() : nullptr;
}

const PhotonRecord *PhotonMap::data() const
{
  return photons_ ? photons_->data() : nullptr;
}

device_vector<PhotonRecord> *PhotonMap::get_photons()
{
  return photons_.get();
}

uint PhotonMap::get_grid_size() const
{
  return grid_size_;
}

float PhotonMap::get_cell_size() const
{
  return cell_size_;
}

float PhotonMap::get_inv_cell_size() const
{
  return inv_cell_size_;
}

float3 PhotonMap::get_bounds_min() const
{
  return bounds_min_;
}

float3 PhotonMap::get_bounds_max() const
{
  return bounds_max_;
}

CCL_NAMESPACE_END

