"""Create a concrete Landscape paint layer without modifying the source assets."""

import traceback

import unreal


SOURCE_MASTER = "/Game/Landscape/Materials/M_Landscape_Height"
SOURCE_INSTANCE = "/Game/Example/Materials/M_Landscape_NaniteEx"
OUTPUT_ROOT = "/Game/VHV_Stuff/Materials/Landscape"
NEW_MASTER = OUTPUT_ROOT + "/M_VHV_Landscape_ConcreteRoad"
NEW_INSTANCE = OUTPUT_ROOT + "/MI_VHV_Landscape_Nanite_ConcreteRoad"
LAYER_INFO = OUTPUT_ROOT + "/LI_F_ConcreteRoad"
TARGET_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
FORBIDDEN_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout"
LAYER_NAME = "F_ConcreteRoad"
LOG = "[VHV Concrete Landscape]"

TEXTURES = {
    "Color F": "/Game/Fab/Megascans/Surfaces/Concrete_Pavement_wjwjcis/Medium/"
               "wjwjcis_tier_2/Textures/T_wjwjcis_2K_B",
    "Normal F": "/Game/Fab/Megascans/Surfaces/Concrete_Pavement_wjwjcis/Medium/"
                "wjwjcis_tier_2/Textures/T_wjwjcis_2K_N",
    "Mask F": "/Game/Fab/Megascans/Surfaces/Concrete_Pavement_wjwjcis/Medium/"
              "wjwjcis_tier_2/Textures/T_wjwjcis_2K_ORM",
}


def _asset(path, expected_class=None):
    value = unreal.EditorAssetLibrary.load_asset(path)
    if not value or (expected_class and not isinstance(value, expected_class)):
        raise RuntimeError("Missing or wrong asset type: {}".format(path))
    return value


def _package_path(value):
    return value.get_path_name().split(".", 1)[0] if value else ""


def _expressions(material):
    library = unreal.MaterialEditingLibrary
    if hasattr(library, "get_material_expressions"):
        return list(library.get_material_expressions(material))

    def owned_by_material(value):
        current = value
        for _ in range(8):
            try:
                current = current.get_outer()
            except Exception:
                return False
            if not current:
                return False
            if current == material:
                return True
        return False

    # UE 5.7's ObjectIterator includes MaterialExpressionComment objects, while
    # get_num_material_expressions() deliberately excludes those graph comments.
    return [value for value in unreal.ObjectIterator(unreal.MaterialExpression)
            if not isinstance(value, unreal.MaterialExpressionComment)
            and owned_by_material(value)]


def _layer_names(expression):
    return [str(layer.get_editor_property("layer_name"))
            for layer in expression.get_editor_property("layers")]


def _is_ae_layer_set(names):
    normalized = {"".join(character for character in name.lower() if character.isalnum())
                  for name in names}
    return all(any(value in normalized for value in (letter, "layer" + letter, "material" + letter))
               for letter in "abcde")


def _open_material(material):
    subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    if subsystem:
        subsystem.open_editor_for_assets([material])


def _inputs(material, expression):
    try:
        return list(unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
            material, expression) or [])
    except Exception as exc:
        raise RuntimeError("Could not inspect material connections: {}".format(exc))


def _ancestors(material, root):
    visited = set()
    pending = [root] if root else []
    while pending:
        expression = pending.pop()
        if not expression or expression in visited:
            continue
        visited.add(expression)
        pending.extend(_inputs(material, expression))
    return visited


