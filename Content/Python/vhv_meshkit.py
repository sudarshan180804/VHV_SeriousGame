"""Small helpers for building static meshes from primitives with Geometry Script.

Used by build_placeholder_models.py and build_village_environment.py.
Units are centimetres; +Z is up. Requires the GeometryScripting plugin (editor only).
"""

import math
import unreal


PRIMS = unreal.GeometryScript_Primitives
BASE = unreal.GeometryScriptPrimitiveOriginMode.BASE
CENTER = unreal.GeometryScriptPrimitiveOriginMode.CENTER
MATERIAL_DIR = "/Game/VHV_Stuff/Environment/Materials"
MASTER_MATERIAL_PATH = MATERIAL_DIR + "/M_Env_Master"


def srgb_to_linear(value):
    c = value / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def ensure_material_instance(name, srgb, roughness):
    """Create MI_<name> under the lightweight master material, or fix its parent and tint.

    Saves only when something changed, so reruns do not rewrite unchanged assets.
    """
    master = unreal.load_asset(MASTER_MATERIAL_PATH)
    if not master:
        raise RuntimeError("Missing {}; run setup_village_main_map.py first".format(MASTER_MATERIAL_PATH))
    path = "{}/{}".format(MATERIAL_DIR, name)
    mel = unreal.MaterialEditingLibrary
    changed = False
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        instance = unreal.EditorAssetLibrary.load_asset(path)
    else:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MATERIAL_DIR, unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        changed = True
    if instance.get_editor_property("parent") != master:
        mel.set_material_instance_parent(instance, master)
        changed = True
    tint = unreal.LinearColor(*[srgb_to_linear(c) for c in srgb] + [1.0])
    current_tint = mel.get_material_instance_vector_parameter_value(instance, "Tint")
    if any(abs(a - b) > 1e-4 for a, b in zip((current_tint.r, current_tint.g, current_tint.b),
                                               (tint.r, tint.g, tint.b))):
        mel.set_material_instance_vector_parameter_value(instance, "Tint", tint)
        changed = True
    if abs(mel.get_material_instance_scalar_parameter_value(instance, "Roughness") - roughness) > 1e-4:
        mel.set_material_instance_scalar_parameter_value(instance, "Roughness", roughness)
        changed = True
    if changed:
        mel.update_material_instance(instance)
        unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False)
    return instance


def _roll_sign_that_lowers_positive_y():
    probe = unreal.Transform(location=unreal.Vector(0.0, 0.0, 0.0),
                             rotation=unreal.Rotator(roll=30.0, pitch=0.0, yaw=0.0),
                             scale=unreal.Vector(1.0, 1.0, 1.0))
    tipped = unreal.MathLibrary.transform_location(probe, unreal.Vector(0.0, 1.0, 0.0))
    return -1.0 if tipped.z > 0.0 else 1.0


ROLL_SIGN = None


def transform(location=(0.0, 0.0, 0.0), yaw=0.0, pitch=0.0, roll=0.0, scale=(1.0, 1.0, 1.0)):
    return unreal.Transform(location=unreal.Vector(*location),
                            rotation=unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw),
                            scale=unreal.Vector(*scale))


