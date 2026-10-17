"""Create and verify four requested world-aligned Megascans material instances.

Default mode creates/updates only the four exact /Game/Materials instances below.
Pass -VHVVerifyRequestedSurfaceMIs for a read-only cold verification pass.
"""

import datetime
import json
import os
import traceback

import unreal


LOG = "[VHV Requested Surface MIs]"
SURFACE_ROOT = "/Game/Fab/Megascans/Surfaces"
OUTPUT_ROOT = "/Game/Materials"
MASTER_PATH = "/Game/Materials/M_VHV_WorldAligned_Master"
BASE = "BaseColor_Texture"
NORMAL = "Normal_Texture"
ORM = "ORM_Texture"
SIZE = "TextureSize"

REQUESTS = (
    {
        "token": "sexkaitb",
        "surface_hint": "Brick_Wall_Worn",
        "source_mi": "MI_sexkaitb",
        "output": "/Game/Materials/MI_VHV_WA_Brick_Worn",
        "resolution": "2K",
        "texture_size_cm": (100.0, 100.0, 100.0, 1.0),
    },
    {
        "token": "wjwjcis",
        "surface_hint": "Concrete_Pavement",
        "source_mi": "MI_wjwjcis",
        "output": "/Game/Materials/MI_VHV_WA_Concrete_Pavement",
        "resolution": "2K",
        "texture_size_cm": (150.0, 150.0, 150.0, 1.0),
    },
    {
        "token": "vjqifhu",
        "surface_hint": "Curated_Stone_Facade",
        "source_mi": "MI_vjqifhu",
        "output": "/Game/Materials/MI_VHV_WA_Stone_Facade",
        "resolution": "1K",
        "texture_size_cm": (150.0, 150.0, 150.0, 1.0),
    },
    {
        "token": "xbmgeba",
        "surface_hint": "Stone_Floor",
        "source_mi": "MI_xbmgeba",
        "output": "/Game/Materials/MI_VHV_WA_Stone_Floor",
        "resolution": "1K",
        "texture_size_cm": (100.0, 100.0, 100.0, 1.0),
    },
)


def _path(obj):
    return obj.get_path_name().split(".", 1)[0] if obj else ""