def _find_final_blend(material):
    _open_material(material)
    expressions = _expressions(material)
    if len(expressions) != unreal.MaterialEditingLibrary.get_num_material_expressions(material):
        raise RuntimeError("Could not enumerate every material expression safely")

    blends = [expression for expression in expressions
              if isinstance(expression, unreal.MaterialExpressionLandscapeLayerBlend)
              and _is_ae_layer_set(_layer_names(expression))]
    if not blends:
        raise RuntimeError("No Landscape Layer Blend containing A-E was found")

    try:
        root = unreal.MaterialEditingLibrary.get_material_property_input_node(
            material, unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    except Exception as exc:
        raise RuntimeError("Material Attributes output cannot be inspected: {}".format(exc))
    if not root:
        raise RuntimeError("The master has no connected Material Attributes output")
    final_ancestors = _ancestors(material, root)
    candidates = [blend for blend in blends if blend in final_ancestors]
    if len(candidates) != 1:
        raise RuntimeError("Expected one final A-E attribute blend; found {}".format(len(candidates)))
    target = candidates[0]

    # In this source master the connected A-E LandscapeLayerBlend feeds one
    # typed input of MakeMaterialAttributes; it is not an attributes blend.
    # Appending an attributes-valued layer would create an invalid graph, so
    # reject that architecture before any output asset is duplicated.
    if isinstance(root, unreal.MaterialExpressionMakeMaterialAttributes) and target != root:
        raise RuntimeError(
            "The connected A-E LandscapeLayerBlend is a per-property blend upstream of "
            "MakeMaterialAttributes, not a Material Attributes blend. Layer F requires "
            "cloning the established per-property E branch; UE 5.7 Python does not expose "
            "the required input-pin source mapping safely.")

    rvt_nodes = [expression for expression in expressions
                 if expression.get_class().get_name() == "MaterialExpressionRuntimeVirtualTextureOutput"]
    for rvt in rvt_nodes:
        if _inputs(material, rvt) and target not in _ancestors(material, rvt):
            raise RuntimeError(
                "The final A-E blend does not feed Runtime Virtual Texture output {}".format(
                    rvt.get_name()))
    return target.get_name(), len(expressions), len(rvt_nodes)


def _parameter_snapshot(instance):
    library = unreal.MaterialEditingLibrary
    result = {}
    for kind, names_method, value_method in (
            ("texture", library.get_texture_parameter_names,
             library.get_material_instance_texture_parameter_value),
            ("scalar", library.get_scalar_parameter_names,
             library.get_material_instance_scalar_parameter_value),
            ("vector", library.get_vector_parameter_names,
             library.get_material_instance_vector_parameter_value),
            ("switch", library.get_static_switch_parameter_names,
             library.get_material_instance_static_switch_parameter_value)):
        values = {}
        for parameter_name in names_method(instance):
            name = str(parameter_name)
            value = value_method(instance, parameter_name)
            if kind == "texture":
                value = value.get_path_name() if value else ""
            elif kind == "vector":
                value = (float(value.r), float(value.g), float(value.b), float(value.a))
            elif kind == "scalar":
                value = float(value)
            else:
                value = bool(value)
            values[name] = value
        result[kind] = values
    return result


def _existing_parameters_preserved(before, after):
    for kind, values in before.items():
        for name, value in values.items():
            if name not in after.get(kind, {}) or after[kind][name] != value:
                return False
    return True


def _set(expression, name, value):
    expression.set_editor_property(name, value)


def _create(material, expression_class, x, y, description):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y)
    if not expression:
        raise RuntimeError("Could not create {}".format(expression_class.__name__))
    try:
        _set(expression, "desc", description)
    except Exception:
        pass
    return expression


def _connect(source, output_name, destination, input_names):
    for input_name in input_names:
        if unreal.MaterialEditingLibrary.connect_material_expressions(
                source, output_name, destination, input_name):
            return input_name
    raise RuntimeError("Connection failed: {} -> {} ({})".format(
        source.get_name(), destination.get_name(), ", ".join(input_names)))


def _find_expression(material, name):
    matches = [expression for expression in _expressions(material) if expression.get_name() == name]
    if len(matches) != 1:
        raise RuntimeError("Could not uniquely find duplicated expression {}".format(name))
    return matches[0]


