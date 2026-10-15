import bpy, math
from mathutils import Euler, Vector

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()

bpy.ops.mesh.primitive_monkey_add(size=1.8, location=(0,0,0))
s = bpy.context.object
s.rotation_euler = Euler((math.radians(75.0), math.radians(10.0), math.radians(-25.0)), 'XYZ')
bpy.context.view_layer.update()

min_z = min((s.matrix_world @ v.co).z for v in s.data.vertices)
s.location.z = -min_z
bpy.context.view_layer.update()
print(f"Suzanne resting Z = {s.location.z}")
