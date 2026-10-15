# Vexen Cycles Extreme ⚡

[![License: GPL v2+](https://img.shields.io/badge/License-GPL%20v2%2B-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![Blender Version](https://img.shields.io/badge/Blender-5.4.0--Extreme-orange.svg)](https://www.blender.org)
[![CUDA](https://img.shields.io/badge/CUDA-12.0%2B%20%7C%20sm__86-green.svg)](https://developer.nvidia.com/cuda-toolkit)
[![OptiX](https://img.shields.io/badge/NVIDIA-OptiX%20Supported-76B900.svg)](https://developer.nvidia.com/optix)
[![Architecture](https://img.shields.io/badge/Architecture-Wavefront%20GPU%20SPPM-purple.svg)](docs/vexen/architecture.md)

**Vexen Cycles Extreme** is a high-performance, experimental Blender development fork dedicated to solving the long-standing photon caustics challenge in the **Cycles** renderer.

By integrating a dedicated, GPU-accelerated **Progressive Photon Mapping (PPM)** wavefront pipeline, Vexen delivers smooth, firefly-free, physically accurate reflective and refractive caustics at over **10 million photons/second on consumer laptop hardware**—achieving parity with top commercial renderers like Chaos Corona and Chaos V-Ray GPU.

---

## 🌟 Why Vexen Cycles Extreme?

Native Blender Cycles relies on **MNEE (Manifold Next Event Estimation)**, which suffers from severe production limitations: it is hardcoded for refractive shadows only (zero reflective caustics), fails on nested or multi-interface glass (ice in drinks, complex folds), requires tedious manual tagging of lights, casters, and receivers, and produces extreme fireflies.

Vexen Cycles Extreme completely redesigns the caustics pipeline from the ground up:

| Capability | Native Cycles (MNEE) | Chaos Corona ("One-Click") | **Vexen Cycles Extreme** |
| :--- | :--- | :--- | :--- |
| **Reflective Caustics** *(Metals, mirrors, jewelry)* | ❌ **Unsupported** | ✅ Supported | **✅ Fully Supported** *(Specular & metallic)* |
| **Nested / Multi-Interface Glass** *(Ice in drinks)* | ❌ **Fails** *(Diverges on >1 interface)* | ✅ Handled | **✅ Fully Supported** *(Multi-bounce & TIR)* |
| **Total Internal Reflection (TIR)** | ❌ Dark voids / fireflies | ✅ Handled | **✅ Fully Supported** *(Exact Fresnel physics)* |
| **Emission Speed & Hardware** | CPU/GPU path walk | CPU Multi-core | **CUDA GPU: >10.1M photons/sec on laptop GPU** |
| **Caster Guiding ("Caustic in a Stadium")** | Manifold walk | Metropolis Light Transport | **Automated Bounding Sphere Cone Guiding** |
| **Spatial Acceleration Structure** | N/A | KD-Tree / Hash grid | **GPU Spatial Hash Grid + Parallel Morton / Prefix Sum** |
| **Reconstruction Kernel** | Point estimation | Spline / Epanechnikov | **2D Normalized Epanechnikov ($O(1)$ query)** |
| **Firefly Immunity** | Clamping (loses energy) | Adaptive clamping | **Dynamic Flux Clamping (`photon_caustics_clamp`)** |
| **Artist Usability** | ❌ 3 manual flags | ✅ "One-Click" automatic | **✅ Automatic shader heuristic detection** |

---

## 🚀 Key Features

* **⚡ Ultra-Fast GPU Wavefront Pipeline:** Emits, traces, and indexes over **10,137,000 photons/second** on an entry-level laptop GPU (NVIDIA RTX 3050 Laptop, 4GB VRAM). A complete 20-million-photon pass finishes in under **1.97 seconds**.
* **✨ Zero Fireflies & Smooth Caustic Ribbons:** Uses a normalized **2D Epanechnikov quadratic kernel** ($K(d) = \frac{2}{\pi R^2}(1 - d^2/R^2)$) evaluated on receiver tangent planes, combined with dynamic flux clamping.
* **🎯 Automated Caster Guiding:** Eliminates wasted rays via **Bounding Sphere Cone Guiding** ($4\pi \to 2\pi(1-\cos\theta)$) with zero-cost conjugate precision ($\frac{\sin^2(\theta/2)}{1+\cos(\theta/2)}$), solving the classic *"Caustic in a Stadium"* problem.
* **🧊 Multi-Bounce Dielectric & Metallic Walk:** Supports complex multi-interface refractions, total internal reflection (TIR), and metallic glossy reflections (tested on Suzanne with intricate concave ear folds).
* **🔒 Rock-Solid 16-Byte Memory Alignment:** Strictly enforces 16-byte packed alignment (`CasterBound`, `PhotonRecord`) across Host (MSVC) and Device (CUDA/OptiX), preventing memory corruption and zero-flux bugs.
* **🛡️ Zero Overhead When Off:** Compiles out entirely via `KERNEL_FEATURE_PHOTON_CAUSTICS`. When disabled, render times, memory, and kernel registers are bit-identical to upstream Cycles.

---

## 🎨 Quick Start Guide

### 1. Enable in Blender UI
1. Open the **Render Properties** tab in Blender.
2. Expand the **Vexen Extreme** panel (located next to *Light Paths*).
3. Check **Vexen Extreme Mode** (`use_vexen_extreme`).
4. Check **Photon Caustics** (`vexen_photon_caustics`).

![Vexen Cycles Extreme UI](docs/vexen/ui_preview.png)

### 2. Render Your Scene
Any object using a **Glass BSDF**, **Principled BSDF** with transmission, or pure specular/metallic reflection will automatically cast caustics onto diffuse/glossy receivers without any manual light or receiver tagging!

---

## 🛠️ Building from Source

### Prerequisites
* **Windows:** Visual Studio 2022 / 2026 (MSVC v143+), CMake 3.24+, CUDA Toolkit 12.0+
* **Linux:** GCC 11+ / Clang 14+, CMake 3.24+, Ninja, CUDA Toolkit 12.0+

### Windows Build
```powershell
# Clone the repository
git clone https://github.com/HyperVexen/BlenderCyclesExtreme.git
cd BlenderCyclesExtreme

# Configure and build with Release settings
make.bat release
```

### Linux Build
```bash
git clone https://github.com/HyperVexen/BlenderCyclesExtreme.git
cd BlenderCyclesExtreme
make release
```

---

## 🧪 Benchmark & Verification Suite

Vexen includes automated benchmarking scripts in `tools/vexen/`:

```powershell
# Run the automated GPU photon pipeline test
& ./bin/Release/blender.exe --background --python tools/vexen/test_gpu_caustics.py

# Render the high-contrast aesthetic reference scene
& ./bin/Release/blender.exe --background --python tools/vexen/render_reference_aesthetic.py
```

---

## 🗺️ Roadmap & Milestones

* [x] **M0: Baseline & Benchmark Harness:** Reference scenes and automated error curves.
* [x] **M1: Plumbing & UI:** Seamless UI integration and zero-overhead feature flags.
* [x] **M2: CPU Photon Emission & Specular Tracing:** Multi-light emission, adjoint BSDF, Russian roulette.
* [x] **M3: Hash Grid & Gather on CPU:** Uniform spatial hash grid and 8-cell gathering.
* [x] **M4: Double-Counting Suppression & Correctness:** Clean MNEE bypass and unbiased energy conservation.
* [x] **M5: Automated Caster Targeting:** Bounding cone guiding with conjugate precision.
* [x] **M6: GPU CUDA Acceleration (`sm_86`):** Wavefront kernels, 16B alignment, 10.14M photons/sec.
* [ ] **M7: Progressive SPPM Adaptive Radius & Spectral Dispersion:** Auto-sharpening caustics and rainbow prism dispersion.
* [ ] **M8: Hardware RT Core OptiX Acceleration:** 50M–100M+ photons/sec via `optixTrace`.
* [ ] **M9: Multiple Importance Sampling (MIS):** Balanced transitions for glossy/semi-rough receivers.
* [ ] **M10: Volumetric Caustic Beams (UPBP):** Underwater god rays and smoky light shafts.
* [ ] **M11: Interactive Viewport IPR & Temporal Animation Cache:** Real-time viewport feedback and flythrough caching.
* [ ] **M12: Multi-Backend & Production Polish:** Apple Metal, AMD HIP, and AI denoiser integration.

For full mathematical derivations and design details, see the [Photon Caustics Master Plan](docs/vexen/Vexen%20Cycles%20Extreme%20Photon%20Caustics%20Master%20Plan.md) and [Architecture Specification](docs/vexen/architecture.md).

---

## 🤝 Contributing

We welcome contributions! Please read our [Contributing Guidelines](CONTRIBUTING.md) and [Code of Conduct](CODE_OF_CONDUCT.md) before submitting pull requests.

---

## 📄 License & Acknowledgments

* **Blender & Cycles:** Licensed under the [GNU General Public License v2 or later (GPL-2.0-or-later)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
* **Research Heritage:** Vexen Cycles Extreme builds upon pioneering computer graphics research:
  * Henrik Wann Jensen (*Photon Mapping*)
  * Martin Šik & Jaroslav Křivánek (*Implementing One-Click Caustics in Corona Renderer, EGSR 2019*)
  * Iliyan Georgiev & Toshiya Hachisuka (*Vertex Connection and Merging, 2012*)
  * Johannes Hanika et al. (*Manifold Next Event Estimation, 2015*)
