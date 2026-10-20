"""Author the three small materials used by the native navigation guidance.

The runtime route and destination visuals are geometry-driven, so this script
creates no textures, Niagara systems, Blueprints, or map actors. It is safe to
run repeatedly and only owns the two material assets named below.
"""

import traceback
import unreal


LOG = "[VHV Guidance Authoring]"
MATERIAL_DIR = "/Game/VHV_Stuff/UI/Tracking"
GOLD_PATH = MATERIAL_DIR + "/M_VHV_GuidanceGold"
BEAM_PATH = MATERIAL_DIR + "/M_VHV_GuidanceBeam"
CORE_PATH = MATERIAL_DIR + "/M_VHV_GuidanceCore"


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError("Could not create {}".format(path))


def load_or_create_material(path):
    material = (unreal.EditorAssetLibrary.load_asset(path)
                if unreal.EditorAssetLibrary.does_asset_exist(path)
                else None)
    if material:
        if not isinstance(material, unreal.Material):
            raise RuntimeError("{} exists but is not a Material".format(path))
        return material

    name = path.rsplit("/", 1)[-1]
    directory = path.rsplit("/", 1)[0]
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, directory, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Could not create {}".format(path))
    return material


def expression(material, expression_class, x, y, description=""):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y)
    if not node:
        raise RuntimeError("Could not create {}".format(expression_class.__name__))
    if description:
        try:
            node.set_editor_property("desc", description)
        except Exception:
            pass
    return node


def connect(source, target, *input_names):
    for input_name in input_names:
        if unreal.MaterialEditingLibrary.connect_material_expressions(
                source, "", target, input_name):
            return
    raise RuntimeError("Could not connect {} to {} ({})".format(
        source.get_name(), target.get_name(), input_names))


def connect_property(source, material_property):
    if not unreal.MaterialEditingLibrary.connect_material_property(
            source, "", material_property):
        raise RuntimeError("Could not connect {} to {}".format(
            source.get_name(), material_property))


def scalar(material, value, x, y, description=""):
    node = expression(
        material, unreal.MaterialExpressionConstant, x, y, description)
    node.set_editor_property("r", float(value))
    return node


def scalar_parameter(material, name, value, x, y):
    node = expression(
        material, unreal.MaterialExpressionScalarParameter, x, y, name)
    node.set_editor_properties({
        "parameter_name": unreal.Name(name),
        "default_value": float(value),
    })
    return node


def vector_parameter(material, name, value, x, y):
    node = expression(
        material, unreal.MaterialExpressionVectorParameter, x, y, name)
    node.set_editor_properties({
        "parameter_name": unreal.Name(name),
        "default_value": value,
    })
    return node


def binary(material, expression_class, a, b, x, y, description=""):
    node = expression(material, expression_class, x, y, description)
    connect(a, node, "A")
    connect(b, node, "B")
    return node


def unary(material, expression_class, source, x, y, description=""):
    node = expression(material, expression_class, x, y, description)
    connect(source, node, "Input", "")
    return node


def finalize(material):
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(
            material, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(material.get_path_name()))


