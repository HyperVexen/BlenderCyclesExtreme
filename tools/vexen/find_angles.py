import bpy, math
from mathutils import Euler, Vector

# Default Suzanne has:
# Brow/Forehead at +Z, +Y (or -Y)
# Let's inspect default vertices
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()
bpy.ops.mesh.primitive_monkey_add()
m = bpy.context.object

# Find vertex for nose tip, brow center, etc.
min_y = min(v.co.y for v in m.data.vertices) # nose tip is most negative Y in Blender!
max_z = max(v.co.y for v in m.data.vertices) # top of head is positive Z
print(f"Default monkey: nose tip Y={min_y:.2f}, ears along X")

# In reference photo:
# Looking from front (+Y looking towards -Y or -Y looking towards +Y):
# Camera is at -Y looking towards +Y (standard Blender camera setup).
# When camera is at (0, -3.4, 2.1), camera looks in +Y direction.
# If Suzanne is tilted so brow is at top-left and nose is towards bottom-right / center:
# In reference:
# Brow is at upper-left.
# Nose/mouth is at center-right.
# Right ear on floor at bottom-right.
# Left ear on floor at bottom-left.
# Rotation:
# Tilted back around X: e.g. -40 degrees (or -50)
# Rotated around Z: e.g. -45 degrees
# Rotated around Y: e.g. 20 degrees
for rx in [-30, -45, -60]:
    for ry in [0, 15, -15]:
        for rz in [30, 45, -30, -45]:
            pass
print("Done")
