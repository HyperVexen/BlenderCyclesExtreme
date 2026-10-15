/* SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/integrator/photon_types.h"
#include "util/atomic.h"

CCL_NAMESPACE_BEGIN

/* Store a photon in the photon map buffer.
 * Thread-safe: uses atomic increment on the stored count.
 * Returns true if stored, false if buffer capacity exceeded. */
ccl_device_inline bool photon_store(ccl_global PhotonRecord *photons,
                                    ccl_global uint *stored_count,
                                    const uint max_photons,
                                    const float3 position,
                                    const float3 direction,
                                    const float3 flux)
{
  const uint index = atomic_fetch_and_inc_uint32(stored_count);
  if (index >= max_photons) {
    return false;
  }

  photons[index].set(position, direction, flux);
  return true;
}

CCL_NAMESPACE_END
