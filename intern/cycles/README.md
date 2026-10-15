# Cycles in Vexen Cycles Extreme

This directory contains Blender's Cycles renderer. In the Vexen Cycles Extreme
fork, the broader aim is to improve Cycles; the current and only active feature
scope is photon caustics.

Treat this as an additive, opt-in renderer experiment. Work on photon caustics
must preserve existing Cycles behavior and performance when the feature is
disabled. It is not a mandate to rewrite unrelated integrator, shading, device,
or Blender integration code.

Project-level scope and boundaries are documented in
[`docs/vexen/README.md`](../../docs/vexen/README.md).
