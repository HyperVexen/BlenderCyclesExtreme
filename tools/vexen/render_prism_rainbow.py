"""
Render a classic Newton prism scene demonstrating Milestone 7:
Continuous Spectral Dispersion (Rainbow Caustics) on dark neutral slate floor.
"""

import bpy
import bmesh
import math
import os
import sys

def setup_prism_scene(output_path, dispersion=0.08, num_photons=1000000, samples=48, radius=0.07):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene

    # 1. World background: deep dark studio
    scene.world = bpy.data.worlds.new("World")
    scene.world.use_nodes = True
    bg = scene.world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.005, 0.005, 0.008, 1.0)
        bg.inputs["Strength"].default_value = 0.2

    # 2. Floor: Dark neutral charcoal slate (makes rainbow caustics glow intensely)
    mesh_floor = bpy.data.meshes.new("FloorMesh")
    obj_floor = bpy.data.objects.new("Floor", mesh_floor)
    bpy.context.collection.objects.link(obj_floor)

    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=12.0)
    bm.to_mesh(mesh_floor)
    bm.free()
    obj_floor.location = (0, 0, 0)

    mat_floor = bpy.data.materials.new("FloorMat")
    mat_floor.use_nodes = True
    bsdf_floor = mat_floor.node_tree.nodes.get("Principled BSDF")
    if bsdf_floor:
        bsdf_floor.inputs["Base Color"].default_value = (0.12, 0.12, 0.14, 1.0)
        bsdf_floor.inputs["Roughness"].default_value = 0.4
        bsdf_floor.inputs["Specular IOR Level"].default_value = 0.05
    obj_floor.data.materials.append(mat_floor)
    obj_floor.cycles.is_caustics_receiver = True

    # 3. Equilateral Glass Prism Caster
    mesh_prism = bpy.data.meshes.new("PrismMesh")
    obj_prism = bpy.data.objects.new("Prism", mesh_prism)
    bpy.context.collection.objects.link(obj_prism)

    side = 0.7
    h_tri = side * math.sqrt(3) / 2.0
    z_len = 0.9

    bm_p = bmesh.new()
    v0 = bm_p.verts.new((-side / 2.0, -h_tri / 3.0, 0.0))
    v1 = bm_p.verts.new((side / 2.0, -h_tri / 3.0, 0.0))
    v2 = bm_p.verts.new((0.0, 2.0 * h_tri / 3.0, 0.0))

    v3 = bm_p.verts.new((-side / 2.0, -h_tri / 3.0, z_len))
    v4 = bm_p.verts.new((side / 2.0, -h_tri / 3.0, z_len))
    v5 = bm_p.verts.new((0.0, 2.0 * h_tri / 3.0, z_len))

    bm_p.faces.new((v2, v1, v0))
    bm_p.faces.new((v3, v4, v5))
    bm_p.faces.new((v0, v1, v4, v3))
    bm_p.faces.new((v1, v2, v5, v4))
    bm_p.faces.new((v2, v0, v3, v5))

    bmesh.ops.recalc_face_normals(bm_p, faces=bm_p.faces)
    bm_p.to_mesh(mesh_prism)
    bm_p.free()

    obj_prism.location = (0.0, 0.0, 0.01)
    obj_prism.rotation_euler = (0.0, 0.0, math.radians(22))

    mat_glass = bpy.data.materials.new("PrismGlass")
    mat_glass.use_nodes = True
    mat_glass.node_tree.nodes.clear()
    nodes = mat_glass.node_tree.nodes
    links = mat_glass.node_tree.links

    out_node = nodes.new("ShaderNodeOutputMaterial")
    out_node.location = (300, 0)
    glass_node = nodes.new("ShaderNodeBsdfGlass")
    glass_node.location = (0, 0)
    glass_node.inputs["Color"].default_value = (1.0, 1.0, 1.0, 1.0)
    glass_node.inputs["Roughness"].default_value = 0.001
    glass_node.inputs["IOR"].default_value = 1.52
    links.new(glass_node.outputs["BSDF"], out_node.inputs["Surface"])

    obj_prism.data.materials.append(mat_glass)
    obj_prism.cycles.is_caustics_caster = True

    # 4. Narrow White Spotlight Beam
    light_data = bpy.data.lights.new("PrismSpot", type='SPOT')
    light_data.energy = 3500.0
    light_data.color = (1.0, 1.0, 1.0)
    light_data.spot_size = math.radians(16.0)
    light_data.spot_blend = 0.12
    light_data.shadow_soft_size = 0.01

    light_obj = bpy.data.objects.new("PrismSpot", light_data)
    bpy.context.collection.objects.link(light_obj)
    light_obj.location = (-1.9, -0.8, 1.5)

    # Aim spot at prism center (0, 0, 0.45)
    dx = 0.0 - light_obj.location[0]
    dy = 0.0 - light_obj.location[1]
    dz = 0.45 - light_obj.location[2]
    pitch = math.atan2(math.sqrt(dx*dx + dy*dy), -dz)
    yaw = math.atan2(dy, dx)
    light_obj.rotation_euler = (pitch, 0, yaw - math.pi/2)

    # Subtle rim fill light to outline glass edge
    fill_data = bpy.data.lights.new("RimFill", type='POINT')
    fill_data.energy = 40.0
    fill_data.color = (0.6, 0.75, 1.0)
    fill_obj = bpy.data.objects.new("RimFill", fill_data)
    bpy.context.collection.objects.link(fill_obj)
    fill_obj.location = (2.2, 2.5, 2.0)

    # 5. Camera: Elegant wide view of prism and rainbow projection
    cam_data = bpy.data.cameras.new("Camera")
    cam_data.lens = 40.0
    cam_obj = bpy.data.objects.new("Camera", cam_data)
    bpy.context.collection.objects.link(cam_obj)
    scene.camera = cam_obj

    cam_obj.location = (1.9, -2.8, 2.2)
    # Pitch: 55 deg down, Yaw: 24 deg
    cam_obj.rotation_euler = (math.radians(55), 0, math.radians(24))

    # 6. Render Settings
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'GPU'
    scene.render.resolution_x = 960
    scene.render.resolution_y = 540
    scene.render.resolution_percentage = 100
    scene.cycles.samples = samples
    scene.cycles.preview_samples = 16

    # Enable Vexen Photon Caustics Extreme with Milestone 7 Continuous Spectral Dispersion
    scene.cycles.use_vexen_extreme = True
    scene.cycles.vexen_photon_caustics = True
    scene.cycles.photon_caustics_count = num_photons
    scene.cycles.photon_caustics_radius = radius
    scene.cycles.photon_caustics_alpha = 1.0
    scene.cycles.photon_caustics_dispersion = dispersion
    scene.cycles.photon_caustics_max_bounces = 8
    scene.cycles.photon_caustics_clamp = 50.0

    scene.render.filepath = output_path
    scene.render.image_settings.file_format = 'PNG'

    print("=" * 60)
    print(f"Prism Rainbow Scene Configured (Fast Demo):")
    print(f"  Dispersion: {dispersion}")
    print(f"  Photons: {num_photons}")
    print(f"  Radius: {scene.cycles.photon_caustics_radius}")
    print(f"  Alpha: {scene.cycles.photon_caustics_alpha}")
    print(f"  Samples: {samples}")
    print(f"  Resolution: {scene.render.resolution_x}x{scene.render.resolution_y}")
    print(f"  Output: {output_path}")
    print("=" * 60)

    bpy.ops.render.render(write_still=True)
    print(f"Successfully rendered prism scene to {output_path}")

if __name__ == "__main__":
    out_file = sys.argv[-1] if len(sys.argv) > 1 and sys.argv[-1].endswith(".png") else "prism_rainbow_render.png"
    out_file = os.path.abspath(out_file)
    setup_prism_scene(out_file, dispersion=0.08, num_photons=400000, samples=32, radius=0.055)