def _safe_get(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def _class_name(asset_data):
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        return str(asset_data.asset_class)


def _asset(path, expected_type=None):
    value = unreal.EditorAssetLibrary.load_asset(path)
    if not value:
        raise RuntimeError("Missing asset {}".format(path))
    if expected_type and not isinstance(value, expected_type):
        raise RuntimeError("{} has unexpected asset type".format(path))
    return value


def _mode():
    return "verify" if "-VHVVerifyRequestedSurfaceMIs" in str(
        unreal.SystemLibrary.get_command_line()) else "apply"


def _report_path(stamp, mode):
    folder = os.path.normpath(os.path.join(
        unreal.Paths.project_saved_dir(), "VHV_RequestedSurfaceInstances"))
    os.makedirs(folder, exist_ok=True)
    return os.path.join(folder, "VHV_RequestedSurfaceInstances_{}_{}.json".format(mode, stamp))


def _discover(request):
    """Discover source MI and texture assets from the registry, not folder spelling."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    token = request["token"].lower()
    candidates = list(registry.get_assets_by_path(SURFACE_ROOT, recursive=True))
    candidates = [item for item in candidates if token in str(item.package_name).lower()]

    source_mis = [item for item in candidates
                  if _class_name(item) == "MaterialInstanceConstant"
                  and str(item.asset_name) == request["source_mi"]]
    if len(source_mis) != 1:
        raise RuntimeError("{}: expected one source MI {}, found {}".format(
            token, request["source_mi"], len(source_mis)))
    source_mi = source_mis[0].get_asset()
    if not source_mi:
        raise RuntimeError("{}: could not load source MI".format(token))

    textures = [item for item in candidates if _class_name(item) in ("Texture2D", "Texture")]
    resolved = {}
    for role, suffix in (("base", "_B"), ("normal", "_N"), ("orm", "_ORM")):
        matches = [item for item in textures
                   if str(item.asset_name).upper().endswith(suffix)
                   and request["resolution"].upper() in str(item.asset_name).upper()]
        if len(matches) != 1:
            raise RuntimeError("{}: expected one {} {} texture, found {}".format(
                token, request["resolution"], role, len(matches)))
        resolved[role] = matches[0].get_asset()
        if not resolved[role]:
            raise RuntimeError("{}: could not load {} texture".format(token, role))

    source_used = set()
    for parameter_name in unreal.MaterialEditingLibrary.get_texture_parameter_names(source_mi):
        texture = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(
            source_mi, parameter_name)
        if texture:
            source_used.add(_path(texture))
    texture_paths = {_path(texture) for texture in resolved.values()}
    if not texture_paths.issubset(source_used):
        raise RuntimeError("{}: source MI does not use every resolved Base/Normal/ORM texture".format(token))

    parent = _safe_get(source_mi, "parent")
    return source_mi, parent, resolved


def _check_texture(role, texture):
    srgb = bool(_safe_get(texture, "srgb", False))
    compression = str(_safe_get(texture, "compression_settings", ""))
    if role == "base" and not srgb:
        raise RuntimeError("Base Color {} is not sRGB".format(_path(texture)))
    if role == "normal" and (srgb or "TC_NORMALMAP" not in compression):
        raise RuntimeError("Normal {} has incorrect sampling settings".format(_path(texture)))
    if role == "orm" and (srgb or "TC_MASKS" not in compression):
        raise RuntimeError("ORM {} has incorrect sampling settings".format(_path(texture)))
    return {"path": _path(texture), "srgb": srgb, "compression_settings": compression}


def _check_master(master):
    connections = {}
    properties = {
        "base_color": unreal.MaterialProperty.MP_BASE_COLOR,
        "normal": unreal.MaterialProperty.MP_NORMAL,
        "roughness": unreal.MaterialProperty.MP_ROUGHNESS,
        "metallic": unreal.MaterialProperty.MP_METALLIC,
        "ambient_occlusion": unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,
    }
    for name, material_property in properties.items():
        expression = unreal.MaterialEditingLibrary.get_material_property_input_node(
            master, material_property)
        connections[name] = {
            "node_class": expression.get_class().get_name() if expression else "NONE",
            "output": unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
                master, material_property),
        }
        if not expression:
            raise RuntimeError("World-aligned master is missing {} connection".format(name))
    texture_parameters = {str(name) for name in
                          unreal.MaterialEditingLibrary.get_texture_parameter_names(master)}
    vector_parameters = {str(name) for name in
                         unreal.MaterialEditingLibrary.get_vector_parameter_names(master)}
    if not {BASE, NORMAL, ORM}.issubset(texture_parameters) or SIZE not in vector_parameters:
        raise RuntimeError("World-aligned master is missing required VHV parameters")
    tangent_space_normal = _safe_get(master, "tangent_space_normal")
    if tangent_space_normal is not False:
        raise RuntimeError("World-aligned master must have tangent-space normal disabled")
    return connections


def _set_parent(instance, master):
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, master)
    unreal.MaterialEditingLibrary.update_material_instance(instance)
    if _path(_safe_get(instance, "parent")) != _path(master):
        raise RuntimeError("Could not set parent on {}".format(_path(instance)))


def _set_texture(instance, parameter, texture):
    # UE 5.7 setters may return False after a successful write; read back instead.
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
        instance, parameter, texture)
    actual = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(
        instance, parameter)
    if _path(actual) != _path(texture):
        raise RuntimeError("{} did not persist on {}".format(parameter, _path(instance)))


def _set_size(instance, values):
    expected = unreal.LinearColor(*values)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, SIZE, expected)
    actual = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(instance, SIZE)
    wanted = (expected.r, expected.g, expected.b, expected.a)
    observed = (actual.r, actual.g, actual.b, actual.a)
    if any(abs(float(a) - float(b)) > 0.0001 for a, b in zip(wanted, observed)):
        raise RuntimeError("TextureSize did not persist on {}".format(_path(instance)))


def _ensure_instance(request, master, textures, report):
    output = request["output"]
    instance = unreal.EditorAssetLibrary.load_asset(output)
    created = False
    if instance:
        if not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError("{} exists but is not a MaterialInstanceConstant".format(output))
    else:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            output.rsplit("/", 1)[-1], OUTPUT_ROOT,
            unreal.MaterialInstanceConstant, factory)
        if not instance:
            raise RuntimeError("Could not create {}".format(output))
        created = True
    instance.modify()
    _set_parent(instance, master)
    _set_texture(instance, BASE, textures["base"])
    _set_texture(instance, NORMAL, textures["normal"])
    _set_texture(instance, ORM, textures["orm"])
    _set_size(instance, request["texture_size_cm"])
    unreal.MaterialEditingLibrary.update_material_instance(instance)
    if not unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(output))
    report["created" if created else "updated"].append(output)
    return instance


def _verify_instance(request, master, textures):
    instance = _asset(request["output"], unreal.MaterialInstanceConstant)
    errors = []
    if _path(_safe_get(instance, "parent")) != _path(master):
        errors.append("parent mismatch")
    for parameter, role in ((BASE, "base"), (NORMAL, "normal"), (ORM, "orm")):
        actual = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(instance, parameter)
        if _path(actual) != _path(textures[role]):
            errors.append("{} mismatch".format(parameter))
    actual_size = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(instance, SIZE)
    wanted = request["texture_size_cm"]
    observed = (actual_size.r, actual_size.g, actual_size.b, actual_size.a)
    if any(abs(float(a) - float(b)) > 0.0001 for a, b in zip(wanted, observed)):
        errors.append("TextureSize mismatch")
    return instance, errors


def _validate(assets, report):
    validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    for asset in assets:
        result, errors, warnings = validator.is_object_valid(asset, unreal.DataValidationUsecase.MANUAL)
        item = {"asset": _path(asset), "result": str(result),
                "errors": [str(value) for value in errors],
                "warnings": [str(value) for value in warnings]}
        report["validation"].append(item)
        if result == unreal.DataValidationResult.INVALID:
            report["errors"].append("Validation rejected {}".format(_path(asset)))


def main():
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    mode = _mode()
    report = {"timestamp_utc": stamp, "mode": mode,
              "engine_version": str(unreal.SystemLibrary.get_engine_version()),
              "master": MASTER_PATH, "master_connections": {}, "orm_channel_mapping":
              "R=Ambient Occlusion, G=Roughness, B=Metallic",
              "materials": [], "created": [], "updated": [], "validation": [], "errors": []}
    try:
        master = _asset(MASTER_PATH, unreal.Material)
        report["master_connections"] = _check_master(master)
        validation_assets = []
        for request in REQUESTS:
            source_mi, source_parent, textures = _discover(request)
            texture_checks = {role: _check_texture(role, texture)
                              for role, texture in sorted(textures.items())}
            entry = {"output": request["output"], "source_mi": _path(source_mi),
                     "source_parent": _path(source_parent), "token": request["token"],
                     "resolution": request["resolution"],
                     "texture_size_cm": request["texture_size_cm"],
                     "textures": texture_checks}
            if mode == "apply":
                instance = _ensure_instance(request, master, textures, report)
            else:
                instance, errors = _verify_instance(request, master, textures)
                if errors:
                    report["errors"].append("{}: {}".format(request["output"], errors))
            validation_assets.append(instance)
            report["materials"].append(entry)
        _validate(validation_assets, report)
        if report["errors"]:
            raise RuntimeError("Requested surface material verification failed")
    except Exception as exc:
        report["errors"].append(str(exc))
        report["traceback"] = traceback.format_exc()
        unreal.log_error("{} {}".format(LOG, report["traceback"]))
    finally:
        path = _report_path(stamp, mode)
        with open(path, "x", encoding="utf-8") as handle:
            json.dump(report, handle, indent=2, sort_keys=True)
        unreal.log("{} mode={} created={} updated={} errors={} report={}".format(
            LOG, mode, len(report["created"]), len(report["updated"]),
            len(report["errors"]), path))
    if report["errors"]:
        raise RuntimeError("Requested surface instance {} failed; see {}".format(mode, path))


main()
