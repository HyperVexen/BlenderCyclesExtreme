/* SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "integrator/photon_pass.h"
#include "device/device.h"
#include "device/queue.h"
#include "kernel/device/cpu/globals.h"
#include "kernel/integrator/photon_emit.h"
#include "kernel/integrator/photon_shade_specular.h"
#include "kernel/integrator/photon_store.h"
#include "scene/integrator.h"
#include "scene/object.h"
#include "scene/photon_map.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_nodes.h"
#include "util/log.h"
#include "util/tbb.h"
#include "util/time.h"

CCL_NAMESPACE_BEGIN

PhotonPass::PhotonPass() = default;
PhotonPass::~PhotonPass() = default;

static inline ThreadKernelGlobalsCPU *kernel_thread_globals_get(
    vector<ThreadKernelGlobalsCPU> &kernel_thread_globals)
{
  const int thread_index = tbb::this_task_arena::current_thread_index();
  DCHECK_GE(thread_index, 0);
  DCHECK_LT(thread_index, (int)kernel_thread_globals.size());
  return &kernel_thread_globals[thread_index];
}

static bool shader_is_specular_caster(Shader *shader)
{
  if (!shader || !shader->graph) {
    return false;
  }
  for (ShaderNode *node : shader->graph->nodes) {
    if (node->type == GlassBsdfNode::get_node_type() ||
        node->type == RefractionBsdfNode::get_node_type())
    {
      return true;
    }
    if (node->type == PrincipledBsdfNode::get_node_type()) {
      PrincipledBsdfNode *pnode = static_cast<PrincipledBsdfNode *>(node);
      ShaderInput *trans_input = pnode->input("Transmission Weight");
      if (pnode->get_transmission_weight() > 0.01f ||
          (trans_input && trans_input->link != nullptr) ||
          pnode->get_metallic() > 0.5f)
      {
        return true;
      }
    }
  }
  return false;
}

static void build_caster_list(Scene *scene, vector<CasterBound> &casters)
{
  casters.clear();
  for (Object *object : scene->objects) {
    if (object->bounds.valid()) {
      bool is_caster = object->get_is_caustics_caster();
      if (!is_caster && object->get_geometry()) {
        Geometry *geom = object->get_geometry();
        for (Node *node : geom->get_used_shaders()) {
          Shader *shader = static_cast<Shader *>(node);
          if (shader_is_specular_caster(shader)) {
            is_caster = true;
            break;
          }
        }
      }
      if (is_caster) {
        CasterBound bound;
        bound.center = object->bounds.center();
        bound.radius = fmaxf(0.5f * len(object->bounds.size()), 1e-3f);
        casters.push_back(bound);
      }
    }
  }
}

bool PhotonPass::execute_cpu(Scene *scene,
                             PhotonMap *photon_map,
                             Device *device,
                             const uint num_photons,
                             const int max_specular_bounces,
                             const float clamp_flux,
                             const uint seed)
{
  if (!scene || !photon_map || num_photons == 0) {
    return false;
  }

  /* Defensive routing: if invoked with a GPU device, safely redirect to GPU execution
   * to guarantee zero crashes from acquire_cpu_kernel_thread_globals(). */
  if (device && device->info.type != DEVICE_CPU) {
    return execute_gpu(
        scene, photon_map, device, num_photons, max_specular_bounces, clamp_flux, seed);
  }

  /* Identify specular caustic casters in scene (Milestone 5). */
  vector<CasterBound> casters;
  build_caster_list(scene, casters);
  if (casters.empty()) {
    /* Performance rule #2: Near-zero cost when on but useless.
     * No specular casters in scene, so skip photon tracing entirely. */
    photon_map->set_stored_count(0);
    photon_map->set_emitted_count(0);
    LOG_INFO << "PhotonPass: No specular casters found in scene; skipping photon pass.";
    return true;
  }

  const uint max_photons = (uint)scene->integrator->get_photon_caustics_max_photons();
  photon_map->allocate(device, max_photons);
  photon_map->clear();

  PhotonRecord *photon_data = photon_map->data();
  if (!photon_data) {
    return false;
  }

  vector<ThreadKernelGlobalsCPU> *kernel_thread_globals =
      device->acquire_cpu_kernel_thread_globals();
  if (!kernel_thread_globals || kernel_thread_globals->empty()) {
    return false;
  }

  uint stored_count = 0;
  const double start_time = time_dt();
  const int num_casters = (int)casters.size();
  const CasterBound *caster_data = casters.data();

  tbb::task_arena local_arena(device->info.cpu_threads);
  local_arena.execute([&]() {
    parallel_for(int64_t(0), int64_t(num_photons), [&](int64_t photon_index) {
      ThreadKernelGlobalsCPU *kg = kernel_thread_globals_get(*kernel_thread_globals);

      PhotonPath photon;
      if (photon_emit(kg,
                      (uint)photon_index,
                      num_photons,
                      seed,
                      caster_data,
                      num_casters,
                      &photon))
      {
        photon_trace_and_store(kg,
                               &photon,
                               photon_data,
                               &stored_count,
                               max_photons,
                               max_specular_bounces,
                               clamp_flux);
      }
    });
  });

  device->release_cpu_kernel_thread_globals();

  const uint final_stored = min(stored_count, max_photons);
  photon_map->set_stored_count(final_stored);
  photon_map->set_emitted_count(num_photons);

  /* Build spatial hash grid and upload to device scene. */
  photon_map->build_grid(scene, device);

  const double elapsed_seconds = time_dt() - start_time;
  const double throughput = (elapsed_seconds > 0.0) ? (double)num_photons / elapsed_seconds : 0.0;

  LOG_INFO << "PhotonPass: Emitted " << num_photons << " photons, stored " << final_stored
           << " caustic photons (targeting " << num_casters << " casters) in " << elapsed_seconds
           << "s (" << throughput << " photons/sec).";
  printf("PhotonPass: Emitted %u photons, stored %u caustic photons (targeting %d casters) in %.2fs (%.0f photons/sec)\n",
         num_photons, final_stored, num_casters, elapsed_seconds, throughput);
  fflush(stdout);

  return true;
}

