"""Discover and author VHV World-Aligned materials in Unreal Editor 5.7.

The safe default is a report-only pass. Run this file from Unreal Editor's
Tools > Execute Python Script command. It never modifies assets below /Game/Fab.
"""

import datetime
import hashlib
import json
import os
import re
import traceback

import unreal


# -----------------------------------------------------------------------------
# Configuration. The first execution must keep these defaults.
# -----------------------------------------------------------------------------

DRY_RUN = False
UPDATE_MESH_ASSIGNMENTS = False
ROLLBACK_MESH_ASSIGNMENTS = False

# Empty means: use the newest backup in Saved/VHV_WorldAligned/Backups.
ROLLBACK_BACKUP_PATH = ""

FAB_ROOT = "/Game/Fab"
FAB_SURFACES_ROOT = "/Game/Fab/Megascans/Surfaces"
OUTPUT_ROOT = "/Game/Materials"
MODELS_ROOT = "/Game/3D_Models"
REFERENCE_MATERIAL_PATH = "/Game/Materials/M_VHV_WorldAligned_Wood"
MASTER_PATH = "/Game/Materials/M_VHV_WorldAligned_Master"
MASTER_NAME = "M_VHV_WorldAligned_Master"
MR_FACTOR_MASTER_PATH = "/Game/Materials/M_VHV_WorldAligned_MRFactor_Master"
MR_FACTOR_MASTER_NAME = "M_VHV_WorldAligned_MRFactor_Master"

BASE_PARAMETER = "BaseColor_Texture"
NORMAL_PARAMETER = "Normal_Texture"
ORM_PARAMETER = "ORM_Texture"
SIZE_PARAMETER = "TextureSize"
BASE_FACTOR_PARAMETER = "BaseColor_Factor"
MR_PARAMETER = "MetallicRoughness_Texture"
DEFAULT_TEXTURE_SIZE_CM = (100.0, 100.0, 100.0)

WORLD_ALIGNED_TEXTURE_PATH = (
    "/Engine/Functions/Engine_MaterialFunctions01/Texturing/"
    "WorldAlignedTexture.WorldAlignedTexture"
)
WORLD_ALIGNED_NORMAL_PATH = (
    "/Engine/Functions/Engine_MaterialFunctions01/Texturing/"
    "WorldAlignedNormal.WorldAlignedNormal"
)

LOG_PREFIX = "[VHV WorldAligned]"
REPORT_FOLDER_NAME = "Reports"
BACKUP_FOLDER_NAME = "Backups"

TEXTURE_SUFFIXES = {
    "base": ("_B", "_BASECOLOR", "_BASE_COLOR", "_ALBEDO", "_DIFFUSE"),
    "normal": ("_N", "_NORMAL"),
    "orm": ("_ORM", "_OCCLUSIONROUGHNESSMETALLIC"),
}

EXCLUDED_WORDS = (
    "glass", "window", "transluc", "foliage", "leaf", "leaves", "plant",
    "grass", "flower", "fern", "tree", "branch", "bark atlas", "opacity",
    "masked", "decal", "layer", "blendmaterial", "billboard",
)


def _path_name(obj):
    return obj.get_path_name() if obj else ""


def _asset_path(asset_data):
    return str(asset_data.package_name)


def _class_name(asset_data):
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        try:
            return str(asset_data.asset_class)
        except Exception:
            return ""


def _safe_get(obj, property_name, default=None):
    try:
        return obj.get_editor_property(property_name)
    except Exception:
        return default


def _enum_text(value):
    return str(value).split(".")[-1].upper()


def _is_opaque(material_interface):
    base = material_interface
    visited = set()
    while base and not isinstance(base, unreal.Material):
        path = _path_name(base)
        if path in visited:
            return False, "cyclic material parent chain"
        visited.add(path)
        base = _safe_get(base, "parent")
    if not base:
        return False, "base material could not be resolved"
    blend_mode = _safe_get(base, "blend_mode")
    if blend_mode is None:
        return False, "blend mode unavailable"
    if "OPAQUE" not in _enum_text(blend_mode):
        return False, "non-opaque blend mode {}".format(_enum_text(blend_mode))
    return True, ""


def _excluded_by_name(path):
    lowered = path.lower().replace("_", " ").replace("-", " ")
    for word in EXCLUDED_WORDS:
        if word in lowered:
            return "excluded category keyword '{}'".format(word)
    return ""


def _texture_role(texture_name):
    upper = texture_name.upper()
    for role, suffixes in TEXTURE_SUFFIXES.items():
        for suffix in sorted(suffixes, key=len, reverse=True):
            if upper.endswith(suffix):
                return role, texture_name[:-len(suffix)]
    return None, None


def _texture_compatible(texture, role):
    if not texture or not isinstance(texture, unreal.Texture2D):
        return False, "not a Texture2D"
    srgb = bool(_safe_get(texture, "srgb", False))
    compression = _enum_text(_safe_get(texture, "compression_settings", ""))
    if role == "base" and not srgb:
        return False, "base color texture is not sRGB"
    if role == "normal":
        if srgb:
            return False, "normal texture is sRGB"
        if "NORMALMAP" not in compression and "NORMAL" not in compression:
            return False, "normal texture compression is not Normalmap"
    if role == "orm" and srgb:
        return False, "ORM texture is sRGB"
    return True, ""


