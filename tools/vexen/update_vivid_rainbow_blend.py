"""
Configure prism_rainbow_caustics.blend with a narrow collimated beam (4 deg)
and high dispersion (0.20) so the dispersion separates completely across the beam,
producing an unmistakable, vibrant rainbow ribbon in the interactive viewport.
"""

import bpy
import bmesh
import math
import os

def update_scene_for_vivid_rainbow(blend_path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene

    # 1. Dark World
    scene.world = bpy.data.worlds.new("World")
    scene.world.use_nodes = True
    bg = scene.world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.005, 0.005, 0.008, 1.0)
        bg.inputs["Strength"].default_value = 0.2

    # 2. Floor Receiver
    mesh_floor = bpy.data.meshes.new("FloorMesh")
    obj_floor = bpy.data.objects.new("Floor", mesh_floor)
    bpy.context.collection.objects.link(obj_floor)

    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=14.0)
    bm.to_mesh(mesh_floor)
    bm.free()
    obj_floor.location = (0, 0, 0)

    mat_floor = bpy.data.materials.new("FloorMat")
    mat_floor.use_nodes = True
    bsdf_floor = mat_floor.node_tree.nodes.get("Principled BSDF")
    if bsdf_floor:
        bsdf_floor.inputs["Base Color"].default_value = (0.10, 0.10, 0.12, 1.0)
        bsdf_floor.inputs["Roughness"].default_value = 0.4
        bsdf_floor.inputs["Specular IOR Level"].default_value = 0.05
    obj_floor.data.materials.append(mat_floor)
    obj_floor.cycles.is_caustics_receiver = True

    # 3. Glass Prism
    mesh_prism = bpy.data.meshes.new("PrismMesh")
    obj_prism = bpy.data.objects.new("Glass_Prism", mesh_prism)
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
    # 25 deg angle of incidence for strong refraction
    obj_prism.rotation_euler = (0.0, 0.0, math.radians(25))

    mat_glass = bpy.data.materials.new("Prism_Glass")
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

    # 4. Narrow White Beam (6.0 degrees aperture, high intensity)
    # A narrow beam prevents colors from overlapping into white in the center!
    light_data = bpy.data.lights.new("White_Beam", type='SPOT')
    light_data.energy = 5000.0
    light_data.color = (1.0, 1.0, 1.0)
    light_data.spot_size = math.radians(6.5)  # Narrow beam!
    light_data.spot_blend = 0.05
    light_data.shadow_soft_size = 0.005

    light_obj = bpy.data.objects.new("White_Beam", light_data)
    bpy.context.collection.objects.link(light_obj)
    light_obj.location = (-1.9, -0.8, 1.5)

    dx = 0.0 - light_obj.location[0]
    dy = 0.0 - light_obj.location[1]
    dz = 0.45 - light_obj.location[2]
    pitch = math.atan2(math.sqrt(dx*dx + dy*dy), -dz)
    yaw = math.atan2(dy, dx)
    light_obj.rotation_euler = (pitch, 0, yaw - math.pi/2)

    # Rim light
    fill_data = bpy.data.lights.new("Rim_Fill", type='POINT')
    fill_data.energy = 30.0
    fill_data.color = (0.6, 0.75, 1.0)
    fill_obj = bpy.data.objects.new("Rim_Fill", fill_data)
    bpy.context.collection.objects.link(fill_obj)
    fill_obj.location = (2.2, 2.5, 2.0)

    # 5. Camera
    cam_data = bpy.data.cameras.new("Camera")
    cam_data.lens = 42.0
    cam_obj = bpy.data.objects.new("Camera", cam_data)
    bpy.context.collection.objects.link(cam_obj)
    scene.camera = cam_obj

    cam_obj.location = (1.8, -2.7, 2.1)
    cam_obj.rotation_euler = (math.radians(55), 0, math.radians(24))

    # 6. Render & Caustics Settings
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'GPU'
    scene.render.resolution_x = 960
    scene.render.resolution_y = 540
    scene.render.resolution_percentage = 100
    scene.cycles.samples = 32
    scene.cycles.preview_samples = 32

    # Enable Vexen Photon Caustics Extreme with Milestone 7 Continuous Spectral Dispersion
    scene.cycles.use_vexen_extreme = True
    scene.cycles.vexen_photon_caustics = True
    scene.cycles.photon_caustics_count = 600000
    scene.cycles.photon_caustics_radius = 0.065   # Smooth gather radius!
    scene.cycles.photon_caustics_alpha = 1.0     # Constant radius so it doesn't shrink to salt!
    scene.cycles.photon_caustics_dispersion = 0.18 # High, vivid dispersion!
    scene.cycles.photon_caustics_max_bounces = 8
    scene.cycles.photon_caustics_clamp = 50.0

    # Save interactive .blend file
    bpy.ops.wm.save_as_mainfile(filepath=blend_path)
    print(f"Interactive .blend scene updated at: {blend_path}")

    # Render a preview image
    preview_img_path = blend_path.replace(".blend", "_preview.png")
    scene.render.filepath = preview_img_path
    bpy.ops.render.render(write_still=True)
    print(f"Rendered preview to: {preview_img_path}")

if __name__ == "__main__":
    blend_file = os.path.abspath("prism_rainbow_caustics.blend")
    update_scene_for_vivid_rainbow(blend_file)
