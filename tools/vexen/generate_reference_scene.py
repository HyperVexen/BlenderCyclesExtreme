#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
#
# SPDX-License-Identifier: Apache-2.0
"""Generate a test caustic scene matching the user's reference image perfectly."""

import math
from pathlib import Path
import bpy
from mathutils import Euler, Vector

def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)

    for collection in (bpy.data.materials, bpy.data.cameras, bpy.data.lights, bpy.data.meshes, bpy.data.curves):
        for block in list(collection):
            collection.remove(block)

def make_glass_material(name="Glass_Material", ior=1.50):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    nodes.clear()

    output = nodes.new('ShaderNodeOutputMaterial')
    principled = nodes.new('ShaderNodeBsdfPrincipled')
    principled.inputs['Base Color'].default_value = (1.0, 1.0, 1.0, 1.0)
    principled.inputs['Roughness'].default_value = 0.0
    if 'Transmission Weight' in principled.inputs:
        principled.inputs['Transmission Weight'].default_value = 1.0
    elif 'Transmission' in principled.inputs:
        principled.inputs['Transmission'].default_value = 1.0
    principled.inputs['IOR'].default_value = ior

    mat.node_tree.links.new(principled.outputs['BSDF'], output.inputs['Surface'])
    return mat

def make_copper_material(name="Copper_Material"):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    principled = mat.node_tree.nodes.get('Principled BSDF')
    principled.inputs['Base Color'].default_value = (0.75, 0.40, 0.22, 1.0)  # Bronze/copper
    principled.inputs['Metallic'].default_value = 0.95
    principled.inputs['Roughness'].default_value = 0.32
    return mat

def make_backdrop_material(name="Backdrop_Material"):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    principled = mat.node_tree.nodes.get('Principled BSDF')
    principled.inputs['Base Color'].default_value = (0.90, 0.90, 0.91, 1.0)  # Studio white
    principled.inputs['Roughness'].default_value = 0.50
    if 'Specular IOR Level' in principled.inputs:
        principled.inputs['Specular IOR Level'].default_value = 0.2
    return mat

def create_cyclorama(size=18.0, height=12.0, radius=4.0, material=None):
    bpy.ops.mesh.primitive_plane_add(size=size, location=(0.0, 0.0, 0.0))
    plane = bpy.context.object
    plane.name = "Cyclorama"

    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='DESELECT')
    bpy.ops.object.mode_set(mode='OBJECT')

    for v in plane.data.vertices:
        if v.co.y > (size * 0.45):
            v.select = True

    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.extrude_region_move(TRANSFORM_OT_translate={"value": (0.0, 0.0, height)})
    bpy.ops.object.mode_set(mode='OBJECT')

    bevel = plane.modifiers.new("Bevel", 'BEVEL')
    bevel.affect = 'EDGES'
    bevel.width = radius
    bevel.segments = 24

    subsurf = plane.modifiers.new("Subsurf", 'SUBSURF')
    subsurf.levels = 2
    subsurf.render_levels = 2

    bpy.ops.object.shade_smooth()
    if material:
        plane.data.materials.append(material)
    return plane

def create_glass_suzanne(glass_mat):
    bpy.ops.mesh.primitive_monkey_add(size=1.4, location=(-0.3, 0.2, 0.0))
    suzanne = bpy.context.object
    suzanne.name = "Glass_Suzanne"

    # Euler angles matching the tilted resting pose in the reference
    suzanne.rotation_euler = Euler((math.radians(-54.0), math.radians(24.0), math.radians(35.0)), 'XYZ')
    bpy.context.view_layer.update()

    min_z = min((suzanne.matrix_world @ v.co).z for v in suzanne.data.vertices)
    suzanne.location.z = -min_z + 0.01

    subsurf = suzanne.modifiers.new("Subsurf", 'SUBSURF')
    subsurf.levels = 2
    subsurf.render_levels = 3

    bpy.ops.object.shade_smooth()
    suzanne.data.materials.append(glass_mat)
    if hasattr(suzanne.cycles, 'is_caustics_caster'):
        suzanne.cycles.is_caustics_caster = True
    return suzanne

def create_copper_figurine(copper_mat):
    bpy.ops.mesh.primitive_monkey_add(size=1.5, location=(-2.6, 0.1, 0.6))
    copper = bpy.context.object
    copper.name = "Copper_Suzanne"
    copper.rotation_euler = Euler((math.radians(12.0), math.radians(-15.0), math.radians(-75.0)), 'XYZ')
    bpy.context.view_layer.update()

    min_z = min((copper.matrix_world @ v.co).z for v in copper.data.vertices)
    copper.location.z = -min_z + 0.01

    subsurf = copper.modifiers.new("Subsurf", 'SUBSURF')
    subsurf.levels = 2
    subsurf.render_levels = 2

    bpy.ops.object.shade_smooth()
    copper.data.materials.append(copper_mat)
    return copper

