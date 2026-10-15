import bpy
import time
import os

print("=== Setting up Vexen Reference Aesthetic Render ===")
scene = bpy.context.scene

# Configure Cycles GPU with CUDA
scene.cycles.device = 'GPU'
prefs = bpy.context.preferences.addons['cycles'].preferences
prefs.compute_device_type = 'CUDA'
prefs.get_devices()

for d in prefs.devices:
    if d.type == 'CUDA':
        d.use = True
        print(f"  -> Enabled CUDA device: {d.name}")
    else:
        d.use = False

# Enable Vexen Extreme and Photon Caustics
scene.cycles.use_vexen_extreme = True
scene.cycles.vexen_photon_caustics = True
scene.cycles.photon_caustics_count = 20000000
scene.cycles.photon_caustics_max_photons = 10000000
scene.cycles.photon_caustics_radius = 0.012
scene.cycles.photon_caustics_clamp = 10.0
scene.cycles.samples = 64

# Adjust Floor Material to dark floor like in reference picture
mat_floor = bpy.data.materials.get('Studio_Matte_White')
if mat_floor and mat_floor.node_tree:
    for n in mat_floor.node_tree.nodes:
        if n.type == 'BSDF_PRINCIPLED':
            # Dark warm slate gray / asphalt like reference picture
            n.inputs['Base Color'].default_value = (0.12, 0.11, 0.10, 1.0)
            n.inputs['Roughness'].default_value = 0.6

# Adjust Key Light to warm gold / amber like reference picture
key_light = bpy.data.lights.get('Key_Light')
if key_light:
    key_light.color = (1.0, 0.88, 0.65)
    key_light.energy = 4500.0

fill_light = bpy.data.lights.get('Fill_Light')
if fill_light:
    fill_light.energy = 100.0

# Slightly dim world background for dramatic high-contrast caustics
if bpy.data.worlds:
    w = bpy.data.worlds[0]
    if w.node_tree:
        for n in w.node_tree.nodes:
            if n.type == 'BACKGROUND':
                n.inputs['Strength'].default_value = 0.2

output_path = "C:/tmp/reference_aesthetic.png"
scene.render.filepath = output_path
print(f"Rendering 20M photons with reference aesthetics to {output_path}...")

t0 = time.time()
bpy.ops.render.render(write_still=True)
t1 = time.time()

print(f"=== Render completed in {t1 - t0:.2f}s ===")
if os.path.exists(output_path):
    print(f"Output saved: {output_path} ({os.path.getsize(output_path)} bytes)")
else:
    print("Output file not found!")
