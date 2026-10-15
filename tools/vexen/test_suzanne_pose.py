import bpy, math
from mathutils import Euler, Vector

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()

bpy.ops.mesh.primitive_monkey_add(size=1.25)
m = bpy.context.object
m.rotation_euler = Euler((math.radians(-54.0), math.radians(24.0), math.radians(35.0)), 'XYZ')
bpy.context.view_layer.update()

min_z = min((m.matrix_world @ v.co).z for v in m.data.vertices)
m.location.z = -min_z + 0.01
bpy.context.view_layer.update()

# Print positions of key parts:
# Ears were at (+1.3, 0, 0) and (-1.3, 0, 0)
right_ear = m.matrix_world @ Vector((1.3, 0.0, 0.0))
left_ear = m.matrix_world @ Vector((-1.3, 0.0, 0.0))
nose = m.matrix_world @ Vector((0.0, -0.85, -0.3))
forehead = m.matrix_world @ Vector((0.0, -0.3, 0.6))

print(f"Right Ear Z={right_ear.z:.2f}, X={right_ear.x:.2f}, Y={right_ear.y:.2f}")
print(f"Left Ear Z={left_ear.z:.2f}, X={left_ear.x:.2f}, Y={left_ear.y:.2f}")
print(f"Nose Z={nose.z:.2f}, X={nose.x:.2f}, Y={nose.y:.2f}")
print(f"Forehead Z={forehead.z:.2f}, X={forehead.x:.2f}, Y={forehead.y:.2f}")
