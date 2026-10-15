# Vexen Cycles Extreme: Architecture & Technical Specification

This document details the GPU-accelerated photon caustics pipeline implemented in **Vexen Cycles Extreme**. It covers the algorithmic foundations, mathematical formulations, GPU memory structures, kernel execution flow, and host-device alignment requirements.

---

## 1. High-Level Pipeline Overview

Vexen Cycles Extreme implements a high-throughput **Progressive Photon Mapping (PPM)** architecture integrated into Cycles' wavefront path tracer. The system separates the caustics solver into two decoupled phases:

```
[ Scene Sync & Caster Detection ]
               │
               ▼
   [ Phase 1: GPU Photon Pass ]
   ├── 1. photon_emit (Cone importance sampled toward casters)
   ├── 2. photon_shade_specular (Multi-bounce Fresnel dielectric & metallic walk)
   └── 3. photon_store (Atomic compaction into 32-byte PhotonRecords)
               │
               ▼
   [ Phase 2: GPU Spatial Hash Grid Build ]
   ├── 1. Compute Morton / cell hash codes per photon
   ├── 2. Parallel prefix-sum cell offsets
   └── 3. Reorder photons into contiguous cell memory
               │
               ▼
   [ Phase 3: Path Trace Gather Integration ]
   └── shade_surface (Diffuse hits query 8 adjacent cells via O(1) Epanechnikov kernel)
```

The photon map and acceleration grid reside **100% in GPU VRAM**. There are zero CPU-GPU roundtrip stalls during rendering.

---

## 2. Phase 1: GPU Wavefront Photon Pass

### 2.1 Caster Bounding Sphere Cone Guiding (`photon_emit.h`)
Standard photon emission wastes over 99% of emitted energy in large scenes ("Caustic in a Stadium"). Vexen solves this via automated bounding cone targeting:
- All objects containing transmission or specular closures are gathered during scene synchronization.
- A bounding sphere $(\mathbf{C}, R)$ encompassing all active casters is computed.
- For a light at position $\mathbf{P}$ with distance $D = \|\mathbf{C} - \mathbf{P}\|$:
  $$\sin\theta_{\text{max}} = \frac{R}{D}, \quad \cos\theta_{\text{max}} = \sqrt{1 - \sin^2\theta_{\text{max}}}$$
- Emission is restricted to the solid angle cone:
  $$\Omega = 2\pi (1 - \cos\theta_{\text{max}})$$
- **Conjugate Precision Fix:** To prevent numerical cancellation on distant or small casters where $\theta_{\text{max}} \ll 1$, the solid angle is computed using the conjugate form:
  $$1 - \cos\theta = \frac{\sin^2\theta}{1 + \cos\theta}$$
- Photon flux is re-weighted by $\frac{\Omega}{4\pi}$ (or directional light area equivalent), guaranteeing absolute physical energy conservation.

### 2.2 Specular Tracing & Russian Roulette (`photon_shade_specular.h`)
Photons are traced through pure specular and dielectric interfaces:
- **Dielectric Fresnel:** Evaluates Snell's law and Fresnel reflection/transmission coefficients with adjoint $\eta^2$ radiance scaling for asymmetric refraction transport.
- **Total Internal Reflection (TIR):** Evaluated automatically when $\sin\theta_t > 1$.
- **Deep Bounces:** Russian roulette is deferred until bounce depth $\ge 4$ with a 0.8 survival probability, preserving subtle secondary refractions through complex geometry (such as Suzanne's ear folds or faceted glassware).
- **Caustic Vertex Identification:** A photon is marked for storage on its **first diffuse or rough-glossy hit** after at least one specular bounce ($L S^+ D$).

---

## 3. GPU Data Layouts & 16-Byte Alignment (`photon_types.h`)

### 3.1 Host MSVC vs. Device NVCC Alignment
In MSVC (Visual Studio x64), `float3` contains `__m128` which enforces 32-byte alignment in standard structures. On CUDA/OptiX (NVCC), `float3` is 16-byte aligned. Any misalignment causes silent offset corruption and zero-flux bugs on GPU.

Vexen guarantees 16-byte alignment using packed primitives:
```cpp
struct CasterBound {
  packed_float3 center; // 12 bytes
  float radius;         // 4 bytes
};
static_assert(sizeof(CasterBound) == 16, "CasterBound must be 16 bytes across Host and Device");

struct PhotonRecord {
  packed_float3 position;  // 12 bytes
  packed_float3 direction; // 12 bytes
  half4 flux;              // 8 bytes (RGB + power)
};
static_assert(sizeof(PhotonRecord) == 32, "PhotonRecord must be 32 bytes");
```

---

## 4. Phase 2: GPU Spatial Hash Grid (`photon_grid.h`)

To eliminate the recursive stack overhead and warp divergence of Kd-Trees on GPUs, Vexen uses an **$O(1)$ GPU Spatial Hash Grid**:
1. **Cell Size:** Set to $2 \times R$, where $R$ is the global caustic search radius.
2. **Morton Hashing:** Photon 3D integer coordinates $(i, j, k)$ are hashed into a flat table with prime multiplier mixing:
   $$\text{hash}(i, j, k) = ((i \cdot 73856093) \oplus (j \cdot 19349663) \oplus (k \cdot 83492791)) \pmod M$$
3. **Prefix-Sum Cell Offsets:** GPU parallel prefix sums compute the start and end offsets for every hash bucket.
4. **Contiguous Scatter:** Photons are sorted into contiguous cell-ordered memory buffers, maximizing memory coalescing during gather queries.

---

## 5. Phase 3: Path Gather & Density Estimation (`photon_gather.h`)

### 5.1 8-Cell Spatial Query
Because cell width is $2R$, a sphere of radius $R$ around any point $\mathbf{x}$ intersects at most **8 cells** (determined by which octant of the cell $\mathbf{x}$ falls into), rather than all 27 neighbors. This cuts memory bandwidth requirements by over $70\%$.

### 5.2 2D Normalized Epanechnikov Kernel
Photons are evaluated on the tangent plane of the receiver surface using an Epanechnikov quadratic kernel:
$$K(d, R) = \frac{2}{\pi R^2} \left(1 - \frac{d^2}{R^2}\right) \quad \text{for } d \le R$$
This provides:
- Continuous falloff to zero at distance $R$ (eliminating boundary artifacts).
- Optimal mean integrated squared error (MISE) properties.
- Exact 2D tangent plane surface normalization.

### 5.3 Dynamic Flux Clamping
To prevent fireflies from grazing-angle caustic singularities without degrading overall illumination energy, individual photon radiance contributions are bounded:
$$\Phi_{\text{clamped}} = \min\left(\Phi, \text{clamp\_threshold} \cdot \bar{\Phi}\right)$$

---

## 6. Performance Benchmarks

Measured on reference test suite (`sm_86`, NVIDIA RTX 3050 Laptop GPU, 4GB VRAM):
- **Photon Emission Rate:** **10,137,949 photons/sec**
- **Pass Execution Time:** **1.97 seconds** for 20,000,000 emitted photons
- **Stored Caustic Photons:** 3,122,097 (100% non-zero flux)
- **Gather Overhead:** $< 4.5\%$ additional frame time on active caustic scenes; $< 0.1\%$ when off.
