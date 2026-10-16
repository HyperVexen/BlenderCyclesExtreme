/* SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "device/device.h"
#include "device/memory.h"
#include "kernel/integrator/photon_types.h"

CCL_NAMESPACE_BEGIN

template<> struct device_type_traits<PhotonRecord> {
  static constexpr DataType data_type = TYPE_FLOAT;
  static constexpr size_t num_elements = 8;
};

template<> struct device_type_traits<CasterBound> {
  static constexpr DataType data_type = TYPE_FLOAT;
  static constexpr size_t num_elements = 4;
};

class Device;
class Scene;

/* PhotonMap manages photon storage memory and SPPM radius reduction across iterations. */
class PhotonMap {
 public:
  PhotonMap();
  ~PhotonMap();

  /* Reset photon map for a fresh render. */
  void reset(const float initial_radius, const float alpha_val, const uint max_photons_budget);

  /* Advance to next SPPM iteration, shrinking gather radius.
   * r^2_{i+1} = r^2_i * (i + alpha) / (i + 1) */
  void next_iteration();

  /* Allocate photon storage buffer on host/device. */
  void allocate(Device *device, const size_t count);

  /* Free allocated device memory. */
  void device_free(Device *device);

  /* Clear stored photon count for next iteration pass. */
  void clear();

  /* Build spatial hash grid over stored photons and upload to scene->dscene. */
  bool build_grid(Scene *scene, Device *device);

  /* Accessors. */
  size_t get_stored_count() const;
  void set_stored_count(const size_t count);

  size_t get_emitted_count() const;
  void set_emitted_count(const size_t count);

  float get_current_radius() const;
  void set_current_radius(const float r);
  int get_iteration() const;

  PhotonRecord *data();
  const PhotonRecord *data() const;

  device_vector<PhotonRecord> *get_photons();

  uint get_grid_size() const;
  float get_cell_size() const;
  float get_inv_cell_size() const;
  float3 get_bounds_min() const;
  float3 get_bounds_max() const;

 protected:
  unique_ptr<device_vector<PhotonRecord>> photons_;

  uint stored_count_ = 0;
  uint emitted_count_ = 0;
  int iteration_ = 0;

  float initial_radius_ = 0.1f;
  float current_radius_ = 0.1f;
  float alpha_ = 0.666667f;
  uint max_photons_ = 2000000;

  uint grid_size_ = 0;
  float cell_size_ = 0.0f;
  float inv_cell_size_ = 0.0f;
  float3 bounds_min_ = make_float3(0.0f, 0.0f, 0.0f);
  float3 bounds_max_ = make_float3(0.0f, 0.0f, 0.0f);
};


CCL_NAMESPACE_END
