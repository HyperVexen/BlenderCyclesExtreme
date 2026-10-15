#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Vexen Cycles Extreme Contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate the deterministic M0 Cycles photon-caustics benchmark scenes.

Run with a Blender binary from the repository root:

  blender --background --factory-startup --python \
    tools/vexen/generate_m0_scenes.py -- \
    --output-dir tests/benchmarks/cycles/vexen

The output directory intentionally lives in ``tests/benchmarks`` so Blender's
existing performance harness discovers these files as ``cycles/vexen/*``. The
generated blend files are test inputs, not source files: rerun this script to
update them after changing a scene definition.
"""

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


RESOLUTION = 512
SAMPLES = 128


def clear_scene() -> None:
    """Remove all objects and datablocks that can affect a generated scene."""
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)

    for collection in (bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for datablock in collection:
            collection.remove(datablock)


def set_socket(node: bpy.types.Node, name: str, value: object) -> None:
    socket = node.inputs.get(name)
    if socket is not None:
        socket.default_value = value


def make_material(name: str, base_color: tuple[float, float, float, float], *, glass=False):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    principled = material.node_tree.nodes.get('Principled BSDF')
    set_socket(principled, 'Base Color', base_color)
    set_socket(principled, 'Roughness', 0.0 if glass else 0.65)

    if glass:
        # The socket name changed across Blender releases. Support both names
        # so a scene can be generated from current and nearby development builds.
        set_socket(principled, 'Transmission Weight', 1.0)
        set_socket(principled, 'Transmission', 1.0)
        set_socket(principled, 'IOR', 1.45)

    return material


def assign_material(obj: bpy.types.Object, material: bpy.types.Material) -> None:
    obj.data.materials.append(material)


def look_at(obj: bpy.types.Object, target: tuple[float, float, float]) -> None:
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()


def add_camera(location: tuple[float, float, float], target: tuple[float, float, float]) -> None:
    camera_data = bpy.data.cameras.new('Camera')
    camera_data.lens = 50.0
    camera = bpy.data.objects.new('Camera', camera_data)
    bpy.context.scene.collection.objects.link(camera)
    camera.location = location
    look_at(camera, target)
    bpy.context.scene.camera = camera


def add_area_light(location: tuple[float, float, float], target: tuple[float, float, float], energy=1200.0,
                   size=1.0) -> None:
    light_data = bpy.data.lights.new('Key', 'AREA')
    light_data.energy = energy
    light_data.shape = 'DISK'
    light_data.size = size
    light = bpy.data.objects.new('Key', light_data)
    bpy.context.scene.collection.objects.link(light)
    light.location = location
    look_at(light, target)


def add_plane(size=16.0, z=0.0, material=None) -> bpy.types.Object:
    bpy.ops.mesh.primitive_plane_add(size=size, location=(0.0, 0.0, z))
    plane = bpy.context.object
    if material is not None:
        assign_material(plane, material)
    return plane


def add_uv_sphere(location, scale, material) -> bpy.types.Object:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, location=location)
    sphere = bpy.context.object
    sphere.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign_material(sphere, material)
    return sphere


def configure_scene(name: str) -> None:
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.render.resolution_x = RESOLUTION
    scene.render.resolution_y = RESOLUTION
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'OPEN_EXR'
    scene.render.film_transparent = False
    scene.render.use_file_extension = True
    scene.cycles.samples = SAMPLES
    scene.cycles.use_adaptive_sampling = False
    scene.cycles.use_denoising = False
    scene.cycles.caustics_reflective = True
    scene.cycles.caustics_refractive = True
    scene.world.color = (0.01, 0.01, 0.01)
    scene.render.filepath = f'//{name}.exr'


def glass_sphere_on_plane() -> None:
    diffuse = make_material('Receiver', (0.7, 0.7, 0.7, 1.0))
    glass = make_material('Glass', (0.95, 0.98, 1.0, 1.0), glass=True)
    add_plane(material=diffuse)
    add_uv_sphere((0.0, 0.0, 1.3), (1.3, 1.3, 1.3), glass)
    add_area_light((-2.5, -2.5, 6.0), (0.0, 0.0, 0.0), energy=1800.0, size=0.35)
    add_camera((7.0, -7.0, 5.0), (0.0, 0.0, 1.0))


def glass_ring_and_cup() -> None:
    diffuse = make_material('Receiver', (0.55, 0.45, 0.35, 1.0))
    glass = make_material('Glass', (0.9, 1.0, 0.95, 1.0), glass=True)
    add_plane(material=diffuse)

    bpy.ops.mesh.primitive_torus_add(major_radius=1.3, minor_radius=0.22, major_segments=96, minor_segments=24,
                                     location=(-1.3, 0.0, 1.15))
    assign_material(bpy.context.object, glass)
    bpy.ops.mesh.primitive_cylinder_add(vertices=96, radius=1.0, depth=1.8, location=(1.5, 0.0, 0.9))
    cup = bpy.context.object
    assign_material(cup, glass)
    solidify = cup.modifiers.new('Cup wall', 'SOLIDIFY')
    solidify.thickness = 0.08
    add_area_light((-3.5, -3.5, 6.0), (0.0, 0.0, 0.0), energy=1700.0, size=0.25)
    add_camera((7.5, -8.0, 5.0), (0.0, 0.0, 0.9))


def pool_floor() -> None:
    diffuse = make_material('Pool floor', (0.16, 0.32, 0.38, 1.0))
    glass = make_material('Water lens', (0.85, 0.95, 1.0, 1.0), glass=True)
    add_plane(size=20.0, material=diffuse)
    add_uv_sphere((0.0, 0.0, 1.6), (3.0, 3.0, 0.35), glass)
    add_area_light((0.0, -1.0, 8.0), (0.0, 0.0, 0.0), energy=2500.0, size=1.2)
    add_camera((8.0, -8.0, 6.0), (0.0, 0.0, 0.5))


def prism_spotlight() -> None:
    diffuse = make_material('Receiver', (0.8, 0.8, 0.8, 1.0))
    glass = make_material('Prism', (1.0, 0.95, 0.85, 1.0), glass=True)
    add_plane(material=diffuse)
    bpy.ops.mesh.primitive_cone_add(vertices=3, radius1=1.5, radius2=1.5, depth=3.0,
                                    location=(0.0, 0.0, 1.5), rotation=(0.0, math.pi / 2.0, 0.0))
    assign_material(bpy.context.object, glass)
    light_data = bpy.data.lights.new('Spot', 'SPOT')
    light_data.energy = 1800.0
    light_data.spot_size = math.radians(20.0)
    light_data.shadow_soft_size = 0.02
    light = bpy.data.objects.new('Spot', light_data)
    bpy.context.scene.collection.objects.link(light)
    light.location = (-4.5, -2.0, 5.5)
    look_at(light, (0.0, 0.0, 0.5))
    add_camera((7.0, -7.0, 5.0), (0.0, 0.0, 1.0))


def no_caster_regression() -> None:
    diffuse = make_material('Diffuse', (0.42, 0.42, 0.42, 1.0))
    accent = make_material('Accent', (0.3, 0.55, 0.7, 1.0))
    add_plane(size=30.0, material=diffuse)
    for x in range(-12, 13):
        for y in range(-12, 13):
            height = 0.15 + 0.1 * ((x * 13 + y * 7) % 5)
            bpy.ops.mesh.primitive_cube_add(size=0.28, location=(x * 0.38, y * 0.38, height))
            assign_material(bpy.context.object, accent if (x + y) % 7 == 0 else diffuse)
    add_area_light((-5.0, -4.0, 8.0), (0.0, 0.0, 0.0), energy=2200.0, size=4.0)
    add_camera((9.5, -9.5, 8.0), (0.0, 0.0, 0.0))


def many_small_casters() -> None:
    diffuse = make_material('Receiver', (0.68, 0.65, 0.58, 1.0))
    glass = make_material('Glass', (0.94, 0.98, 1.0, 1.0), glass=True)
    add_plane(size=20.0, material=diffuse)
    for x in range(-4, 5):
        for y in range(-4, 5):
            add_uv_sphere((x * 0.65, y * 0.65, 0.42), (0.38, 0.38, 0.38), glass)
    add_area_light((-3.0, -3.0, 7.0), (0.0, 0.0, 0.0), energy=2200.0, size=0.5)
    add_camera((8.0, -8.0, 6.5), (0.0, 0.0, 0.5))


SCENES = {
    'glass_sphere_plane': glass_sphere_on_plane,
    'glass_ring_cup': glass_ring_and_cup,
    'pool_floor': pool_floor,
    'prism_spotlight': prism_spotlight,
    'no_caster_regression': no_caster_regression,
    'many_small_casters': many_small_casters,
}


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True, help='Directory for generated .blend files.')
    # Blender keeps its own command-line options in sys.argv. Arguments for a
    # Python script conventionally follow a standalone "--" separator.
    script_arguments = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    return parser.parse_args(script_arguments)


def main() -> None:
    args = parse_arguments()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    for name, build_scene in SCENES.items():
        clear_scene()
        configure_scene(name)
        build_scene()
        filepath = args.output_dir / f'{name}.blend'
        bpy.ops.wm.save_as_mainfile(filepath=str(filepath), check_existing=False)
        print(f'Generated {filepath}')


if __name__ == '__main__':
    main()
