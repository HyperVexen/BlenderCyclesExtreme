/* SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/integrator/photon_grid.h"
#include "kernel/integrator/surface_shader.h"
#include "kernel/film/light_passes.h"
#include "util/math.h"

CCL_NAMESPACE_BEGIN

/* Gather caustic radiance from the photon map at a surface hit point sd. */
ccl_device Spectrum photon_gather(KernelGlobals kg,
                                  IntegratorState state,
                                  ccl_private ShaderData *sd)
{
  const int num_stored = kernel_data.integrator.photon_num_stored;
  if (num_stored <= 0) {
    return zero_spectrum();
  }

  /* Check gather mode: PRIMARY (0) only gathers at the first diffuse hit. */
  if (kernel_data.integrator.photon_caustics_gather_mode == 0) {
    if (INTEGRATOR_STATE(state, path, diffuse_bounce) > 0) {
      return zero_spectrum();
    }
  }

  const float R = kernel_data.integrator.photon_caustics_radius;
  if (R <= 0.0f) {
    return zero_spectrum();
  }

  const float3 P = sd->P;
  const float3 N = sd->N;

  /* Bounding box rejection. */
  const float4 bmin = kernel_data.integrator.photon_bounds_min;
  const float4 bmax = kernel_data.integrator.photon_bounds_max;
  if (P.x < bmin.x - R || P.x > bmax.x + R ||
      P.y < bmin.y - R || P.y > bmax.y + R ||
      P.z < bmin.z - R || P.z > bmax.z + R)
  {
    return zero_spectrum();
  }

  /* Evaluate surface BSDF at normal N to obtain diffuse BRDF (rho / pi). */
  BsdfEval bsdf_eval ccl_optional_struct_init;
  float avg_roughness_squared = 0.0f;
  surface_shader_bsdf_eval(kg, state, sd, N, &bsdf_eval, 0, avg_roughness_squared);

  Spectrum diffuse_brdf = bsdf_eval.diffuse;
  if (is_zero(diffuse_brdf)) {
    diffuse_brdf = bsdf_eval_sum(&bsdf_eval);
  }
  if (is_zero(diffuse_brdf)) {
    return zero_spectrum();
  }

  const float inv_cell_size = kernel_data.integrator.photon_inv_cell_size;
  const uint grid_size = (uint)kernel_data.integrator.photon_grid_size;
  const int3 base = photon_grid_gather_base(P, make_float3(bmin.x, bmin.y, bmin.z), inv_cell_size);

  /* Query and deduplicate the 8 neighbor cell hash buckets. */
  uint unique_buckets[8];
  int num_buckets = 0;

  for (int dz = 0; dz <= 1; ++dz) {
    for (int dy = 0; dy <= 1; ++dy) {
      for (int dx = 0; dx <= 1; ++dx) {
        const uint h = photon_grid_hash(base.x + dx, base.y + dy, base.z + dz, grid_size);
        bool duplicate = false;
        for (int k = 0; k < num_buckets; ++k) {
          if (unique_buckets[k] == h) {
            duplicate = true;
            break;
          }
        }
        if (!duplicate) {
          unique_buckets[num_buckets++] = h;
        }
      }
    }
  }

  const float R2 = R * R;
  const float inv_R2 = 1.0f / R2;
  const float kernel_norm = 2.0f / (M_PI_F * R2); /* 2D Epanechnikov normalization */
  const float clamp_flux = kernel_data.integrator.photon_caustics_clamp;

  Spectrum accumulated_flux = zero_spectrum();

  for (int b = 0; b < num_buckets; ++b) {
    const uint bucket = unique_buckets[b];
    const uint start = kernel_data_fetch(photon_cell_offsets, bucket);
    const uint end = kernel_data_fetch(photon_cell_offsets, bucket + 1);

    for (uint i = start; i < end; ++i) {
      const uint photon_index = kernel_data_fetch(photon_indices, i);
      const ccl_global PhotonRecord *photon = &kernel_data_fetch(photons, photon_index);

      const float3 p_pos = photon->get_position();
      const float3 diff = P - p_pos;
      const float d2 = dot(diff, diff);
      if (d2 >= R2) {
        continue;
      }

      const float3 p_dir = photon->get_direction();
      /* Reject photons arriving from behind the surface. */
      if (dot(N, p_dir) <= 0.0f) {
        continue;
      }

      /* Epanechnikov kernel weighting K(d) = 2/(pi*R^2) * (1 - d^2/R^2). */
      const float k_weight = kernel_norm * (1.0f - d2 * inv_R2);
      float3 f = photon->get_flux();
      if (clamp_flux > 0.0f) {
        const float m = reduce_max(f);
        if (m > clamp_flux) {
          f *= (clamp_flux / m);
        }
      }

      accumulated_flux += make_spectrum(f * k_weight);
    }
  }

  return diffuse_brdf * accumulated_flux;
}

/* Perform photon caustics gather at surface and write contribution to film buffer. */
ccl_device_forceinline void integrate_surface_photon_caustics(
    KernelGlobals kg,
    IntegratorState state,
    ccl_private ShaderData *sd,
    ccl_global float *ccl_restrict render_buffer)
{
  if (!(sd->runtime_flag & SR_BSDF_HAS_EVAL)) {
    return;
  }

  const Spectrum L = photon_gather(kg, state, sd);
  if (is_zero(L)) {
    return;
  }

  Spectrum contribution = INTEGRATOR_STATE(state, path, throughput) * L;

  ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);
  const PathRayVisibility path_visibility = INTEGRATOR_STATE(state, path, visibility);
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  const int sample = INTEGRATOR_STATE(state, path, sample);

  film_write_combined_pass(kg, path_visibility, path_flag, sample, contribution, buffer);

#ifdef __PASSES__
  if (kernel_data.film.light_pass_flag & PASSMASK(DIFFUSE_INDIRECT)) {
    film_write_pass_spectrum(buffer + kernel_data.film.pass_diffuse_indirect, contribution);
  }
#endif
}

CCL_NAMESPACE_END
