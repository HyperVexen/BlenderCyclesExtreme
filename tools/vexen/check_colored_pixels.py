import bpy
import numpy as np

img = bpy.data.images.load('C:/Users/death/.gemini/antigravity/brain/058ee3ca-075f-49f2-ba98-1b6c1598bce3/.user_uploaded/media_1792129765566_863cdcc0.png')
w, h = img.size
pixels = np.array(img.pixels[:]).reshape((h, w, 4))[:, :, :3]
diff = np.max(pixels, axis=2) - np.min(pixels, axis=2)

colored = diff > 0.20
coords = np.argwhere(colored)
print(f"Total colored pixels (saturation > 0.20): {len(coords)}")
if len(coords) > 0:
    print(f"Y range: {np.min(coords[:, 0])} to {np.max(coords[:, 0])} (out of {h})")
    print(f"X range: {np.min(coords[:, 1])} to {np.max(coords[:, 1])} (out of {w})")
    for y, x in coords[::max(1, len(coords)//10)]:
        rgb = pixels[y, x]
        print(f"Pixel at (X={x}, Y={y}): R={rgb[0]:.2f}, G={rgb[1]:.2f}, B={rgb[2]:.2f}")
