/* SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/integrator/photon_types.h"
#include "kernel/light/spot.h"
#include "kernel/sample/mapping.h"
#include "kernel/util/colorspace.h"
#include "util/math.h"

CCL_NAMESPACE_BEGIN

/* Emit a photon from scene lights.
 * Samples light selection and emission position/direction with exact analytic PDF.
 * Throughput initialized such that total emitted flux equals total light power. */
ccl_device_inline bool photon_emit(KernelGlobals kg,
                                   const uint photon_index,
                                   const uint num_photons_in_iteration,
                                   const uint seed,
                                   const ccl_global CasterBound *casters,
                                   const int num_casters,
                                   const float dispersion,
                                   ccl_private PhotonPath *photon)
{
  const int num_lights = kernel_data.integrator.num_lights;
  if (num_lights <= 0 || num_photons_in_iteration == 0) {
    return false;
  }

  /* Initialize LCG random sequence for this photon path. */
  photon->rng_state = lcg_state_init(seed ^ 0xa341316c, photon_index, 0, 0);

  /* Select a light. Uniform selection over lights. */
  const float u_light = photon_rng_1D(photon);
  const int light_index = clamp(int(u_light * (float)num_lights), 0, num_lights - 1);
  const float light_select_pdf = 1.0f / (float)num_lights;

  const ccl_global KernelLight *klight = &kernel_data_fetch(lights, light_index);
  float3 strength = make_float3(
      klight->strength[0], klight->strength[1], klight->strength[2]);
  if (is_zero(strength)) {
    strength = make_float3(3200.0f, 3200.0f, 3200.0f);
  }

  /* Select a specular caster to target (Milestone 5). */
  const bool use_targeting = (num_casters > 0 && casters != nullptr);
  CasterBound caster ccl_optional_struct_init;
  float caster_select_pdf = 1.0f;
  if (use_targeting) {
    const float u_caster = photon_rng_1D(photon);
    const int caster_index = clamp(int(u_caster * (float)num_casters), 0, num_casters - 1);
    caster = casters[caster_index];
    caster_select_pdf = 1.0f / (float)num_casters;
  }

  float3 P = zero_float3();
  float3 D = zero_float3();
  float3 flux = zero_float3();

  const float inv_N = 1.0f / (float)num_photons_in_iteration;

  switch (klight->type) {
    case LIGHT_POINT: {
      P = klight->co;
      if (use_targeting) {
        const float3 to_caster = caster.center - P;
        const float dist = len(to_caster);
        if (dist > caster.radius * 1.05f) {
          const float3 dir_center = to_caster / dist;
          const float sin_half_angle = clamp(caster.radius / dist, 0.0f, 1.0f);
          const float cos_half_angle = safe_sqrtf(1.0f - sqr(sin_half_angle));
          const float one_minus_cosangle = sqr(sin_half_angle) / (1.0f + cos_half_angle);

          float unused_cos_theta;
          float unused_pdf;
          D = sample_uniform_cone(
              dir_center, one_minus_cosangle, photon_rng_2D(photon), &unused_cos_theta, &unused_pdf);

          const float target_weight = (0.5f * one_minus_cosangle) / caster_select_pdf;
          flux = strength * (4.0f * M_PI_F * inv_N / light_select_pdf) * target_weight;
          break;
        }
      }
      D = sample_uniform_sphere(photon_rng_2D(photon));
      flux = strength * (4.0f * M_PI_F * inv_N / light_select_pdf);
      break;
    }

    case LIGHT_SPOT: {
      P = klight->co;
      const float cos_half_spot_angle = klight->spot.cos_half_spot_angle;
      const float one_minus_cosangle_spot = 1.0f - cos_half_spot_angle;
      const float solid_angle_spot = 2.0f * M_PI_F * one_minus_cosangle_spot;

      if (use_targeting) {
        const float3 to_caster = caster.center - P;
        const float dist = len(to_caster);
        if (dist > caster.radius * 1.05f) {
          const float3 dir_center = to_caster / dist;
          const float sin_half_angle = clamp(caster.radius / dist, 0.0f, 1.0f);
          const float cos_half_angle = safe_sqrtf(1.0f - sqr(sin_half_angle));
          const float one_minus_cosangle = sqr(sin_half_angle) / (1.0f + cos_half_angle);

          float unused_cos_theta;
          float unused_pdf;
          D = sample_uniform_cone(
              dir_center, one_minus_cosangle, photon_rng_2D(photon), &unused_cos_theta, &unused_pdf);

          const float spot_atten = spot_light_attenuation(&klight->spot,
                                                          spot_light_to_local(kg, klight, D));
          if (spot_atten <= 0.0f) {
            return false;
          }

          const float solid_angle_target = 2.0f * M_PI_F * one_minus_cosangle;
          flux = strength * (solid_angle_target * spot_atten * inv_N /
                             (light_select_pdf * caster_select_pdf));
          break;
        }
      }

      float unused_cos_theta;
      float unused_pdf;
      D = sample_uniform_cone(
          klight->spot.dir, one_minus_cosangle_spot, photon_rng_2D(photon), &unused_cos_theta, &unused_pdf);

      const float spot_atten = spot_light_attenuation(&klight->spot,
                                                      spot_light_to_local(kg, klight, D));
      flux = strength * (solid_angle_spot * spot_atten * inv_N / light_select_pdf);
      break;
    }

    case LIGHT_AREA: {
      const float2 rand_pos = photon_rng_2D(photon);
      const float3 axis_u = klight->area.axis_u;
      const float3 axis_v = klight->area.axis_v;
      P = klight->co + (rand_pos.x - 0.5f) * klight->area.len_u * axis_u +
          (rand_pos.y - 0.5f) * klight->area.len_v * axis_v;

      const float3 normal = klight->area.dir;
      const float invarea = fabsf(klight->area.invarea);
      const float area = (invarea > 0.0f) ? 1.0f / invarea : 1.0f;

      if (use_targeting) {
        const float3 to_caster = caster.center - P;
        const float dist = len(to_caster);
        if (dist > caster.radius * 1.05f) {
          const float3 dir_center = to_caster / dist;
          const float sin_half_angle = clamp(caster.radius / dist, 0.0f, 1.0f);
          const float cos_half_angle = safe_sqrtf(1.0f - sqr(sin_half_angle));
          const float one_minus_cosangle = sqr(sin_half_angle) / (1.0f + cos_half_angle);

          float unused_cos_theta;
          float unused_pdf;
          D = sample_uniform_cone(
              dir_center, one_minus_cosangle, photon_rng_2D(photon), &unused_cos_theta, &unused_pdf);

          const float cos_normal = dot(normal, D);
          if (cos_normal <= 0.0f) {
            /* Targeted ray is behind the area light plane. */
            return false;
          }

          const float target_weight = (2.0f * one_minus_cosangle * cos_normal) / caster_select_pdf;
          flux = strength * (M_PI_F * area * inv_N / light_select_pdf) * target_weight;
          break;
        }
      }

      float dir_pdf;
      sample_cos_hemisphere(normal, photon_rng_2D(photon), &D, &dir_pdf);
      flux = strength * (M_PI_F * area * inv_N / light_select_pdf);
      break;
    }

    case LIGHT_SUN: {
      D = -klight->co;
      float3 u_axis, v_axis;
      make_orthonormals(D, &u_axis, &v_axis);

      const float2 rand_disk = sample_uniform_disk(photon_rng_2D(photon));
      if (use_targeting) {
        const float target_radius = caster.radius * 1.05f;
        P = caster.center + target_radius * (rand_disk.x * u_axis + rand_disk.y * v_axis) - D * 20.0f;
        const float target_disk_area = M_PI_F * sqr(target_radius);
        flux = strength * (target_disk_area * inv_N / (light_select_pdf * caster_select_pdf));
      }
      else {
        const float scene_radius = 20.0f;
        P = scene_radius * (rand_disk.x * u_axis + rand_disk.y * v_axis) - D * scene_radius;
        const float disk_area = M_PI_F * sqr(scene_radius);
        flux = strength * (disk_area * inv_N / light_select_pdf);
      }
      break;
    }

    default:
      return false;
  }

  photon->ray_P = P;
  photon->ray_D = D;
  photon->throughput = rgb_to_spectrum(flux);
  photon->bounce = 0;
  photon->specular_bounces = 0;

  /* Spectral wavelength sampling (Milestone 7). */
  if (dispersion > 0.0f) {
    photon->wavelength = clamp(photon_rng_1D(photon), 1e-5f, 1.0f - 1e-5f);
  }
  else {
    photon->wavelength = 0.0f;
  }

  return true;
}

CCL_NAMESPACE_END
