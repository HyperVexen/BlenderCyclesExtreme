# Contributing to Vexen Cycles Extreme

Thank you for your interest in contributing to **Vexen Cycles Extreme**! This project is an experimental, performance-first Blender fork dedicated to bringing production-grade photon caustics to the Cycles renderer.

---

## 1. Ground Rules & The 6 Performance Principles

Every contribution, pull request, and architectural decision must adhere to our six foundational performance rules:

1. **Zero Cost When Off:** When `Vexen Extreme Mode` or `Photon Caustics` is disabled, render time, memory footprint, binary size, and GPU kernel register usage must be bit-identical to upstream Cycles. New code in hot path-tracing paths must be compiled out via `#ifdef KERNEL_FEATURE_PHOTON_CAUSTICS`.
2. **Near-Zero Cost When Idle:** If a scene has no specular casters or produces zero caustic photons, all spatial grid traversals and density gatherings must be bypassed immediately.
3. **Time-to-Noise is King:** Optimizations are measured by the time required to reach a target RMSE against reference renders, not by raw photon emission count alone.
4. **Wavefront & GPU Coherent:** Never introduce recursive stack-based tree traversals (e.g., Kd-Trees) or pointer chasing on GPU paths. Everything must use wavefront queues, flat arrays, and $O(1)$ spatial hashing.
5. **Deterministic Memory Budget:** Hard memory caps are mandatory. Gracefully drop photon counts or reduce iterations rather than causing out-of-memory (OOM) crashes on consumer hardware.
6. **Measure Before and After:** Every change must pass benchmark verification against reference scenes before being merged.

---

## 2. Development Setup

### Windows
- **Compiler:** Microsoft Visual Studio 2022 (MSVC v143 or later).
- **GPU Toolkit:** NVIDIA CUDA Toolkit 12.0+ (Compute Capability `sm_86` or higher).
- **CMake:** Version 3.24+.
- **Python:** 3.10 or 3.11 with `numpy`.

### Linux
- **Compiler:** GCC 11+ or Clang 14+.
- **GPU Toolkit:** CUDA 12.0+ / OptiX SDK 7.5+.
- **Build System:** Ninja and CMake.

---

## 3. Coding Standards & Hardware Constraints

### 3.1 Host vs. Device Memory Alignment (Critical!)
MSVC x64 packs `float3` with `__m128` (32-byte alignment), whereas CUDA / OptiX (NVCC) uses 16-byte alignment. To prevent memory offset corruption between host and device:
- **Always** use `packed_float3` instead of raw `float3` in shared structs.
- **Always** add compile-time size assertions:
  ```cpp
  struct MyDeviceRecord {
    packed_float3 position;
    float scalar;
  };
  static_assert(sizeof(MyDeviceRecord) == 16, "Must be 16 bytes across all platforms");
  ```

### 3.2 Upstream Isolation
To make rebasing against upstream Blender seamless:
- Keep all Vexen-specific logic isolated in dedicated files:
  - `intern/cycles/kernel/integrator/photon_*.h`
  - `intern/cycles/integrator/photon_pass.*`
  - `intern/cycles/scene/photon_map.*`
- Minimize edits to upstream core files (`shade_surface.h`, `integrator.cpp`). Any hook into core Cycles must be guarded with `#ifdef KERNEL_FEATURE_PHOTON_CAUSTICS`.

---

## 4. Benchmark Verification & Regression Policy

Before submitting a Pull Request, run the validation harness:
```powershell
# Run GPU caustics benchmark test
& <path-to-blender.exe> --background --python tools/vexen/test_gpu_caustics.py

# Render reference aesthetic verification
& <path-to-blender.exe> --background --python tools/vexen/render_reference_aesthetic.py
```

### Regression Policy:
- Any change that slows down the **off-state** by more than **0.5%** will be rejected.
- Any change that increases kernel register pressure or decreases GPU occupancy without an indisputable quality improvement will be reworked.

---

## 5. Pull Request Process

1. Fork the repository and create your branch from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```
2. Commit with clear, descriptive commit messages adhering to standard conventions:
   ```
   Cycles: Vexen: Implement SPPM adaptive radius contraction in gather kernel
   ```
3. Include before-and-after render comparisons, timing logs, and memory stats in your PR description.
4. Ensure all files pass code formatting (`clang-format`).

---

## 6. Code of Conduct

All contributors are expected to uphold our [Code of Conduct](CODE_OF_CONDUCT.md). Please report unacceptable behavior to the project maintainers.