def _material_textures(material_interface):
    textures = []
    if isinstance(material_interface, unreal.Material):
        try:
            textures.extend(unreal.MaterialEditingLibrary.get_used_textures(material_interface))
        except Exception:
            pass
    else:
        try:
            parameter_names = unreal.MaterialEditingLibrary.get_texture_parameter_names(
                material_interface)
            for parameter_name in parameter_names:
                texture = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(
                    material_interface, parameter_name)
                if texture:
                    textures.append(texture)
        except Exception:
            # Direct overrides still give useful evidence if inherited lookup fails.
            for parameter in _safe_get(material_interface, "texture_parameter_values", []) or []:
                texture = _safe_get(parameter, "parameter_value")
                if texture:
                    textures.append(texture)
    unique = {}
    for texture in textures:
        unique[_path_name(texture)] = texture
    return [unique[path] for path in sorted(unique)]


def _normalized_parameter_name(value):
    return re.sub(r"[^a-z0-9]", "", str(value).lower())


def _classify_mr_factor_material(material_interface):
    if not isinstance(material_interface, unreal.MaterialInstanceConstant):
        return None, ""

    direct_textures = {}
    for parameter in _safe_get(material_interface, "texture_parameter_values", []) or []:
        info = _safe_get(parameter, "parameter_info")
        name = _safe_get(info, "name", "") if info else ""
        texture = _safe_get(parameter, "parameter_value")
        if name and texture:
            direct_textures[_normalized_parameter_name(name)] = texture
    mr_texture = direct_textures.get("metallicroughnesstexture")
    if len(direct_textures) != 1 or not mr_texture or not mr_texture.get_name().upper().endswith("_MR"):
        return None, ""

    valid, reason = _texture_compatible(mr_texture, "orm")
    if not valid:
        return None, "MetallicRoughnessTexture: {}".format(reason)

    try:
        vector_names = unreal.MaterialEditingLibrary.get_vector_parameter_names(material_interface)
    except Exception:
        vector_names = []
    color_name = None
    aliases = {"basecolorfactor", "basecolortint"}
    for name in vector_names:
        if _normalized_parameter_name(name) in aliases:
            color_name = name
            break
    if color_name is None:
        return None, "MetallicRoughnessTexture found, but no unambiguous Base Color factor parameter"

    color = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
        material_interface, color_name)
    return {
        "profile": "gltf_mr_factor",
        "mr": mr_texture,
        "base_factor": (float(color.r), float(color.g), float(color.b), float(color.a)),
        "base_factor_source": str(color_name),
        "stem": mr_texture.get_name()[:-3].lower(),
        "ignored_textures": [],
    }, ""


def _classify_texture_set(textures, material_interface=None):
    by_role = {"base": [], "normal": [], "orm": []}
    ignored = []
    for texture in textures:
        role, stem = _texture_role(texture.get_name())
        if role:
            by_role[role].append((texture, stem.lower()))
        else:
            ignored.append(_path_name(texture))

    for role in ("base", "normal", "orm"):
        if len(by_role[role]) != 1:
            if material_interface:
                alternate, alternate_reason = _classify_mr_factor_material(material_interface)
                if alternate or alternate_reason:
                    return alternate, alternate_reason
            return None, "expected exactly one {} texture; found {}".format(
                role, len(by_role[role]))

    stems = {by_role[role][0][1] for role in by_role}
    if len(stems) != 1:
        return None, "texture suffixes do not resolve to one exact family stem"

    folders = {
        os.path.dirname(_path_name(by_role[role][0][0])).replace("\\", "/")
        for role in by_role
    }
    if len(folders) != 1:
        return None, "required textures are not in the same source folder"

    result = {role: by_role[role][0][0] for role in by_role}
    result["profile"] = "orm_texture"
    result["stem"] = next(iter(stems))
    result["ignored_textures"] = ignored
    for role in ("base", "normal", "orm"):
        valid, reason = _texture_compatible(result[role], role)
        if not valid:
            return None, "{}: {}".format(role, reason)
    return result, ""


def _signature(texture_set):
    if texture_set["profile"] == "gltf_mr_factor":
        return (
            "gltf_mr_factor", _path_name(texture_set["mr"]),
            *("{:.9g}".format(value) for value in texture_set["base_factor"]),
        )
    return tuple(_path_name(texture_set[role]) for role in ("base", "normal", "orm"))


def _sanitize_name(value):
    value = re.sub(r"^(MI_|M_|T_|MS_)+", "", value, flags=re.IGNORECASE)
    value = re.sub(r"[^A-Za-z0-9_]+", "_", value)
    value = re.sub(r"_+", "_", value).strip("_")
    if not value:
        value = "Surface"
    if value[0].isdigit():
        value = "Surface_" + value
    return value[:80]


def _instance_names(candidates):
    grouped = {}
    for candidate in candidates:
        preferred = _sanitize_name(candidate["surface_name"])
        grouped.setdefault(preferred.lower(), []).append(candidate)
    for same_name in grouped.values():
        same_name.sort(key=lambda item: item["signature"])
        collision = len(same_name) > 1
        for candidate in same_name:
            preferred = _sanitize_name(candidate["surface_name"])
            if collision:
                digest = hashlib.sha1("|".join(candidate["signature"]).encode("utf-8")).hexdigest()[:8]
                preferred = "{}_{}".format(preferred, digest)
            candidate["instance_name"] = "MI_VHV_WA_{}".format(preferred)
            candidate["instance_path"] = "{}/{}".format(
                OUTPUT_ROOT, candidate["instance_name"])