class MeshKit(object):
    """Accumulates primitives into one DynamicMesh; each named slot is one material."""

    def __init__(self, materials):
        global ROLL_SIGN
        if ROLL_SIGN is None:
            ROLL_SIGN = _roll_sign_that_lowers_positive_y()
        self.mesh = unreal.DynamicMesh()
        self.materials = materials          # slot name -> MaterialInterface
        self.slots = []

    def _options(self, slot):
        if slot not in self.slots:
            self.slots.append(slot)
        return unreal.GeometryScriptPrimitiveOptions(material_id=self.slots.index(slot))

    # -- primitives ---------------------------------------------------------

    def box(self, slot, center, size, yaw=0.0, pitch=0.0, roll=0.0):
        PRIMS.append_box(self.mesh, self._options(slot), transform(center, yaw, pitch, roll),
                         size[0], size[1], size[2], 0, 0, 0, CENTER)

    def box_from(self, slot, x0, x1, y0, y1, z0, z1):
        self.box(slot, ((x0 + x1) / 2.0, (y0 + y1) / 2.0, (z0 + z1) / 2.0),
                 (abs(x1 - x0), abs(y1 - y0), abs(z1 - z0)))

    def beam(self, slot, start, end, thickness):
        sx, sy, sz = start
        ex, ey, ez = end
        dx, dy, dz = ex - sx, ey - sy, ez - sz
        length = math.sqrt(dx * dx + dy * dy + dz * dz)
        yaw = math.degrees(math.atan2(dy, dx))
        pitch = math.degrees(math.atan2(dz, math.sqrt(dx * dx + dy * dy)))
        self.box(slot, ((sx + ex) / 2.0, (sy + ey) / 2.0, (sz + ez) / 2.0), (length, thickness, thickness),
                 yaw=yaw, pitch=pitch)

    def cylinder(self, slot, base, radius, height, steps=10, yaw=0.0, pitch=0.0, roll=0.0):
        PRIMS.append_cylinder(self.mesh, self._options(slot), transform(base, yaw, pitch, roll),
                              radius, height, steps, 0, True, BASE)

    def cone(self, slot, base, base_radius, top_radius, height, steps=10, pitch=0.0, yaw=0.0):
        PRIMS.append_cone(self.mesh, self._options(slot), transform(base, yaw, pitch),
                          base_radius, top_radius, height, steps, 0, True, BASE)

    def sphere(self, slot, center, radius, scale=(1.0, 1.0, 1.0), steps=8):
        PRIMS.append_sphere_lat_long(self.mesh, self._options(slot), transform(center, scale=scale),
                                     radius, steps, steps * 2, CENTER)

    def prism_x(self, slot, profile_yz, x0, x1):
        """Extrude a YZ polygon along +X from x0 to x1 (gable triangles)."""
        rotation = unreal.MathLibrary.make_rot_from_xy(unreal.Vector(0.0, 1.0, 0.0), unreal.Vector(0.0, 0.0, 1.0))
        where = unreal.Transform(location=unreal.Vector(x0, 0.0, 0.0), rotation=rotation,
                                 scale=unreal.Vector(1.0, 1.0, 1.0))
        # The extrusion expects a counter-clockwise outline; a clockwise one comes out inside-out.
        area = sum(y0 * z1 - y1 * z0 for (y0, z0), (y1, z1) in zip(profile_yz, profile_yz[1:] + profile_yz[:1]))
        if area < 0.0:
            profile_yz = list(reversed(profile_yz))
        points = [unreal.Vector2D(y, z) for y, z in profile_yz]
        PRIMS.append_simple_extrude_polygon(self.mesh, self._options(slot), where, points, x1 - x0, 0, True, BASE)

    # -- building parts ---------------------------------------------------------

    def gable_roof(self, slot, x0, x1, half_depth, eave_z, pitch_deg, overhang, thickness=6.0, fascia=None):
        """Two slabs meeting at a ridge along X. Returns the ridge height."""
        slope = math.tan(math.radians(pitch_deg))
        run = half_depth + overhang
        ridge_z = eave_z + half_depth * slope
        for side in (-1.0, 1.0):
            center_z = ridge_z - (run / 2.0) * slope + thickness / 2.0
            self.box(slot, ((x0 + x1) / 2.0, side * run / 2.0, center_z),
                     (x1 - x0, run / math.cos(math.radians(pitch_deg)), thickness),
                     roll=side * ROLL_SIGN * pitch_deg)
            if fascia:
                edge_z = eave_z - overhang * slope
                self.box_from(fascia, x0, x1, side * run - 2.5 * side, side * run + 2.5 * side,
                              edge_z - 15.0, edge_z + thickness)
        self.box_from(slot, x0, x1, -8.0, 8.0, ridge_z - 2.0, ridge_z + thickness + 6.0)
        return ridge_z

    def gable_fill(self, slot, x_face, outward, half_depth, eave_z, ridge_z, thickness=8.0):
        x0, x1 = (x_face - thickness, x_face) if outward > 0 else (x_face, x_face + thickness)
        self.prism_x(slot, [(-half_depth, eave_z), (half_depth, eave_z), (0.0, ridge_z)], x0, x1)

    def window(self, wall, pos, sill_z, width, height, frame, glass, bars=0, crossbars=0, mullion=False):
        """Framed window on a wall face. `wall` = ("x" or "y", face coordinate, outward sign)."""
        axis, face, out = wall

        def panel(a0, a1, z0, z1, proud, slot):
            n0, n1 = sorted((face, face + out * proud))
            if axis == "y":
                self.box_from(slot, a0, a1, n0, n1, z0, z1)
            else:
                self.box_from(slot, n0, n1, a0, a1, z0, z1)

        a0, a1 = pos - width / 2.0, pos + width / 2.0
        z0, z1 = sill_z, sill_z + height
        f = 6.0
        panel(a0, a1, z0, z1, 1.5, glass)
        panel(a0 - f, a1 + f, z1, z1 + f, 5.0, frame)
        panel(a0 - f, a1 + f, z0 - f, z0, 7.0, frame)
        panel(a0 - f, a0, z0, z1, 5.0, frame)
        panel(a1, a1 + f, z0, z1, 5.0, frame)
        if mullion:
            panel(pos - 2.5, pos + 2.5, z0, z1, 5.0, frame)
        for i in range(1, bars + 1):
            a = a0 + width * i / (bars + 1)
            panel(a - 1.2, a + 1.2, z0, z1, 4.0, frame)
        for i in range(1, crossbars + 1):
            z = z0 + height * i / (crossbars + 1)
            panel(a0, a1, z - 1.2, z + 1.2, 4.0, frame)

    # -- output -----------------------------------------------------------------

    def _apply_materials(self, static_mesh):
        static_mesh.set_editor_property("static_materials", [
            unreal.StaticMaterial(material_interface=self.materials[slot], material_slot_name=slot)
            for slot in self.slots
        ])

    def create_asset(self, asset_path, collision=True):
        """Create a new static mesh asset. Returns the asset."""
        options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions(
            enable_recompute_normals=False,
            enable_recompute_tangents=True,
            enable_collision=collision,
            collision_mode=unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        static_mesh, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
            self.mesh, asset_path, options)
        if outcome != unreal.GeometryScriptOutcomePins.SUCCESS or not static_mesh:
            raise RuntimeError("Could not create {}".format(asset_path))
        self._apply_materials(static_mesh)
        unreal.EditorAssetLibrary.save_loaded_asset(static_mesh, only_if_is_dirty=False)
        return static_mesh

    def write_asset(self, asset_path, collision=True):
        """Create the asset, or overwrite its geometry in place so placed actors keep working."""
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            return self.create_asset(asset_path, collision)
        static_mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
        options = unreal.GeometryScriptCopyMeshToAssetOptions(
            enable_recompute_normals=False, enable_recompute_tangents=True, replace_materials=False)
        _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
            self.mesh, static_mesh, options, unreal.GeometryScriptMeshWriteLOD(), False)
        if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
            raise RuntimeError("Could not update {}".format(asset_path))
        self._apply_materials(static_mesh)
        body = static_mesh.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        unreal.EditorAssetLibrary.save_loaded_asset(static_mesh, only_if_is_dirty=False)
        return static_mesh
