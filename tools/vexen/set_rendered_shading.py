import bpy

for window in bpy.context.window_manager.windows:
    screen = window.screen
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            for space in area.spaces:
                if space.type == 'VIEW_3D':
                    space.shading.type = 'RENDERED'

bpy.ops.wm.save_mainfile()
print("Saved blend file with viewport shading = RENDERED.")