def _discover_assets():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    fab_assets = list(registry.get_assets_by_path(FAB_ROOT, recursive=True))
    fab_assets.sort(key=_asset_path)

    texture_objects = {}
    material_assets = []
    for asset_data in fab_assets:
        class_name = _class_name(asset_data)
        if class_name == "Texture2D":
            texture = asset_data.get_asset()
            if texture:
                texture_objects[_asset_path(asset_data)] = texture
        elif class_name in ("Material", "MaterialInstanceConstant"):
            material_assets.append(asset_data)

    candidates_by_signature = {}
    skipped = []
    represented_texture_paths = set()

    for asset_data in material_assets:
        source_path = _asset_path(asset_data)
        exclusion = _excluded_by_name(source_path)
        if exclusion:
            skipped.append({"asset": source_path, "reason": exclusion})
            continue
        material = asset_data.get_asset()
        if not material:
            skipped.append({"asset": source_path, "reason": "asset could not be loaded"})
            continue
        opaque, reason = _is_opaque(material)
        if not opaque:
            skipped.append({"asset": source_path, "reason": reason})
            continue
        texture_set, reason = _classify_texture_set(_material_textures(material), material)
        if not texture_set:
            skipped.append({"asset": source_path, "reason": reason})
            continue
        sig = _signature(texture_set)
        if texture_set["profile"] == "gltf_mr_factor":
            represented_texture_paths.add(_path_name(texture_set["mr"]))
        else:
            represented_texture_paths.update(sig)
        candidate = candidates_by_signature.get(sig)
        if not candidate:
            candidate = {
                "signature": sig,
                "textures": texture_set,
                "surface_name": material.get_name(),
                "source_materials": [],
                "discovery": "material references",
            }
            candidates_by_signature[sig] = candidate
        candidate["source_materials"].append(source_path)

    # Strict fallback for unreferenced surface texture families: exact stem and
    # exact folder only. It does not associate merely similar filenames.
    families = {}
    for path, texture in texture_objects.items():
        if not path.startswith(FAB_SURFACES_ROOT + "/") or path in represented_texture_paths:
            continue
        role, stem = _texture_role(texture.get_name())
        if not role:
            continue
        folder = os.path.dirname(path).replace("\\", "/")
        key = (folder, stem.lower())
        families.setdefault(key, {"base": [], "normal": [], "orm": []})[role].append(texture)

    for (folder, stem), family in sorted(families.items()):
        flat = family["base"] + family["normal"] + family["orm"]
        texture_set, reason = _classify_texture_set(flat)
        label = "{}/{}".format(folder, stem)
        if not texture_set:
            skipped.append({"asset": label, "reason": "texture-family fallback: " + reason})
            continue
        sig = _signature(texture_set)
        if sig in candidates_by_signature:
            continue
        candidates_by_signature[sig] = {
            "signature": sig,
            "textures": texture_set,
            "surface_name": stem,
            "source_materials": [],
            "discovery": "exact folder/stem texture family",
        }

    candidates = sorted(candidates_by_signature.values(), key=lambda item: item["signature"])
    for candidate in candidates:
        candidate["source_materials"].sort()
    _instance_names(candidates)
    return candidates, skipped, fab_assets


def _reference_summary():
    reference = unreal.EditorAssetLibrary.load_asset(REFERENCE_MATERIAL_PATH)
    if not reference or not isinstance(reference, unreal.Material):
        return None, {"found": False, "path": REFERENCE_MATERIAL_PATH}

    properties = {}
    for property_name in ("blend_mode", "shading_model", "two_sided", "tangent_space_normal"):
        value = _safe_get(reference, property_name)
        properties[property_name] = str(value)

    connections = {}
    material_properties = {
        "base_color": unreal.MaterialProperty.MP_BASE_COLOR,
        "normal": unreal.MaterialProperty.MP_NORMAL,
        "ambient_occlusion": unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,
        "roughness": unreal.MaterialProperty.MP_ROUGHNESS,
        "metallic": unreal.MaterialProperty.MP_METALLIC,
    }
    for label, material_property in material_properties.items():
        try:
            node = unreal.MaterialEditingLibrary.get_material_property_input_node(
                reference, material_property)
            output = unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
                reference, material_property)
            connections[label] = {
                "node_class": node.get_class().get_name() if node else "",
                "node_name": node.get_name() if node else "",
                "output": output,
            }
        except Exception as error:
            connections[label] = {"inspection_error": str(error)}

    textures = []
    try:
        textures = sorted(_path_name(t) for t in unreal.MaterialEditingLibrary.get_used_textures(reference))
    except Exception:
        pass
    return reference, {
        "found": True,
        "path": REFERENCE_MATERIAL_PATH,
        "expression_count": unreal.MaterialEditingLibrary.get_num_material_expressions(reference),
        "properties": properties,
        "connections": connections,
        "textures": textures,
    }


