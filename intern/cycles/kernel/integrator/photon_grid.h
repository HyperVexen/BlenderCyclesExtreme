/* SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "util/math.h"

CCL_NAMESPACE_BEGIN

/* Spatial hash function for 3D grid cell coordinates.
 * Multiplies by 3 large primes and masks with (grid_size - 1). */
ccl_device_inline uint photon_grid_hash(int ix, int iy, int iz, uint grid_size)
{
  const uint p1 = 73856093u;
  const uint p2 = 19349663u;
  const uint p3 = 83492791u;
  return (uint)((ix * p1) ^ (iy * p2) ^ (iz * p3)) & (grid_size - 1u);
}

/* Compute integer cell coordinate for a world position P. */
ccl_device_inline int3 photon_grid_coord(float3 P, float3 bounds_min, float inv_cell_size)
{
  float3 rel = (P - bounds_min) * inv_cell_size;
  return make_int3((int)floorf(rel.x), (int)floorf(rel.y), (int)floorf(rel.z));
}

/* Compute base cell coordinate for 8-cell gather.
 * Since cell size w = 2*R, any query sphere of radius R covers at most 2 cells along each axis.
 * The 8 cells to query are (base.x + dx, base.y + dy, base.z + dz) for dx, dy, dz in {0, 1}. */
ccl_device_inline int3 photon_grid_gather_base(float3 P, float3 bounds_min, float inv_cell_size)
{
  float3 rel = (P - bounds_min) * inv_cell_size;
  return make_int3((int)floorf(rel.x - 0.5f),
                   (int)floorf(rel.y - 0.5f),
                   (int)floorf(rel.z - 0.5f));
}

CCL_NAMESPACE_END