bool PhotonPass::execute_gpu(Scene *scene,
                             PhotonMap *photon_map,
                             Device *device,
                             const uint num_photons,
                             const int max_specular_bounces,
                             const float clamp_flux,
                             const uint seed)
{
  if (!scene || !photon_map || !device || num_photons == 0) {
    return false;
  }

  /* Identify specular caustic casters in scene (Milestone 5). */
  vector<CasterBound> casters;
  build_caster_list(scene, casters);
  if (casters.empty()) {
    /* Performance rule #2: Near-zero cost when on but useless.
     * No specular casters in scene, so skip photon tracing entirely. */
    photon_map->set_stored_count(0);
    photon_map->set_emitted_count(0);
    LOG_INFO << "PhotonPass [GPU]: No specular casters found in scene; skipping GPU photon pass.";
    return true;
  }

  const uint max_photons = (uint)scene->integrator->get_photon_caustics_max_photons();
  photon_map->allocate(device, max_photons);
  photon_map->clear();

  device_vector<PhotonRecord> *photons_vec = photon_map->get_photons();
  if (!photons_vec) {
    LOG_ERROR << "PhotonPass [GPU]: Failed to acquire photon buffer.";
    return false;
  }
  photons_vec->zero_to_device();
  if (photons_vec->device_pointer == 0) {
    LOG_ERROR << "PhotonPass [GPU]: Failed to allocate GPU memory for photons.";
    return false;
  }

  /* Upload caster bounds to device. */
  const int num_casters = (int)casters.size();
  device_vector<CasterBound> d_casters(device, "photon_casters", MEM_READ_ONLY);
  d_casters.alloc(num_casters);
  memcpy(d_casters.data(), casters.data(), num_casters * sizeof(CasterBound));
  d_casters.copy_to_device();

  /* Atomic counter for stored photons on device. */
  device_vector<uint> d_stored_count(device, "photon_stored_count", MEM_READ_WRITE);
  d_stored_count.alloc(1);
  d_stored_count.zero_to_device();

  if (d_casters.device_pointer == 0 || d_stored_count.device_pointer == 0) {
    LOG_ERROR << "PhotonPass [GPU]: Failed to allocate GPU auxiliary buffers.";
    return false;
  }

  /* Create GPU device queue. */
  unique_ptr<DeviceQueue> queue = device->gpu_queue_create();
  if (!queue) {
    LOG_ERROR << "PhotonPass [GPU]: Failed to create GPU device queue.";
    return false;
  }
  queue->init_execution();

  printf("PhotonPass [GPU]: num_lights=%d, lights_device_ptr=0x%llx\n",
         scene->dscene.data.integrator.num_lights,
         (unsigned long long)scene->dscene.lights.device_pointer);
  for (int i = 0; i < scene->dscene.data.integrator.num_lights; ++i) {
    const KernelLight *kl = &scene->dscene.lights[i];
    printf("  Light %d: type=%d, strength=(%.3f, %.3f, %.3f)\n",
           i, kl->type, kl->strength[0], kl->strength[1], kl->strength[2]);
  }
  fflush(stdout);

  const double start_time = time_dt();

  /* Launch photon emission and tracing kernel on GPU.
   * Work size is num_photons (one thread per photon). */
  device_ptr d_photons = photons_vec->device_pointer;
  device_ptr d_stored = d_stored_count.device_pointer;
  device_ptr d_casters_ptr = d_casters.device_pointer;
  const int32_t i_num_photons = (int32_t)num_photons;
  const int32_t i_max_photons = (int32_t)max_photons;
  const int32_t i_max_bounces = (int32_t)max_specular_bounces;
  const int32_t i_seed = (int32_t)seed;
  const int32_t i_num_casters = (int32_t)num_casters;

  const DeviceKernelArguments args(&d_photons,
                                   &d_stored,
                                   &i_num_photons,
                                   &i_max_photons,
                                   &i_max_bounces,
                                   &clamp_flux,
                                   &i_seed,
                                   &d_casters_ptr,
                                   &i_num_casters);

  queue->enqueue(DEVICE_KERNEL_PHOTON_EMIT_AND_TRACE, i_num_photons, args);

  /* Read back stored count from GPU. */
  queue->copy_from_device(d_stored_count);
  if (!queue->synchronize()) {
    LOG_ERROR << "PhotonPass [GPU]: GPU kernel synchronization failed.";
    return false;
  }

  const uint raw_stored = d_stored_count.data()[0];
  const uint final_stored = min(raw_stored, max_photons);
  photon_map->set_stored_count(final_stored);
  photon_map->set_emitted_count(num_photons);

  if (final_stored > 0) {
    /* Copy stored photons back to host memory for hash grid construction. */
    queue->copy_from_device(*photons_vec);
    queue->synchronize();
  }

  /* Build spatial hash grid and upload to device scene. */
  photon_map->build_grid(scene, device);

  const double elapsed_seconds = time_dt() - start_time;
  const double throughput = (elapsed_seconds > 0.0) ? (double)num_photons / elapsed_seconds : 0.0;

  LOG_INFO << "PhotonPass [GPU]: Emitted " << num_photons << " photons, stored " << final_stored
           << " caustic photons (targeting " << num_casters << " casters) in " << elapsed_seconds
           << "s (" << throughput << " photons/sec).";
  printf("PhotonPass [GPU]: Emitted %u photons, stored %u caustic photons (targeting %d casters) in %.2fs (%.0f photons/sec)\n",
         num_photons, final_stored, num_casters, elapsed_seconds, throughput);
  fflush(stdout);

  return true;
}

CCL_NAMESPACE_END