def _ensure_output_directory():
    if not unreal.EditorAssetLibrary.does_directory_exist(OUTPUT_ROOT):
        if not unreal.EditorAssetLibrary.make_directory(OUTPUT_ROOT):
            raise RuntimeError("Could not create {}".format(OUTPUT_ROOT))


def _new_expression(material, expression_class, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y)
    if not expression:
        raise RuntimeError("Could not create {}".format(expression_class.__name__))
    return expression


def _connect(source, output_name, target, input_name):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
            source, output_name, target, input_name):
        raise RuntimeError(
            "Could not connect {}:{} to {}:{}".format(
                source.get_name(), output_name, target.get_name(), input_name))


def _connect_property(source, output_name, material_property):
    if not unreal.MaterialEditingLibrary.connect_material_property(
            source, output_name, material_property):
        raise RuntimeError(
            "Could not connect {}:{} to {}".format(
                source.get_name(), output_name, material_property))


def _build_master(reference, default_textures):
    existing = unreal.EditorAssetLibrary.load_asset(MASTER_PATH)
    if existing:
        if not isinstance(existing, unreal.Material):
            raise RuntimeError("{} exists but is not a Material".format(MASTER_PATH))
        required_texture_names = {
            str(name) for name in unreal.MaterialEditingLibrary.get_texture_parameter_names(existing)
        }
        required_vector_names = {
            str(name) for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(existing)
        }
        missing = {BASE_PARAMETER, NORMAL_PARAMETER, ORM_PARAMETER} - required_texture_names
        if missing or SIZE_PARAMETER not in required_vector_names:
            raise RuntimeError(
                "Existing master is not the generated VHV schema; missing parameters: {}".format(
                    sorted(missing | ({SIZE_PARAMETER} if SIZE_PARAMETER not in required_vector_names else set()))))
        if bool(_safe_get(existing, "tangent_space_normal", True)):
            existing.modify()
            existing.set_editor_property("tangent_space_normal", False)
            unreal.MaterialEditingLibrary.recompile_material(existing)
            if not unreal.EditorAssetLibrary.save_loaded_asset(existing, only_if_is_dirty=False):
                raise RuntimeError("Could not save corrected normal-space setting on {}".format(MASTER_PATH))
        return existing, False

    factory = unreal.MaterialFactoryNew()
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MASTER_NAME, OUTPUT_ROOT, unreal.Material, factory)
    if not material:
        raise RuntimeError("Could not create {}".format(MASTER_PATH))

    material.modify()
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("tangent_space_normal", False)
    reference_shading_model = _safe_get(reference, "shading_model")
    if reference_shading_model is not None:
        material.set_editor_property("shading_model", reference_shading_model)

    wa_texture = unreal.load_asset(WORLD_ALIGNED_TEXTURE_PATH)
    wa_normal = unreal.load_asset(WORLD_ALIGNED_NORMAL_PATH)
    if not wa_texture or not wa_normal:
        raise RuntimeError("Engine WorldAligned material functions could not be loaded")

    base_parameter = _new_expression(
        material, unreal.MaterialExpressionTextureObjectParameter, -1100, -500)
    base_parameter.set_editor_properties({
        "parameter_name": BASE_PARAMETER,
        "texture": default_textures["base"],
        "sampler_type": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    })
    normal_parameter = _new_expression(
        material, unreal.MaterialExpressionTextureObjectParameter, -1100, -100)
    normal_parameter.set_editor_properties({
        "parameter_name": NORMAL_PARAMETER,
        "texture": default_textures["normal"],
        "sampler_type": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
    })
    orm_parameter = _new_expression(
        material, unreal.MaterialExpressionTextureObjectParameter, -1100, 300)
    orm_parameter.set_editor_properties({
        "parameter_name": ORM_PARAMETER,
        "texture": default_textures["orm"],
        "sampler_type": unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
    })

    size_parameter = _new_expression(
        material, unreal.MaterialExpressionVectorParameter, -1100, 700)
    size_parameter.set_editor_properties({
        "parameter_name": SIZE_PARAMETER,
        "default_value": unreal.LinearColor(
            DEFAULT_TEXTURE_SIZE_CM[0], DEFAULT_TEXTURE_SIZE_CM[1],
            DEFAULT_TEXTURE_SIZE_CM[2], 1.0),
    })
    size_rgb = _new_expression(material, unreal.MaterialExpressionComponentMask, -850, 700)
    size_rgb.set_editor_properties({"r": True, "g": True, "b": True, "a": False})
    _connect(size_parameter, "", size_rgb, "")

    base_call = _new_expression(
        material, unreal.MaterialExpressionMaterialFunctionCall, -600, -500)
    normal_call = _new_expression(
        material, unreal.MaterialExpressionMaterialFunctionCall, -600, -100)
    orm_call = _new_expression(
        material, unreal.MaterialExpressionMaterialFunctionCall, -600, 300)
    if not base_call.set_material_function(wa_texture):
        raise RuntimeError("Could not set WorldAlignedTexture on Base Color function call")
    if not normal_call.set_material_function(wa_normal):
        raise RuntimeError("Could not set WorldAlignedNormal on Normal function call")
    if not orm_call.set_material_function(wa_texture):
        raise RuntimeError("Could not set WorldAlignedTexture on ORM function call")

    _connect(base_parameter, "", base_call, "TextureObject")
    _connect(normal_parameter, "", normal_call, "TextureObject")
    _connect(orm_parameter, "", orm_call, "TextureObject")
    for function_call in (base_call, normal_call, orm_call):
        _connect(size_rgb, "", function_call, "TextureSize")

    ao_mask = _new_expression(material, unreal.MaterialExpressionComponentMask, -150, 180)
    roughness_mask = _new_expression(material, unreal.MaterialExpressionComponentMask, -150, 330)
    metallic_mask = _new_expression(material, unreal.MaterialExpressionComponentMask, -150, 480)
    ao_mask.set_editor_properties({"r": True, "g": False, "b": False, "a": False})
    roughness_mask.set_editor_properties({"r": False, "g": True, "b": False, "a": False})
    metallic_mask.set_editor_properties({"r": False, "g": False, "b": True, "a": False})
    for mask in (ao_mask, roughness_mask, metallic_mask):
        _connect(orm_call, "XYZ Texture", mask, "")

    _connect_property(base_call, "XYZ Texture", unreal.MaterialProperty.MP_BASE_COLOR)
    _connect_property(normal_call, "XYZ Texture", unreal.MaterialProperty.MP_NORMAL)
    _connect_property(ao_mask, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    _connect_property(roughness_mask, "", unreal.MaterialProperty.MP_ROUGHNESS)
    _connect_property(metallic_mask, "", unreal.MaterialProperty.MP_METALLIC)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(MASTER_PATH))
    return material, True


