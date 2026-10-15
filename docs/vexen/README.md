# Vexen Cycles Extreme

## Project scope

Vexen Cycles Extreme is an experimental Blender fork focused on improving the
Cycles renderer over time. It is deliberately narrow in the present: the only
feature effort currently in scope is **photon caustics**.

Photon caustics are the focused-light patterns produced by specular paths, such
as light refracted through glass onto a diffuse surface. The intended work is a
performance-conscious implementation that improves convergence for these
cases, while preserving the existing Cycles path tracer for all other lighting.

## Current priorities

- Add photon caustics to Cycles.
- Keep the feature disabled by default and retain unchanged behavior when it is
  disabled.
- Validate physical correctness, image quality, memory use, and render-time
  impact on CPU and GPU.
- Keep the implementation isolated enough to follow upstream Cycles changes.

## Not current scope

- A general rewrite or replacement of Cycles.
- Changes to Eevee, modeling, animation, or other unrelated Blender systems.
- Promising unimplemented renderer improvements as available features.

Future Cycles improvements may be considered after photon caustics are
implemented, measured, and documented. Until then, references to “Vexen Cycles
Extreme” mean this photon-caustics effort specifically.

## Documentation

- [Root README](../../README.md): project entry point and upstream Blender links.
- [Cycles contributor note](../../intern/cycles/README.md): scope at the
  renderer source level.
- [Photon Caustics Master Plan](Vexen%20Cycles%20Extreme%20Photon%20Caustics%20Master%20Plan.md): comprehensive architecture, verified milestones, and the roadmap to surpass commercial renderers.
