import bpy
import time
import os

print("=== Setting up Vexen GPU Caustics Test ===")
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
scene.cycles.photon_caustics_radius = 0.015
scene.cycles.photon_caustics_clamp = 5.0
scene.cycles.samples = 64

output_path = "C:/tmp/test_gpu_m6.png"
scene.render.filepath = output_path
print(f"Rendering 20M photons to {output_path}...")

t0 = time.time()
bpy.ops.render.render(write_still=True)
t1 = time.time()

print(f"=== Render completed in {t1 - t0:.2f}s ===")
if os.path.exists(output_path):
    print(f"Output saved: {output_path} ({os.path.getsize(output_path)} bytes)")
else:
    print("Output file not found!")