def create_glass_sculpture(glass_mat):
    curve_data = bpy.data.curves.new('Glass_Wave', type='CURVE')
    curve_data.dimensions = '3D'
    curve_data.resolution_u = 48

    spline = curve_data.splines.new('BEZIER')
    points = [
        # (x, y, z, handle_left, handle_right)
        (1.4, -0.6, 0.22, (1.2, -1.2, 0.15), (1.8, 0.0, 0.45)),
        (2.4, 0.8, 0.75, (2.0, 0.3, 0.60), (2.7, 1.4, 0.95)),
        (2.6, 2.2, 1.15, (2.8, 1.7, 1.05), (2.2, 2.8, 1.30)),
        (1.2, 3.2, 1.25, (1.8, 3.2, 1.30), (0.6, 3.1, 1.10)),
    ]

    spline.bezier_points.add(len(points) - 1)
    for i, (x, y, z, hl, hr) in enumerate(points):
        bp = spline.bezier_points[i]
        bp.co = (x, y, z)
        bp.handle_left = hl
        bp.handle_right = hr
        bp.handle_left_type = 'FREE'
        bp.handle_right_type = 'FREE'

    curve_data.bevel_depth = 0.32
    curve_data.bevel_resolution = 16
    curve_data.use_fill_caps = True

    sculpture = bpy.data.objects.new('Glass_Sculpture', curve_data)
    bpy.context.scene.collection.objects.link(sculpture)

    bpy.context.view_layer.objects.active = sculpture
    sculpture.select_set(True)
    bpy.ops.object.convert(target='MESH')

    solidify = sculpture.modifiers.new("Solidify", 'SOLIDIFY')
    solidify.thickness = 0.08
    solidify.use_even_offset = True

    subsurf = sculpture.modifiers.new("Subsurf", 'SUBSURF')
    subsurf.levels = 1
    subsurf.render_levels = 2

    bpy.ops.object.shade_smooth()
    sculpture.data.materials.append(glass_mat)
    if hasattr(sculpture.cycles, 'is_caustics_caster'):
        sculpture.cycles.is_caustics_caster = True
    return sculpture

def setup_lighting():
    # Key light from high-right shining towards back-left
    key_data = bpy.data.lights.new('Key_Light', 'AREA')
    key_data.energy = 3200.0
    key_data.shape = 'DISK'
    key_data.size = 0.35
    key_obj = bpy.data.objects.new('Key_Light', key_data)
    bpy.context.scene.collection.objects.link(key_obj)
    key_obj.location = (4.0, -2.5, 4.8)

    target = Vector((0.2, 0.8, 0.7))
    direction = target - key_obj.location
    key_obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()

    # Studio fill light
    fill_data = bpy.data.lights.new('Fill_Light', 'AREA')
    fill_data.energy = 320.0
    fill_data.shape = 'RECTANGLE'
    fill_data.size = 4.0
    fill_data.size_y = 3.0
    fill_obj = bpy.data.objects.new('Fill_Light', fill_data)
    bpy.context.scene.collection.objects.link(fill_obj)
    fill_obj.location = (-3.5, -3.5, 4.0)
    fill_dir = Vector((0.0, 0.5, 0.5)) - fill_obj.location
    fill_obj.rotation_euler = fill_dir.to_track_quat('-Z', 'Y').to_euler()

    bpy.context.scene.world.use_nodes = False
    bpy.context.scene.world.color = (0.28, 0.28, 0.29)

def setup_camera():
    cam_data = bpy.data.cameras.new('Camera')
    cam_data.lens = 50.0
    cam_obj = bpy.data.objects.new('Camera', cam_data)
    bpy.context.scene.collection.objects.link(cam_obj)

    # Positioned with proper distance and perspective matching reference
    cam_obj.location = (-0.1, -4.8, 2.7)
    target = Vector((-0.1, 0.3, 0.75))
    cam_dir = target - cam_obj.location
    cam_obj.rotation_euler = cam_dir.to_track_quat('-Z', 'Y').to_euler()

    bpy.context.scene.camera = cam_obj
    return cam_obj

def configure_render_settings():
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.render.resolution_x = 768
    scene.render.resolution_y = 680
    scene.render.resolution_percentage = 100

    scene.cycles.samples = 64
    scene.cycles.use_adaptive_sampling = False
    scene.cycles.use_denoising = True
    scene.cycles.denoiser = 'OPENIMAGEDENOISE'
    scene.cycles.caustics_reflective = True
    scene.cycles.caustics_refractive = False

    # Configure Vexen Extreme Photon Caustics
    scene.cycles.use_vexen_extreme = True
    scene.cycles.vexen_photon_caustics = True
    scene.cycles.photon_caustics_count = 2000000
    scene.cycles.photon_caustics_radius = 0.08
    scene.cycles.photon_caustics_alpha = 0.666667
    scene.cycles.photon_caustics_max_bounces = 8
    scene.cycles.photon_caustics_clamp = 40.0

def main():
    clear_scene()

    glass_mat = make_glass_material("Glass_Crystal", ior=1.52)
    copper_mat = make_copper_material("Metallic_Copper")
    backdrop_mat = make_backdrop_material("Studio_Matte_White")

    create_cyclorama(size=18.0, height=12.0, radius=4.0, material=backdrop_mat)
    suzanne = create_glass_suzanne(glass_mat)
    create_copper_figurine(copper_mat)
    create_glass_sculpture(glass_mat)

    setup_lighting()
    setup_camera()
    configure_render_settings()

    # Select Suzanne so transform gizmo is active when opened
    bpy.ops.object.select_all(action='DESELECT')
    suzanne.select_set(True)
    bpy.context.view_layer.objects.active = suzanne

    out_dir = Path("tests/benchmarks/cycles/vexen")
    out_dir.mkdir(parents=True, exist_ok=True)
    blend_path1 = out_dir / "reference_caustics_scene.blend"
    blend_path2 = Path("reference_caustics_scene.blend")

    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path1))
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path2))
    print(f"Saved reference scene to: {blend_path1} and {blend_path2}")

if __name__ == '__main__':
    main()
