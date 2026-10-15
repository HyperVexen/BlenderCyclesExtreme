/* SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/bvh/bvh.h"
#include "kernel/closure/bsdf.h"
#include "kernel/geom/shader_data.h"
#include "kernel/integrator/photon_store.h"
#include "kernel/integrator/photon_types.h"
#include "kernel/integrator/surface_shader.h"
#include "kernel/util/colorspace.h"
#include "util/math.h"

#ifndef __KERNEL_GPU__
#  include <atomic>
#endif

CCL_NAMESPACE_BEGIN



/* Trace a photon through specular surfaces and store on first diffuse hit.
 * Restricted to caustic paths: light -> (specular)+ -> diffuse (L S+ D).
 * Returns true if photon stored, false if escaped or terminated. */
ccl_device bool photon_trace_and_store(KernelGlobals kg,
                                       ccl_private PhotonPath *photon,
                                       ccl_global PhotonRecord *photons,
                                       ccl_global uint *stored_count,
                                       const uint max_photons,
                                       const int max_specular_bounces,
                                       const float clamp_flux)
{
  int last_object = OBJECT_NONE;
  int last_prim = PRIM_NONE;

  while (photon->bounce < max_specular_bounces) {
    Ray ray ccl_optional_struct_init;
    ray.P = photon->ray_P;
    ray.D = photon->ray_D;
    ray.tmin = 0.0f;
    ray.tmax = FLT_MAX;
    ray.time = 0.5f;
#ifdef __RAY_DIFFERENTIALS__
    ray.dP = 0.0f;
    ray.dD = 0.0f;
#endif
    ray.self.object = last_object;
    ray.self.prim = last_prim;
    ray.self.light_object = OBJECT_NONE;
    ray.self.light_prim = PRIM_NONE;

    Intersection isect ccl_optional_struct_init;
    isect.object = OBJECT_NONE;
    isect.prim = PRIM_NONE;

    const bool hit = scene_intersect(kg, &ray, PATH_RAY_VISIBILITY_ALL, &isect);
    if (!hit) {
      /* Escaped scene into environment. */
      return false;
    }

    ShaderDataCausticsStorage sd_storage;
    ccl_private ShaderData *sd = AS_SHADER_DATA(&sd_storage);
    shader_setup_from_ray(kg, sd, &ray, &isect);

    /* Evaluate shader closures.
     * Uses CAUSTICS_MAX_CLOSURE to keep storage and evaluation lean. */
    surface_shader_eval<KERNEL_FEATURE_NODE_MASK_SURFACE_SHADOW>(
        kg, photon, sd, nullptr, PATH_RAY_VISIBILITY_CAMERA, PATH_RAY_FLAG_NONE, true);

    if (sd->num_closure == 0) {
      return false;
    }

    /* Check whether hit surface has diffuse or specular closures. */
    bool has_diffuse = false;
    bool has_specular = false;

    for (int i = 0; i < sd->num_closure; i++) {
      const ccl_private ShaderClosure *sc = &sd->closure[i];
      if (CLOSURE_IS_BSDF(sc->type)) {
        if (CLOSURE_IS_BSDF_SINGULAR(sc->type) ||
            (CLOSURE_IS_GLASS(sc->type) &&
             bsdf_get_specular_roughness_squared(sc) < 0.05f) ||
            (CLOSURE_IS_BSDF_TRANSMISSION(sc->type) &&
             bsdf_get_specular_roughness_squared(sc) < 0.05f) ||
            (CLOSURE_IS_BSDF_GLOSSY(sc->type) &&
             bsdf_get_specular_roughness_squared(sc) < 0.05f))
        {
          has_specular = true;
        }
        else if (CLOSURE_IS_BSDF_DIFFUSE(sc->type) ||
                 bsdf_get_specular_roughness_squared(sc) >= 0.05f)
        {
          has_diffuse = true;
        }
      }
    }

    /* First diffuse hit after at least one specular bounce (L S+ D): store caustic photon! */
    if (has_diffuse && photon->specular_bounces >= 1) {
      float3 flux = spectrum_to_rgb(photon->throughput);

      /* Flux clamping to eliminate high-variance fireflies. */
      if (clamp_flux > 0.0f) {
        const float m = reduce_max(flux);
        if (m > clamp_flux) {
          flux *= (clamp_flux / m);
        }
      }

      photon_store(photons, stored_count, max_photons, sd->P, -ray.D, flux);
      return true;
    }

    /* Direct diffuse hit without specular bounces (L D): discard to avoid double counting. */
    if (has_diffuse && photon->specular_bounces == 0 && !has_specular) {
      return false;
    }

    /* Specular bounce: sample closure and continue path. */
    if (!has_specular) {
      return false;
    }

    float3 rand_bsdf = photon_rng_3D(photon);
    const ccl_private ShaderClosure *sc = surface_shader_bsdf_bssrdf_pick(sd, &rand_bsdf);
    if (!sc) {
      return false;
    }

    Spectrum bsdf_eval;
    float3 bsdf_wo;
    float bsdf_pdf;
    float2 bsdf_roughness;
    float bsdf_eta;
    const int label = bsdf_sample(
        kg, sd, sc, rand_bsdf, &bsdf_eval, &bsdf_wo, &bsdf_pdf, &bsdf_roughness, &bsdf_eta);

    if (bsdf_pdf <= 0.0f || isequal(bsdf_eval, zero_spectrum())) {
      return false;
    }

    /* Update throughput with adjoint scaling for light transport across refractions. */
    Spectrum weight = bsdf_eval / bsdf_pdf;
    if (label & LABEL_TRANSMIT) {
      weight *= sqr(bsdf_eta);
    }
    photon->throughput *= weight;

    photon->bounce++;
    photon->specular_bounces++;

    /* Russian roulette after bounce 4 to terminate deep specular bounces unbiasedly. */
    if (photon->bounce >= 4) {
      const float p_survive = 0.8f;
      if (photon_rng_1D(photon) > p_survive) {
        return false;
      }
      photon->throughput /= p_survive;
    }

    /* Offset ray origin along shading/geometric normal to avoid self-intersection. */
    const float3 D = normalize(bsdf_wo);
    const float3 N = (dot(D, sd->Ng) > 0.0f) ? sd->Ng : -sd->Ng;
    photon->ray_P = ray_offset(sd->P, N);
    photon->ray_D = D;
    last_object = isect.object;
    last_prim = isect.prim;
  }

  return false;
}

CCL_NAMESPACE_END
