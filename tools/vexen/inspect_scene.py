import bpy

print("=== SCENE OBJECTS ===")
for obj in bpy.data.objects:
    print(f"Object: {obj.name}, type: {obj.type}, loc: {obj.location}")

print("\n=== LIGHTS ===")
for light in bpy.data.lights:
    print(f"Light: {light.name}, type: {light.type}, energy: {light.energy}, color: {list(light.color)}")

print("\n=== WORLDS ===")
for w in bpy.data.worlds:
    print(f"World: {w.name}")
    if w.use_nodes:
        for node in w.node_tree.nodes:
            print(f"  Node: {node.name} ({node.type})")
            if node.type == 'BACKGROUND':
                print(f"    Strength: {node.inputs['Strength'].default_value}")
                print(f"    Color: {list(node.inputs['Color'].default_value)}")

print("\n=== MATERIALS ===")
for mat in bpy.data.materials:
    print(f"Material: {mat.name}")
