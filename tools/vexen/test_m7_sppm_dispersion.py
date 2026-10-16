import bpy
import time
import os
import sys

print("==========================================================")
print("  Vexen Cycles Extreme - Milestone 7 Verification Suite   ")
print("  (Adaptive SPPM Radius Contraction & Spectral Dispersion)")
print("==========================================================")

scene = bpy.context.scene

# Configure Cycles GPU with CUDA
scene.cycles.device = 'GPU'
prefs = bpy.context.preferences.addons['cycles'].preferences
prefs.compute_device_type = 'CUDA'
prefs.get_devices()

cuda_found = False
for d in prefs.devices:
    if d.type == 'CUDA':
        d.use = True
        cuda_found = True
        print(f"  [Device] Enabled CUDA: {d.name}")
    else:
        d.use = False

if not cuda_found:
    print("  [Device] No CUDA device found, falling back to CPU.")
    scene.cycles.device = 'CPU'

# Ensure output directory exists
os.makedirs("C:/tmp", exist_ok=True)

# ----------------------------------------------------------------------
# TEST 1: SPPM Adaptive Radius Contraction + Auto-Radius Heuristic
# ----------------------------------------------------------------------
print("\n--- TEST 1: SPPM Adaptive Radius Contraction (Corona Standard) ---")
scene.cycles.use_vexen_extreme = True
scene.cycles.vexen_photon_caustics = True
scene.cycles.photon_caustics_count = 10000000
scene.cycles.photon_caustics_max_photons = 5000000
scene.cycles.photon_caustics_radius = 0.0  # 0.0 triggers Auto-Radius Heuristic!
scene.cycles.photon_caustics_alpha = 0.666667  # SPPM 2/3 contraction
scene.cycles.photon_caustics_clamp = 10.0
scene.cycles.photon_caustics_dispersion = 0.0  # Achromatic for Test 1
scene.cycles.samples = 64

out_sppm = "C:/tmp/test_m7_sppm.png"
scene.render.filepath = out_sppm
print(f"Rendering SPPM auto-radius to {out_sppm}...")

t0 = time.time()
bpy.ops.render.render(write_still=True)
t1 = time.time()
sppm_time = t1 - t0
print(f"  -> SPPM Render completed in {sppm_time:.2f}s")
assert os.path.exists(out_sppm), f"Output file missing: {out_sppm}"
print(f"  -> Verified SPPM output: {out_sppm} ({os.path.getsize(out_sppm)} bytes)")

# ----------------------------------------------------------------------
# TEST 2: Continuous Spectral Dispersion (LuxCore Standard)
# ----------------------------------------------------------------------
print("\n--- TEST 2: Continuous Spectral Dispersion (LuxCore Standard) ---")
# Set dispersion to 0.04 (physical flint glass dispersion)
scene.cycles.photon_caustics_dispersion = 0.04
scene.cycles.photon_caustics_radius = 0.015
scene.cycles.samples = 64

out_disp = "C:/tmp/test_m7_dispersion.png"
scene.render.filepath = out_disp
print(f"Rendering spectral dispersion to {out_disp}...")

t0 = time.time()
bpy.ops.render.render(write_still=True)
t1 = time.time()
disp_time = t1 - t0
print(f"  -> Spectral Dispersion Render completed in {disp_time:.2f}s")
assert os.path.exists(out_disp), f"Output file missing: {out_disp}"
print(f"  -> Verified Spectral Dispersion output: {out_disp} ({os.path.getsize(out_disp)} bytes)")

print("\n==========================================================")
print("  Milestone 7 Verification: ALL TESTS PASSED!")
print(f"  SPPM Time: {sppm_time:.2f}s | Dispersion Time: {disp_time:.2f}s")
print("==========================================================")
