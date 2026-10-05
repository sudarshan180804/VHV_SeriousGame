"""Create the assets used by the ground navigation trail (AVHVNavigationTrail).

  /Game/VHV_Stuff/Navigation/SM_NavChevron   flat chevron arrow pointing +X, about 50 cm wide
  /Game/VHV_Stuff/Navigation/M_NavTrail      unlit gold glow; a pulse runs along the route towards
                                             the target using per-instance custom data 0
                                             (each arrow's negated distance to the target, in cm,
                                             so values grow towards the target)

Assets are only created when missing, so hand-made replacements are kept. The C++ trail loads
them by path, so keep the names.
"""

import importlib
import traceback
import unreal

import vhv_meshkit

# Unreal keeps imported modules cached between runs in the editor; reload so edits are picked up.
importlib.reload(vhv_meshkit)

from vhv_meshkit import MeshKit, srgb_to_linear  # noqa: E402


NAV_DIR = "/Game/VHV_Stuff/Navigation"
MESH_PATH = NAV_DIR + "/SM_NavChevron"
MATERIAL_PATH = NAV_DIR + "/M_NavTrail"
LOG = "[Navigation Assets]"

GLOW_SRGB = (255, 196, 80)
GLOW_INTENSITY = 2.2
PULSE_CYCLES_PER_SECOND = 0.8
PULSE_WAVELENGTH_CM = 600.0


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def build_material():
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH):
        return unreal.load_asset(MATERIAL_PATH), False
    ensure_directory(NAV_DIR)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_NavTrail", NAV_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_instanced_static_meshes", True)

    mel = unreal.MaterialEditingLibrary
    expr = mel.create_material_expression

    # Pulse = 0.675 + 0.325 * sin(2*pi * (Time * speed - Distance / wavelength)): a glow moving towards the target.
    time = expr(material, unreal.MaterialExpressionTime, -900, 0)
    phase_time = expr(material, unreal.MaterialExpressionMultiply, -700, 0)
    phase_time.set_editor_property("const_b", PULSE_CYCLES_PER_SECOND)
    mel.connect_material_expressions(time, "", phase_time, "A")

    distance = expr(material, unreal.MaterialExpressionPerInstanceCustomData, -900, 200)
    distance.set_editor_properties({"data_index": 0, "const_default_value": 0.0})
    phase_distance = expr(material, unreal.MaterialExpressionDivide, -700, 200)
    phase_distance.set_editor_property("const_b", PULSE_WAVELENGTH_CM)
    mel.connect_material_expressions(distance, "", phase_distance, "A")

    phase = expr(material, unreal.MaterialExpressionSubtract, -500, 100)
    mel.connect_material_expressions(phase_time, "", phase, "A")
    mel.connect_material_expressions(phase_distance, "", phase, "B")
    wave = expr(material, unreal.MaterialExpressionSine, -350, 100)
    mel.connect_material_expressions(phase, "", wave, "")
    scaled = expr(material, unreal.MaterialExpressionMultiply, -200, 100)
    scaled.set_editor_property("const_b", 0.325)
    mel.connect_material_expressions(wave, "", scaled, "A")
    pulse = expr(material, unreal.MaterialExpressionAdd, -50, 100)
    pulse.set_editor_property("const_b", 0.675)
    mel.connect_material_expressions(scaled, "", pulse, "A")

    colour = expr(material, unreal.MaterialExpressionConstant3Vector, -200, -150)
    colour.set_editor_property("constant", unreal.LinearColor(
        *[srgb_to_linear(c) * GLOW_INTENSITY for c in GLOW_SRGB] + [1.0]))
    emissive = expr(material, unreal.MaterialExpressionMultiply, 100, 0)
    mel.connect_material_expressions(colour, "", emissive, "A")
    mel.connect_material_expressions(pulse, "", emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.layout_material_expressions(material)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material, True


def build_mesh(material):
    if unreal.EditorAssetLibrary.does_asset_exist(MESH_PATH):
        return False
    ensure_directory(NAV_DIR)
    kit = MeshKit({"Glow": material})
    # Two flat arms meeting at a tip on +X: a chevron about 50 cm wide and 40 cm long.
    arm_length, arm_width, thickness = 46.0, 9.0, 2.0
    tip_x = 14.0
    for side in (1.0, -1.0):
        kit.box("Glow", (tip_x - 0.5 * arm_length * 0.819, side * 0.5 * arm_length * 0.574, thickness / 2.0),
                (arm_length, arm_width, thickness), yaw=side * 145.0)
    kit.create_asset(MESH_PATH, collision=False)
    return True


def main():
    material, made_material = build_material()
    made_mesh = build_mesh(material)
    unreal.log_warning("{} material {}, mesh {}".format(
        LOG, "created" if made_material else "kept", "created" if made_mesh else "kept"))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
