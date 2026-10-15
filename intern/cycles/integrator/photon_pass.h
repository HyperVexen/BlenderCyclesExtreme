/* SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "device/device.h"
#include "kernel/integrator/photon_types.h"
#include "scene/photon_map.h"
#include "scene/scene.h"

CCL_NAMESPACE_BEGIN

/* Orchestrates photon emission and tracing passes for caustic generation. */
class PhotonPass {
 public:
  PhotonPass();
  ~PhotonPass();

  /* Execute CPU multithreaded photon pass.
   * Emits photons from scene lights, traces them through specular bounce chains,
   * and appends caustic photons to the photon map on first diffuse hits. */
  static bool execute_cpu(Scene *scene,
                          PhotonMap *photon_map,
                          Device *device,
                          const uint num_photons,
                          const int max_specular_bounces,
                          const float clamp_flux,
                          const uint seed);

  /* Execute GPU massively-parallel photon pass (Milestone 6).
   * Dispatches lock-free photon tracing on GPU device queue. */
  static bool execute_gpu(Scene *scene,
                          PhotonMap *photon_map,
                          Device *device,
                          const uint num_photons,
                          const int max_specular_bounces,
                          const float clamp_flux,
                          const uint seed);
};

CCL_NAMESPACE_END
