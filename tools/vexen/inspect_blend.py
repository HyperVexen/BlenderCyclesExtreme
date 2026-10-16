import bpy
c = bpy.context.scene.cycles
print(f"use_vexen_extreme: {c.use_vexen_extreme}")
print(f"vexen_photon_caustics: {c.vexen_photon_caustics}")
print(f"photon_caustics_count: {c.photon_caustics_count}")
print(f"photon_caustics_radius: {c.photon_caustics_radius}")
print(f"photon_caustics_alpha: {c.photon_caustics_alpha}")
print(f"photon_caustics_dispersion: {c.photon_caustics_dispersion}")