def _build_mr_factor_master(reference, default_textures):
    existing = unreal.EditorAssetLibrary.load_asset(MR_FACTOR_MASTER_PATH)
    if existing:
        if not isinstance(existing, unreal.Material):
            raise RuntimeError("{} exists but is not a Material".format(MR_FACTOR_MASTER_PATH))
        texture_names = {
            str(name) for name in unreal.MaterialEditingLibrary.get_texture_parameter_names(existing)
        }
        vector_names = {
            str(name) for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(existing)
        }
        if MR_PARAMETER not in texture_names or not {BASE_FACTOR_PARAMETER, SIZE_PARAMETER}.issubset(vector_names):
            raise RuntimeError("Existing MR-factor master is not the generated VHV schema")
        if bool(_safe_get(existing, "tangent_space_normal", True)):
            existing.modify()
            existing.set_editor_property("tangent_space_normal", False)
            unreal.MaterialEditingLibrary.recompile_material(existing)
            if not unreal.EditorAssetLibrary.save_loaded_asset(existing, only_if_is_dirty=False):
                raise RuntimeError("Could not save corrected normal-space setting on {}".format(
                    MR_FACTOR_MASTER_PATH))
        return existing, False

    factory = unreal.MaterialFactoryNew()
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MR_FACTOR_MASTER_NAME, OUTPUT_ROOT, unreal.Material, factory)
    if not material:
        raise RuntimeError("Could not create {}".format(MR_FACTOR_MASTER_PATH))
    material.modify()
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("tangent_space_normal", False)
    reference_shading_model = _safe_get(reference, "shading_model")
    if reference_shading_model is not None:
        material.set_editor_property("shading_model", reference_shading_model)

    wa_texture = unreal.load_asset(WORLD_ALIGNED_TEXTURE_PATH)
    if not wa_texture:
        raise RuntimeError("Engine WorldAlignedTexture material function could not be loaded")

    base_factor = _new_expression(material, unreal.MaterialExpressionVectorParameter, -1000, -300)
    base_factor.set_editor_properties({
        "parameter_name": BASE_FACTOR_PARAMETER,
        "default_value": unreal.LinearColor(*default_textures["base_factor"]),
    })
    mr_parameter = _new_expression(
        material, unreal.MaterialExpressionTextureObjectParameter, -1000, 100)
    mr_parameter.set_editor_properties({
        "parameter_name": MR_PARAMETER,
        "texture": default_textures["mr"],
        "sampler_type": unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
    })
    size_parameter = _new_expression(material, unreal.MaterialExpressionVectorParameter, -1000, 500)
    size_parameter.set_editor_properties({
        "parameter_name": SIZE_PARAMETER,
        "default_value": unreal.LinearColor(*DEFAULT_TEXTURE_SIZE_CM, 1.0),
    })
    size_rgb = _new_expression(material, unreal.MaterialExpressionComponentMask, -750, 500)
    size_rgb.set_editor_properties({"r": True, "g": True, "b": True, "a": False})
    _connect(size_parameter, "", size_rgb, "")

    mr_call = _new_expression(
        material, unreal.MaterialExpressionMaterialFunctionCall, -550, 100)
    if not mr_call.set_material_function(wa_texture):
        raise RuntimeError("Could not set WorldAlignedTexture on MetallicRoughness function call")
    _connect(mr_parameter, "", mr_call, "TextureObject")
    _connect(size_rgb, "", mr_call, "TextureSize")

    roughness_mask = _new_expression(material, unreal.MaterialExpressionComponentMask, -150, 50)
    metallic_mask = _new_expression(material, unreal.MaterialExpressionComponentMask, -150, 200)
    roughness_mask.set_editor_properties({"r": False, "g": True, "b": False, "a": False})
    metallic_mask.set_editor_properties({"r": False, "g": False, "b": True, "a": False})
    _connect(mr_call, "XYZ Texture", roughness_mask, "")
    _connect(mr_call, "XYZ Texture", metallic_mask, "")
    _connect_property(base_factor, "", unreal.MaterialProperty.MP_BASE_COLOR)
    _connect_property(roughness_mask, "", unreal.MaterialProperty.MP_ROUGHNESS)
    _connect_property(metallic_mask, "", unreal.MaterialProperty.MP_METALLIC)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(MR_FACTOR_MASTER_PATH))
    return material, True


