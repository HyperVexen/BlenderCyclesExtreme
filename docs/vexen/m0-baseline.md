# M0: Baseline and benchmark harness

M0 establishes reproducible measurements before any photon-caustics kernel or
UI changes are made. It measures current upstream Cycles behavior only.

## Scene suite

`tools/vexen/generate_m0_scenes.py` creates these files in
`tests/benchmarks/cycles/vexen/`:

- `glass_sphere_plane`: the primary refractive-caustic correctness scene.
- `glass_ring_cup`: curved refractive casters with a more complex footprint.
- `pool_floor`: a broad shallow refractive caster.
- `prism_spotlight`: a narrow-light, hard-caustic case.
- `no_caster_regression`: a moderately heavy all-diffuse scene; future
  photon-caustics code must have no measurable benefit or material cost here.
- `many_small_casters`: a dense-caster stress scene for targeting and grid work.

The scenes are generated rather than committed as binary files, making their
parameters reviewable and the suite reproducible on the current Blender build.

## Generate and run

After a successful Blender build, generate the suite from the repository root:

```powershell
& <path-to-blender.exe> --background --factory-startup --python tools/vexen/generate_m0_scenes.py -- --output-dir tests/benchmarks/cycles/vexen
```

Use Blender's existing performance harness to initialize and run the CPU suite:

```powershell
& <python.exe> tests/performance/benchmark.py init
& <python.exe> tests/performance/benchmark.py run <configuration> cycles
```

The harness records render time and peak memory. Run every scene at least three
times per device, record median timings, and reject a baseline with more than
2% variance. Save high-sample EXR references outside the repository's source
tree, along with the commit ID, device driver, build configuration, and command
line used to produce them.

## M0 exit criteria

- The current source commit builds and runs on CPU.
- The six generated scenes render under current Cycles without renderer changes.
- CPU baseline measurements are repeatable within 2% variance.
- GPU baseline measurements are collected for every available supported backend.
- Reference renders and environment details are archived for later M4 and M6
  correctness comparisons.