def author_gold_material():
    material = load_or_create_material(GOLD_PATH)
    material.modify()
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    color = vector_parameter(
        material, "GuidanceColor",
        unreal.LinearColor(1.0, 0.28, 0.015, 1.0), -500, -80)
    strength = scalar_parameter(
        material, "EmissiveStrength", 4.0, -500, 90)
    emissive = binary(
        material, unreal.MaterialExpressionMultiply,
        color, strength, -180, 0, "Restrained golden emissive")
    connect_property(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finalize(material)
    return material


def author_beam_material():
    material = load_or_create_material(BEAM_PATH)
    material.modify()
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    uv = expression(
        material, unreal.MaterialExpressionTextureCoordinate,
        -1300, 250, "Beam-card UV")
    u = expression(material, unreal.MaterialExpressionComponentMask, -1100, 120)
    u.set_editor_properties({"r": True, "g": False, "b": False, "a": False})
    connect(uv, u, "Input", "")
    v = expression(material, unreal.MaterialExpressionComponentMask, -1100, 430)
    v.set_editor_properties({"r": False, "g": True, "b": False, "a": False})
    connect(uv, v, "Input", "")

    half = scalar(material, 0.5, -1100, -100)
    two = scalar(material, 2.0, -900, -100)
    centered = binary(
        material, unreal.MaterialExpressionSubtract,
        u, half, -900, 90, "Distance from beam center")
    absolute = unary(
        material, unreal.MaterialExpressionAbs,
        centered, -700, 90)
    doubled = binary(
        material, unreal.MaterialExpressionMultiply,
        absolute, two, -500, 90)
    inverted = unary(
        material, unreal.MaterialExpressionOneMinus,
        doubled, -300, 90)
    saturated_horizontal = unary(
        material, unreal.MaterialExpressionSaturate,
        inverted, -100, 90)
    horizontal = expression(
        material, unreal.MaterialExpressionPower,
        100, 90, "Soft tapered beam sides")
    horizontal.set_editor_property("const_exponent", 2.4)
    connect(saturated_horizontal, horizontal, "Base")

    five = scalar(material, 5.0, -900, 620)
    two_point_two = scalar(material, 2.2, -700, 800)
    bottom_multiply = binary(
        material, unreal.MaterialExpressionMultiply,
        v, five, -700, 470, "Short fade at the blessing base")
    bottom_fade = unary(
        material, unreal.MaterialExpressionSaturate,
        bottom_multiply, -500, 470)
    one_minus_v = unary(
        material, unreal.MaterialExpressionOneMinus,
        v, -700, 680)
    top_multiply = binary(
        material, unreal.MaterialExpressionMultiply,
        one_minus_v, two_point_two, -500, 680, "Long fade from above")
    top_fade = unary(
        material, unreal.MaterialExpressionSaturate,
        top_multiply, -300, 680)
    vertical = binary(
        material, unreal.MaterialExpressionMultiply,
        bottom_fade, top_fade, -100, 560)
    beam_mask = binary(
        material, unreal.MaterialExpressionMultiply,
        horizontal, vertical, 160, 300, "Soft beam mask")

    color = vector_parameter(
        material, "GuidanceColor",
        unreal.LinearColor(1.0, 0.28, 0.015, 1.0), 100, -300)
    strength = scalar_parameter(
        material, "EmissiveStrength", 3.25, 100, -160)
    opacity = scalar_parameter(
        material, "Opacity", 0.32, 100, 720)
    color_strength = binary(
        material, unreal.MaterialExpressionMultiply,
        color, strength, 370, -230)
    emissive = binary(
        material, unreal.MaterialExpressionMultiply,
        color_strength, beam_mask, 620, 0)
    final_opacity = binary(
        material, unreal.MaterialExpressionMultiply,
        beam_mask, opacity, 620, 500)

    connect_property(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    connect_property(final_opacity, unreal.MaterialProperty.MP_OPACITY)
    finalize(material)
    return material


def author_core_material():
    """A masked core gives the beacon a saturated silhouette in daylight.

    Unlike the translucent halo, this is depth-writing and cheap to render;
    it keeps the gold recognisable against a bright road or sky.
    """
    material = load_or_create_material(CORE_PATH)
    material.modify()
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("opacity_mask_clip_value", 0.08)

    uv = expression(
        material, unreal.MaterialExpressionTextureCoordinate,
        -1150, 200, "Beam-core UV")
    u = expression(material, unreal.MaterialExpressionComponentMask, -950, 80)
    u.set_editor_properties({"r": True, "g": False, "b": False, "a": False})
    connect(uv, u, "Input", "")
    v = expression(material, unreal.MaterialExpressionComponentMask, -950, 400)
    v.set_editor_properties({"r": False, "g": True, "b": False, "a": False})
    connect(uv, v, "Input", "")

    centered = binary(
        material, unreal.MaterialExpressionSubtract,
        u, scalar(material, 0.5, -950, -100), -760, 50)
    absolute = unary(material, unreal.MaterialExpressionAbs, centered, -570, 50)
    narrowed = binary(
        material, unreal.MaterialExpressionMultiply,
        absolute, scalar(material, 3.1, -570, -110), -380, 50)
    horizontal = unary(
        material, unreal.MaterialExpressionSaturate,
        unary(material, unreal.MaterialExpressionOneMinus, narrowed, -190, 50), 0, 50)
    bottom = unary(
        material, unreal.MaterialExpressionSaturate,
        binary(material, unreal.MaterialExpressionMultiply,
               v, scalar(material, 7.0, -570, 500), -380, 400), -190, 400)
    top = unary(
        material, unreal.MaterialExpressionSaturate,
        binary(material, unreal.MaterialExpressionMultiply,
               unary(material, unreal.MaterialExpressionOneMinus, v, -570, 650),
               scalar(material, 2.7, -380, 710), -190, 600), 0, 600)
    mask = binary(
        material, unreal.MaterialExpressionMultiply,
        binary(material, unreal.MaterialExpressionMultiply,
               horizontal, bottom, 210, 220), top, 400, 220,
        "Tapered beacon core mask")

    color = vector_parameter(
        material, "GuidanceColor",
        unreal.LinearColor(1.0, 0.28, 0.015, 1.0), 160, -170)
    strength = scalar_parameter(
        material, "EmissiveStrength", 5.0, 160, -50)
    emissive = binary(
        material, unreal.MaterialExpressionMultiply,
        binary(material, unreal.MaterialExpressionMultiply,
               color, strength, 390, -120), mask, 600, -40,
        "High-contrast saturated gold core")
    connect_property(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    connect_property(mask, unreal.MaterialProperty.MP_OPACITY_MASK)
    finalize(material)
    return material


def run():
    ensure_directory(MATERIAL_DIR)
    author_gold_material()
    author_beam_material()
    author_core_material()
    unreal.log("{} Authored gold route, masked beam core, and soft halo materials.".format(LOG))


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