def _set_instance_parent_checked(instance, parent):
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
    unreal.MaterialEditingLibrary.update_material_instance(instance)
    if _path_name(_safe_get(instance, "parent")) != _path_name(parent):
        raise RuntimeError("Could not assign parent {}".format(_path_name(parent)))


def _set_instance_texture_checked(instance, parameter_name, texture):
    # UE 5.7 performs this write but MaterialEditingLibrary incorrectly returns false.
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
        instance, parameter_name, texture)
    actual = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(
        instance, parameter_name)
    if _path_name(actual) != _path_name(texture):
        raise RuntimeError("Could not set {} on {}".format(
            parameter_name, _path_name(instance)))


def _set_instance_vector_checked(instance, parameter_name, value):
    # The UE 5.7 vector setter has the same broken return value; verify the write.
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        instance, parameter_name, value)
    actual = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
        instance, parameter_name)
    expected_values = (value.r, value.g, value.b, value.a)
    actual_values = (actual.r, actual.g, actual.b, actual.a)
    if any(abs(float(actual_value) - float(expected_value)) > 0.0001
           for actual_value, expected_value in zip(actual_values, expected_values)):
        raise RuntimeError("Could not set {} on {}".format(
            parameter_name, _path_name(instance)))


def _ensure_instance(candidate, master):
    path = candidate["instance_path"]
    instance = unreal.EditorAssetLibrary.load_asset(path)
    created = False
    if instance:
        if not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError("{} exists but is not a MaterialInstanceConstant".format(path))
    else:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            candidate["instance_name"], OUTPUT_ROOT,
            unreal.MaterialInstanceConstant, factory)
        if not instance:
            raise RuntimeError("Could not create {}".format(path))
        created = True

    instance.modify()
    _set_instance_parent_checked(instance, master)
    texture_set = candidate["textures"]
    if texture_set["profile"] == "gltf_mr_factor":
        setters = ((MR_PARAMETER, texture_set["mr"]),)
    else:
        setters = (
            (BASE_PARAMETER, texture_set["base"]),
            (NORMAL_PARAMETER, texture_set["normal"]),
            (ORM_PARAMETER, texture_set["orm"]),
    )
    for parameter_name, texture in setters:
        _set_instance_texture_checked(instance, parameter_name, texture)
    _set_instance_vector_checked(
        instance, SIZE_PARAMETER,
        unreal.LinearColor(*DEFAULT_TEXTURE_SIZE_CM, 1.0))
    if texture_set["profile"] == "gltf_mr_factor":
        _set_instance_vector_checked(
            instance, BASE_FACTOR_PARAMETER,
            unreal.LinearColor(*texture_set["base_factor"]))
    unreal.MaterialEditingLibrary.update_material_instance(instance)
    if not unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(path))
    return instance, created


def _material_signature(material_interface):
    if not material_interface:
        return None
    texture_set, _ = _classify_texture_set(_material_textures(material_interface), material_interface)
    return _signature(texture_set) if texture_set else None


def _planned_mesh_changes(candidates):
    by_source = {}
    by_signature = {}
    for candidate in candidates:
        for source in candidate["source_materials"]:
            by_source[source] = candidate
        by_signature.setdefault(candidate["signature"], []).append(candidate)

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    mesh_assets = list(registry.get_assets_by_path(MODELS_ROOT, recursive=True))
    mesh_assets = [asset for asset in mesh_assets if _class_name(asset) == "StaticMesh"]
    mesh_assets.sort(key=_asset_path)
    changes = []
    mesh_skips = []

    for asset_data in mesh_assets:
        mesh = asset_data.get_asset()
        if not mesh:
            mesh_skips.append({"asset": _asset_path(asset_data), "reason": "mesh could not be loaded"})
            continue
        slots = _safe_get(mesh, "static_materials", []) or []
        for index, slot in enumerate(slots):
            material = _safe_get(slot, "material_interface")
            original_path = _path_name(material)
            if not material:
                continue
            if original_path == REFERENCE_MATERIAL_PATH or original_path.startswith(OUTPUT_ROOT + "/MI_VHV_WA_"):
                continue
            exclusion = _excluded_by_name(original_path)
            if exclusion:
                continue

            candidate = by_source.get(original_path)
            match_method = "source material"
            if not candidate:
                sig = _material_signature(material)
                matches = by_signature.get(sig, []) if sig else []
                if len(matches) == 1:
                    candidate = matches[0]
                    match_method = "verified texture signature"
                elif len(matches) > 1:
                    mesh_skips.append({
                        "asset": "{}[{}]".format(_asset_path(asset_data), index),
                        "reason": "ambiguous texture-signature match",
                    })
            if not candidate:
                continue
            changes.append({
                "mesh": _asset_path(asset_data),
                "slot_index": index,
                "slot_name": str(_safe_get(slot, "material_slot_name", "")),
                "original_material": original_path,
                "replacement_material": candidate["instance_path"],
                "match_method": match_method,
            })
    return changes, mesh_skips, len(mesh_assets)


