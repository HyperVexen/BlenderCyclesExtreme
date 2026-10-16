/* SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/sample/lcg.h"
#include "util/types.h"
#include "util/types_normal.h"

CCL_NAMESPACE_BEGIN

/* Stored photon record in the photon map (32 bytes) is defined in kernel/types.h
 * to allow its use in data_arrays.h without circular dependencies. */

/* State for a photon path being traced from a light.
 * Kept separate from IntegratorState to avoid polluting main path tracing state. */
struct PhotonPath {
  float3 ray_P;
  float3 ray_D;
  Spectrum throughput;
  uint rng_state;
  int bounce;
  int specular_bounces;
  float wavelength;
};

/* Bounding sphere for a specular caustic caster (Milestone 5). */
struct ccl_align(16) CasterBound {
  packed_float3 center;
  float radius;
};
static_assert_align(CasterBound, 16);
static_assert(sizeof(CasterBound) == 16, "CasterBound must be exactly 16 bytes");

/* Fast LCG random number generation along photon path */
ccl_device_inline float photon_rng_1D(ccl_private PhotonPath *photon)
{
  return lcg_step_float(&photon->rng_state);
}

ccl_device_inline float2 photon_rng_2D(ccl_private PhotonPath *photon)
{
  const float r1 = lcg_step_float(&photon->rng_state);
  const float r2 = lcg_step_float(&photon->rng_state);
  return make_float2(r1, r2);
}

ccl_device_inline float3 photon_rng_3D(ccl_private PhotonPath *photon)
{
  return lcg_step_float3(&photon->rng_state);
}

/* Overload integrator_state_* for PhotonPath to allow surface_shader_eval to run cleanly. */
ccl_device_inline uint integrator_state_lcg_init(const ccl_private PhotonPath *photon,
                                                 const uint hash)
{
  return (photon != nullptr) ? (photon->rng_state ^ hash) : hash;
}
ccl_device_inline uint integrator_state_lcg_init(ccl_private PhotonPath *photon,
                                                 const uint hash)
{
  return integrator_state_lcg_init((const ccl_private PhotonPath *)photon, hash);
}

ccl_device_inline int integrator_state_bounce(const ccl_private PhotonPath *photon,
                                              const uint32_t /*path_flag*/)
{
  return (photon != nullptr) ? photon->bounce : 0;
}
ccl_device_inline int integrator_state_bounce(ccl_private PhotonPath *photon,
                                              const uint32_t path_flag)
{
  return integrator_state_bounce((const ccl_private PhotonPath *)photon, path_flag);
}

ccl_device_inline int integrator_state_transparent_bounce(const ccl_private PhotonPath * /*photon*/,
                                                          const uint32_t /*path_flag*/)
{
  return 0;
}
ccl_device_inline int integrator_state_transparent_bounce(ccl_private PhotonPath *photon,
                                                          const uint32_t path_flag)
{
  return integrator_state_transparent_bounce((const ccl_private PhotonPath *)photon, path_flag);
}

ccl_device_inline int integrator_state_diffuse_bounce(const ccl_private PhotonPath * /*photon*/,
                                                      const uint32_t /*path_flag*/)
{
  return 0;
}
ccl_device_inline int integrator_state_diffuse_bounce(ccl_private PhotonPath *photon,
                                                      const uint32_t path_flag)
{
  return integrator_state_diffuse_bounce((const ccl_private PhotonPath *)photon, path_flag);
}

ccl_device_inline int integrator_state_glossy_bounce(const ccl_private PhotonPath *photon,
                                                     const uint32_t /*path_flag*/)
{
  return (photon != nullptr) ? photon->specular_bounces : 0;
}
ccl_device_inline int integrator_state_glossy_bounce(ccl_private PhotonPath *photon,
                                                     const uint32_t path_flag)
{
  return integrator_state_glossy_bounce((const ccl_private PhotonPath *)photon, path_flag);
}

ccl_device_inline int integrator_state_transmission_bounce(const ccl_private PhotonPath *photon,
                                                           const uint32_t /*path_flag*/)
{
  return (photon != nullptr) ? photon->specular_bounces : 0;
}
ccl_device_inline int integrator_state_transmission_bounce(ccl_private PhotonPath *photon,
                                                           const uint32_t path_flag)
{
  return integrator_state_transmission_bounce((const ccl_private PhotonPath *)photon, path_flag);
}

ccl_device_inline int integrator_state_portal_bounce(KernelGlobals /*kg*/,
                                                     const ccl_private PhotonPath * /*photon*/,
                                                     const uint32_t /*path_flag*/)
{
  return 0;
}
ccl_device_inline int integrator_state_portal_bounce(KernelGlobals kg,
                                                     ccl_private PhotonPath *photon,
                                                     const uint32_t path_flag)
{
  return integrator_state_portal_bounce(kg, (const ccl_private PhotonPath *)photon, path_flag);
}

CCL_NAMESPACE_END
