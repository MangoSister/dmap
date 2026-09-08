"""Builds scene S5 of the path tracer plan in Blender and exports it as glTF
with the displacement attached through custom properties (plan D8, T10):

    dmap_displacement  the [displacement.<name>] asset of the toml scene, or
                       a map path relative to the exported file
    dmap_strength, dmap_midlevel, dmap_uv_scale, dmap_uv_offset

Export settings that matter: Custom Properties on (export_extras), Apply
Modifiers off (export_apply), so any Subdivision + Displace preview
modifiers stay a preview. Blender's exporter flips v, so the glTF texture
coordinates follow ks's no-flip convention for glTF.

Run headless:
    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b -P blender_s5.py
"""

import math
import os

import bpy

DATA = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "data"))
OUT = os.path.join(DATA, "scenes", "assets", "s5_blender.glb")


def principled(name, base, emission=None, strength=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    p = mat.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = base
    p.inputs["Roughness"].default_value = 0.5
    if emission is not None:
        p.inputs["Emission Color"].default_value = emission
        p.inputs["Emission Strength"].default_value = strength
    return mat


bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene

# The torus with its own texture coordinates, from the OBJ (y up -> z up on import).
bpy.ops.wm.obj_import(filepath=os.path.join(DATA, "simple", "cc_torus.obj"))
torus = bpy.context.selected_objects[0]
torus.name = "torus"
torus.data.name = "torus"
torus.data.materials.clear()
torus.data.materials.append(principled("torus_mat", (0.9, 0.6, 0.2, 1.0)))
torus.location = (0.0, 0.0, 0.6)
torus["dmap_displacement"] = "s5_rock"
torus["dmap_uv_scale"] = 2.0
torus["dmap_uv_offset"] = [0.1, 0.2]

bpy.ops.mesh.primitive_plane_add(size=8.0, location=(0.0, 0.0, 0.0))
floor = bpy.context.active_object
floor.name = "floor"
floor.data.materials.append(principled("floor_mat", (0.7, 0.7, 0.7, 1.0)))

# An emissive plane facing down, displaced with cobble.
bpy.ops.mesh.primitive_plane_add(size=1.5, location=(0.0, 0.0, 3.0), rotation=(math.pi, 0.0, 0.0))
emitter = bpy.context.active_object
emitter.name = "emitter"
emitter.data.materials.append(principled("emitter_mat", (1.0, 1.0, 1.0, 1.0), (1.0, 0.9, 0.7, 1.0), 8.0))
emitter["dmap_displacement"] = "s5_cobble"
emitter["dmap_strength"] = 0.05
emitter["dmap_midlevel"] = 0.5

# A patch whose displacement names a map file, not a toml asset.
bpy.ops.mesh.primitive_plane_add(size=1.0, location=(2.5, 0.0, 0.01))
patch = bpy.context.active_object
patch.name = "patch"
patch.data.materials.append(principled("patch_mat", (0.4, 0.5, 0.8, 1.0)))
patch["dmap_displacement"] = "../../simple/disp_terrain.png"
patch["dmap_strength"] = 0.1
patch["dmap_midlevel"] = 0.0

bpy.ops.object.camera_add(location=(4.5, -4.5, 3.2), rotation=(math.radians(65.0), 0.0, math.radians(45.0)))
scene.camera = bpy.context.active_object

bpy.ops.export_scene.gltf(
    filepath=OUT,
    export_format="GLB",
    export_extras=True,
    export_apply=False,
    export_yup=True,
    export_cameras=True,
    export_lights=False,
    export_materials="EXPORT",
    export_normals=True,
    export_texcoords=True,
)
print("wrote", OUT)
