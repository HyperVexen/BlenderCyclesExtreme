import bpy
import os

def run():
    print("=== TEST GUI VIEWPORT SCRIPT RUNNING ===")
    
    # Ensure viewport is in RENDERED mode
    for window in bpy.context.window_manager.windows:
        screen = window.screen
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                for space in area.spaces:
                    if space.type == 'VIEW_3D':
                        space.shading.type = 'RENDERED'
    
    # Take screenshot at 4 seconds
    def take_screenshot():
        shot_path = "C:/tmp/viewport_screenshot.png"
        if os.path.exists(shot_path):
            try:
                os.remove(shot_path)
            except Exception:
                pass
        bpy.ops.screen.screenshot(filepath=shot_path)
        print(f"=== SCREENSHOT SAVED TO {shot_path} ===")
        return None

    # Quit at 6 seconds
    def quit_timer():
        print("=== QUITTING BLENDER ===")
        bpy.ops.wm.quit_blender()
        return None

    bpy.app.timers.register(take_screenshot, first_interval=4.0)
    bpy.app.timers.register(quit_timer, first_interval=6.0)

bpy.app.timers.register(run, first_interval=1.0)