def _saved_root():
    return os.path.normpath(os.path.join(unreal.Paths.project_saved_dir(), "VHV_WorldAligned"))


def _write_json(folder_name, file_name, data):
    folder = os.path.join(_saved_root(), folder_name)
    os.makedirs(folder, exist_ok=True)
    path = os.path.normpath(os.path.join(folder, file_name))
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
    return path


def _apply_mesh_changes(changes, timestamp):
    backup = {
        "schema": 1,
        "created_utc": timestamp,
        "project": unreal.Paths.get_project_file_path(),
        "changes": changes,
    }
    backup_path = _write_json(
        BACKUP_FOLDER_NAME, "VHV_WA_Assignments_{}.json".format(timestamp), backup)

    by_mesh = {}
    for change in changes:
        by_mesh.setdefault(change["mesh"], []).append(change)
    changed_slots = 0
    changed_meshes = 0
    errors = []
    for mesh_path, mesh_changes in sorted(by_mesh.items()):
        mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
        if not mesh or not isinstance(mesh, unreal.StaticMesh):
            errors.append("Could not load StaticMesh {}".format(mesh_path))
            continue
        mesh.modify()
        mesh_changed = False
        for change in sorted(mesh_changes, key=lambda item: item["slot_index"]):
            replacement = unreal.EditorAssetLibrary.load_asset(change["replacement_material"])
            slots = _safe_get(mesh, "static_materials", []) or []
            index = change["slot_index"]
            current = _safe_get(slots[index], "material_interface") if index < len(slots) else None
            if _path_name(current) != change["original_material"]:
                errors.append("Skipped changed slot {}[{}]".format(mesh_path, index))
                continue
            if not replacement:
                errors.append("Missing replacement {}".format(change["replacement_material"]))
                continue
            mesh.set_material(index, replacement)
            changed_slots += 1
            mesh_changed = True
        if mesh_changed:
            if unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
                changed_meshes += 1
            else:
                errors.append("Could not save {}".format(mesh_path))
    return backup_path, changed_meshes, changed_slots, errors


def _newest_backup():
    folder = os.path.join(_saved_root(), BACKUP_FOLDER_NAME)
    if not os.path.isdir(folder):
        return ""
    files = [os.path.join(folder, name) for name in os.listdir(folder) if name.endswith(".json")]
    return max(files, key=os.path.getmtime) if files else ""


def _rollback():
    backup_path = os.path.normpath(ROLLBACK_BACKUP_PATH) if ROLLBACK_BACKUP_PATH else _newest_backup()
    if not backup_path or not os.path.isfile(backup_path):
        raise RuntimeError("No assignment backup was found for rollback")
    with open(backup_path, "r", encoding="utf-8") as handle:
        backup = json.load(handle)
    restored_slots = 0
    restored_meshes = 0
    errors = []
    by_mesh = {}
    for change in backup.get("changes", []):
        by_mesh.setdefault(change["mesh"], []).append(change)
    for mesh_path, mesh_changes in sorted(by_mesh.items()):
        mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
        if not mesh or not isinstance(mesh, unreal.StaticMesh):
            errors.append("Could not load StaticMesh {}".format(mesh_path))
            continue
        mesh.modify()
        mesh_changed = False
        for change in sorted(mesh_changes, key=lambda item: item["slot_index"]):
            original = unreal.EditorAssetLibrary.load_asset(change["original_material"])
            slots = _safe_get(mesh, "static_materials", []) or []
            index = change["slot_index"]
            current = _safe_get(slots[index], "material_interface") if index < len(slots) else None
            # Do not overwrite a manual edit made after generation.
            if _path_name(current) != change["replacement_material"]:
                errors.append("Skipped non-generated current slot {}[{}]".format(mesh_path, index))
                continue
            if not original:
                errors.append("Missing original material {}".format(change["original_material"]))
                continue
            mesh.set_material(index, original)
            restored_slots += 1
            mesh_changed = True
        if mesh_changed:
            if unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
                restored_meshes += 1
            else:
                errors.append("Could not save {}".format(mesh_path))
    return backup_path, restored_meshes, restored_slots, errors


def _candidate_report(candidate):
    texture_set = candidate["textures"]
    if texture_set["profile"] == "gltf_mr_factor":
        textures = {"metallic_roughness": _path_name(texture_set["mr"])}
        channels = {"green": "Roughness", "blue": "Metallic", "ambient_occlusion": "material default"}
    else:
        textures = {
            role: _path_name(texture_set[role])
            for role in ("base", "normal", "orm")
        }
        channels = {"red": "Ambient Occlusion", "green": "Roughness", "blue": "Metallic"}
    result = {
        "instance_path": candidate["instance_path"],
        "source_materials": candidate["source_materials"],
        "discovery": candidate["discovery"],
        "profile": texture_set["profile"],
        "textures": textures,
        "packed_channels": channels,
        "texture_size_cm": list(DEFAULT_TEXTURE_SIZE_CM),
    }
    if texture_set["profile"] == "gltf_mr_factor":
        result["base_color_factor"] = list(texture_set["base_factor"])
        result["base_color_factor_source"] = texture_set["base_factor_source"]
    return result


