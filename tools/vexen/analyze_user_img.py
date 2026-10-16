import bpy
import numpy as np

img = bpy.data.images.load('C:/Users/death/.gemini/antigravity/brain/058ee3ca-075f-49f2-ba98-1b6c1598bce3/.user_uploaded/media_1792129765566_863cdcc0.png')
w, h = img.size
pixels = np.array(img.pixels[:]).reshape((h, w, 4))[:, :, :3]
print(f"Image Size: {w}x{h}")

diff = np.max(pixels, axis=2) - np.min(pixels, axis=2)
bright = np.max(pixels, axis=2) > 0.2

print(f"Bright pixels: {np.sum(bright)}")
if np.sum(bright) > 0:
    print(f"Max saturation on bright pixels: {np.max(diff[bright]):.4f}")
    print(f"Mean saturation on bright pixels: {np.mean(diff[bright]):.4f}")
    
    # Check max red/blue ratio
    r = pixels[:, :, 0][bright]
    g = pixels[:, :, 1][bright]
    b = pixels[:, :, 2][bright]
    print(f"R mean={np.mean(r):.3f}, G mean={np.mean(g):.3f}, B mean={np.mean(b):.3f}")