def _append_layer_and_nodes(material, target_name, textures, size_default):
    target = _find_expression(material, target_name)
    layers = list(target.get_editor_property("layers"))
    if LAYER_NAME in [str(layer.get_editor_property("layer_name")) for layer in layers]:
        raise RuntimeError("{} already exists in the new master".format(LAYER_NAME))

    reference = next((layer for layer in reversed(layers)
                      if str(layer.get_editor_property("layer_name")).lower().endswith("e")), layers[-1])
    new_layer = unreal.LayerBlendInput()
    _set(new_layer, "layer_name", unreal.Name(LAYER_NAME))
    _set(new_layer, "blend_type", reference.get_editor_property("blend_type"))
    _set(new_layer, "preview_weight", 0.0)
    _set(new_layer, "const_height_input", 0.5)
    layers.append(new_layer)
    _set(target, "layers", layers)

    x, y = unreal.MaterialEditingLibrary.get_material_expression_node_position(target)
    world_position = _create(material, unreal.MaterialExpressionWorldPosition,
                             x - 1500, y + 500, "Material F world-space UV source")
    xy_mask = _create(material, unreal.MaterialExpressionComponentMask,
                      x - 1300, y + 500, "Material F XY projection")
    _set(xy_mask, "r", True)
    _set(xy_mask, "g", True)
    size = _create(material, unreal.MaterialExpressionScalarParameter,
                   x - 1300, y + 700, "Material F tiling in Unreal centimetres")
    _set(size, "parameter_name", unreal.Name("Size F"))
    _set(size, "default_value", float(size_default))
    try:
        _set(size, "group", unreal.Name("Material F"))
    except Exception:
        pass
    divide = _create(material, unreal.MaterialExpressionDivide,
                     x - 1050, y + 500, "Material F UV = Absolute World XY / Size F")
    _connect(world_position, "", xy_mask, ("Input", ""))
    _connect(xy_mask, "", divide, ("A", ""))
    _connect(size, "", divide, ("B",))

    samples = {}
    sample_specs = (
        ("Color F", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, y + 100),
        ("Normal F", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, y + 350),
        ("Mask F", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, y + 600),
    )
    for parameter_name, sampler_type, node_y in sample_specs:
        sample = _create(material, unreal.MaterialExpressionTextureSampleParameter2D,
                         x - 750, node_y, parameter_name)
        _set(sample, "parameter_name", unreal.Name(parameter_name))
        _set(sample, "texture", textures[parameter_name])
        _set(sample, "sampler_type", sampler_type)
        try:
            _set(sample, "group", unreal.Name("Material F"))
        except Exception:
            pass
        _connect(divide, "", sample, ("Coordinates", "UVs"))
        samples[parameter_name] = sample

    attributes = _create(material, unreal.MaterialExpressionMakeMaterialAttributes,
                         x - 350, y + 350, "Material F concrete attributes")
    _connect(samples["Color F"], "RGB", attributes, ("Base Color", "BaseColor"))
    _connect(samples["Normal F"], "RGB", attributes, ("Normal",))
    _connect(samples["Mask F"], "R", attributes, ("Ambient Occlusion", "AmbientOcclusion"))
    _connect(samples["Mask F"], "G", attributes, ("Roughness",))
    _connect(samples["Mask F"], "B", attributes, ("Metallic",))
    _connect(attributes, "", target, (LAYER_NAME,))
    return target, attributes


def _verify_compile(material):
    unreal.MaterialEditingLibrary.recompile_material(material)
    errors = None
    for property_name in ("compile_errors", "errors"):
        try:
            errors = list(material.get_editor_property(property_name))
            break
        except Exception:
            pass
    if errors:
        raise RuntimeError("Material compile errors: {}".format("; ".join(str(error) for error in errors)))
    shader_count = None
    if hasattr(unreal.MaterialEditingLibrary, "get_num_shader_types"):
        shader_count = int(unreal.MaterialEditingLibrary.get_num_shader_types(material))
        if shader_count <= 0:
            raise RuntimeError("Material compiled no shader types")
    return shader_count


def _assign_to_open_landscape(new_instance, source_instance):
    world_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = world_subsystem.get_editor_world() if world_subsystem else None
    world_path = _package_path(world)
    if not world or world_path != TARGET_MAP or world_path == FORBIDDEN_MAP:
        return "not assigned: target map is not the loaded editor world"
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    landscapes = [actor for actor in actor_subsystem.get_all_level_actors()
                  if isinstance(actor, unreal.LandscapeProxy) and actor.get_world() == world]
    if len(landscapes) != 1:
        return "not assigned: expected one Landscape actor, found {}".format(len(landscapes))
    landscape = landscapes[0]
    changed = []
    for property_name in ("landscape_material", "nanite_override_material"):
        try:
            current = landscape.get_editor_property(property_name)
        except Exception:
            continue
        if _package_path(current) == SOURCE_INSTANCE:
            landscape.modify()
            landscape.set_editor_property(property_name, new_instance)
            changed.append(property_name)
    if not changed:
        return "not assigned: no Landscape material field referenced the source instance"
    return "assigned in memory (level intentionally not saved): {}".format(", ".join(changed))


def _cleanup(created_paths):
    for path in reversed(created_paths):
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.EditorAssetLibrary.delete_asset(path)