def run():
    timestamp = datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
    if ROLLBACK_MESH_ASSIGNMENTS:
        if DRY_RUN:
            raise RuntimeError("Set DRY_RUN = False to perform rollback")
        backup_path, meshes, slots, errors = _rollback()
        report = {
            "mode": "rollback",
            "backup_path": backup_path,
            "restored_meshes": meshes,
            "restored_slots": slots,
            "errors": errors,
        }
        report_path = _write_json(
            REPORT_FOLDER_NAME, "VHV_WorldAligned_Rollback_{}.json".format(timestamp), report)
        unreal.log("{} Rollback: {} meshes, {} slots; report {}".format(
            LOG_PREFIX, meshes, slots, report_path))
        return report

    candidates, skipped, fab_assets = _discover_assets()
    reference, reference_info = _reference_summary()
    mesh_changes, mesh_skips, mesh_count = _planned_mesh_changes(candidates)
    report = {
        "mode": "dry_run" if DRY_RUN else "apply",
        "timestamp_utc": timestamp,
        "configuration": {
            "DRY_RUN": DRY_RUN,
            "UPDATE_MESH_ASSIGNMENTS": UPDATE_MESH_ASSIGNMENTS,
            "ROLLBACK_MESH_ASSIGNMENTS": ROLLBACK_MESH_ASSIGNMENTS,
        },
        "reference_material": reference_info,
        "fab_assets_scanned": len(fab_assets),
        "compatible_texture_sets": len(candidates),
        "candidates": [_candidate_report(candidate) for candidate in candidates],
        "skipped": skipped,
        "house_static_meshes_scanned": mesh_count,
        "planned_mesh_replacements": mesh_changes,
        "mesh_skips": mesh_skips,
        "created_master": False,
        "created_mr_factor_master": False,
        "instances_created": 0,
        "instances_updated": 0,
        "mesh_assignment_backup": "",
        "meshes_updated": 0,
        "slots_updated": 0,
        "errors": [],
        "limitations": [
            "Opaque Texture2D ORM sets are supported; the exact Fab MR-plus-BaseColor-factor convention is also supported without inventing missing textures.",
            "No texture assets are modified; incorrectly configured sRGB/normal compression sets are skipped.",
            "Appearance still requires visual validation in Unreal Editor on representative meshes.",
        ],
    }

    if not DRY_RUN:
        if not reference:
            raise RuntimeError("Verified reference material was not found at {}".format(
                REFERENCE_MATERIAL_PATH))
        if not candidates:
            raise RuntimeError("No compatible texture sets were discovered; no assets were changed")
        _ensure_output_directory()
        masters = {}
        orm_candidates = [
            candidate for candidate in candidates
            if candidate["textures"]["profile"] == "orm_texture"
        ]
        mr_factor_candidates = [
            candidate for candidate in candidates
            if candidate["textures"]["profile"] == "gltf_mr_factor"
        ]
        if orm_candidates:
            master, master_created = _build_master(reference, orm_candidates[0]["textures"])
            masters["orm_texture"] = master
            report["created_master"] = master_created
        if mr_factor_candidates:
            master, master_created = _build_mr_factor_master(
                reference, mr_factor_candidates[0]["textures"])
            masters["gltf_mr_factor"] = master
            report["created_mr_factor_master"] = master_created
        for candidate in candidates:
            try:
                _, created = _ensure_instance(
                    candidate, masters[candidate["textures"]["profile"]])
                if created:
                    report["instances_created"] += 1
                else:
                    report["instances_updated"] += 1
            except Exception as error:
                report["errors"].append("{}: {}".format(candidate["instance_path"], error))

        if UPDATE_MESH_ASSIGNMENTS:
            successful_paths = {
                candidate["instance_path"] for candidate in candidates
                if unreal.EditorAssetLibrary.does_asset_exist(candidate["instance_path"])
            }
            applicable = [
                change for change in mesh_changes
                if change["replacement_material"] in successful_paths
            ]
            backup_path, meshes, slots, errors = _apply_mesh_changes(applicable, timestamp)
            report["mesh_assignment_backup"] = backup_path
            report["meshes_updated"] = meshes
            report["slots_updated"] = slots
            report["errors"].extend(errors)

    report_path = _write_json(
        REPORT_FOLDER_NAME, "VHV_WorldAligned_{}.json".format(timestamp), report)
    unreal.log(
        "{} {}: {} compatible sets, {} skipped, {} planned slot replacements; report {}".format(
            LOG_PREFIX, report["mode"], len(candidates), len(skipped),
            len(mesh_changes), report_path))
    if not DRY_RUN:
        unreal.log(
            "{} Assets: master_created={}, instances_created={}, instances_updated={}; "
            "mesh_slots_updated={}; errors={}".format(
                LOG_PREFIX, report["created_master"], report["instances_created"],
                report["instances_updated"], report["slots_updated"],
                len(report["errors"])))
    return report


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG_PREFIX, traceback.format_exc()))
        raise