def run():
    source_master = _asset(SOURCE_MASTER, unreal.Material)
    source_instance = _asset(SOURCE_INSTANCE, unreal.MaterialInstanceConstant)
    if _package_path(source_instance.get_editor_property("parent")) != SOURCE_MASTER:
        raise RuntimeError("Unexpected instance parent: {}".format(
            _package_path(source_instance.get_editor_property("parent"))))

    textures = {name: _asset(path, unreal.Texture2D) for name, path in TEXTURES.items()}
    if not textures["Color F"].get_editor_property("srgb"):
        raise RuntimeError("Concrete Base Color must be sRGB")
    if textures["Normal F"].get_editor_property("srgb"):
        raise RuntimeError("Concrete Normal must be linear")
    if textures["Mask F"].get_editor_property("srgb"):
        raise RuntimeError("Concrete ORM must be linear")

    target_name, source_expression_count, rvt_count = _find_final_blend(source_master)
    before_parameters = _parameter_snapshot(source_instance)
    if unreal.EditorAssetLibrary.does_asset_exist(NEW_MASTER) \
            or unreal.EditorAssetLibrary.does_asset_exist(NEW_INSTANCE):
        raise RuntimeError("Output assets already exist; refusing to overwrite or repair them")

    created = []
    try:
        unreal.EditorAssetLibrary.make_directory(OUTPUT_ROOT)
        new_master = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MASTER, NEW_MASTER)
        if not new_master:
            raise RuntimeError("Could not duplicate source master")
        created.append(NEW_MASTER)
        new_instance = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_INSTANCE, NEW_INSTANCE)
        if not new_instance:
            raise RuntimeError("Could not duplicate source instance")
        created.append(NEW_INSTANCE)

        if unreal.MaterialEditingLibrary.get_num_material_expressions(new_master) != source_expression_count:
            raise RuntimeError("Master duplication did not preserve every A-E expression")
        size_e = unreal.MaterialEditingLibrary.get_material_default_scalar_parameter_value(
            source_master, unreal.Name("Size E"))
        target, attributes = _append_layer_and_nodes(
            new_master, target_name, textures, max(float(size_e), 1.0))

        unreal.MaterialEditingLibrary.set_material_instance_parent(new_instance, new_master)
        if _package_path(new_instance.get_editor_property("parent")) != NEW_MASTER:
            raise RuntimeError("New material instance parent verification failed")
        if not _existing_parameters_preserved(
                before_parameters, _parameter_snapshot(new_instance)):
            raise RuntimeError("Existing A-E effective parameter values changed after reparenting")

        _open_material(new_master)
        layer_names = _layer_names(target)
        if layer_names.count(LAYER_NAME) != 1:
            raise RuntimeError("F layer was not added exactly once")
        if attributes not in _inputs(new_master, target):
            raise RuntimeError("F material attributes are not connected to the Landscape blend")
        root = unreal.MaterialEditingLibrary.get_material_property_input_node(
            new_master, unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
        if target not in _ancestors(new_master, root):
            raise RuntimeError("F Landscape blend does not reach the final Material Attributes output")
        for expression in _expressions(new_master):
            if expression.get_class().get_name() == "MaterialExpressionRuntimeVirtualTextureOutput":
                if _inputs(new_master, expression) and target not in _ancestors(new_master, expression):
                    raise RuntimeError("F Landscape blend does not reach all RVT outputs")

        shader_count = _verify_compile(new_master)
        if not unreal.EditorAssetLibrary.save_loaded_asset(new_master, False):
            raise RuntimeError("Could not save new master")
        if not unreal.EditorAssetLibrary.save_loaded_asset(new_instance, False):
            raise RuntimeError("Could not save new material instance")
        assignment = _assign_to_open_landscape(new_instance, source_instance)

        unreal.log("{} Success: master={}, instance={}, layer={}, expressions={}, RVTs={}, "
                   "shader_types={}, landscape={}".format(
                       LOG, NEW_MASTER, NEW_INSTANCE, LAYER_NAME,
                       len(_expressions(new_master)), rvt_count,
                       shader_count if shader_count is not None else "API unavailable", assignment))
        unreal.log_warning(
            "{} Layer Info assignment is not exposed safely in UE 5.7 Python. Create LI_F_ConcreteRoad "
            "from Landscape Paint mode; do not Fill Layer. The level was not saved.".format(LOG))
        return True
    except Exception:
        _cleanup(created)
        raise


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed; no partial output assets retained:\n{}".format(
            LOG, traceback.format_exc()))
        raise
