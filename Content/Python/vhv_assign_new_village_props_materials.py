"""Assign verified VHV PBR materials to the ten wallsnstuff Blender meshes.

Default mode applies only the explicit mappings below. Pass
-VHVVerifyNewPropsMaterials for a read-only cold verification pass.
"""

import csv
import datetime
import json
import os
import traceback
import unreal


LOG = "[VHV New Props Materials]"
ROOT = "/Game/3D_Models/wallsnstuff"
MATERIAL_ROOT = "/Game/Materials"
MR_MASTER = "/Game/Materials/M_VHV_WorldAligned_MRFactor_Master"
MR_TEXTURE = "/Game/Fab/Megascans/Surfaces/Oxidized_Coated_Steel_shlabh0c/Medium/shlabh0c_tier_2/Textures/T_shlabh0c_2K_MR"
GALVANIZED = "/Game/Materials/MI_VHV_WA_GalvanizedSteel"

WOOD_DARK = "/Game/Materials/MI_VHV_WA_wckqdbvs"
WOOD_WARM = "/Game/Materials/MI_VHV_WA_whnfbg3"
METAL_DARK = "/Game/Materials/MI_VHV_WA_DarkPaintedMetal"
ROPE_FALLBACK = "/Game/Materials/MI_VHV_WA_xjghdgl"
THATCH_DARK_FALLBACK = "/Game/Materials/MI_VHV_WA_wckqdbvs"
THATCH_LIGHT_FALLBACK = "/Game/Materials/MI_VHV_WA_whnfbg3"
CLAY_FALLBACK = "/Game/Materials/MI_VHV_WA_Brick_Worn"
STONE_FACADE = "/Game/Materials/MI_VHV_WA_Stone_Facade"
STONE_CAP = "/Game/Materials/MI_VHV_WA_Stone_Floor"
GLASS = "/Game/Materials/M_TransparentGlass"
ROOF_WEATHERED = "/Game/Materials/MI_VHV_WA_vd3kcjs"
ROOF_DARK_TIMBER = "/Game/Materials/MI_VHV_WA_wckqdbvs"

EXPECTED_NAMES = (
    "VHV_Fence_Wood_Corner_L", "VHV_Fence_Wood_Gate_1p8m", "VHV_Fence_Wood_Straight_3m",
    "VHV_Stone_EntrySteps_WallComplete", "VHV_Stone_Pillar_Standalone",
    "VHV_Stone_Wall_Corner_L", "VHV_Stone_Wall_ShortEnd", "VHV_Stone_Wall_Straight_3m",
    "VHV_Riverbank_FishingPlatform_Complete", "VHV_Village_EntranceGate_NoSign_Complete",
)

LIMITATIONS = [
    "No dedicated bamboo PBR scan is installed; warm weathered wood is used for the bamboo slot.",
    "No dedicated thatch/palm PBR scan is installed; dark/warm weathered wood are the closest available opaque PBR fallbacks.",
    "No hemp-rope PBR scan is installed; brown leather is used only for the small rope-binding slot.",
    "No terracotta PBR scan is installed; worn brick is used only for small pottery slots.",
]


def _source(slot):
    return "/Game/3D_Models/Mats/{}".format(slot)


def _rule(index, slot, desired, reason):
    return {"slot_index": index, "slot_name": slot, "expected_current": _source(slot),
            "desired": desired, "reason": reason}


FENCE_RULES = (
    _rule(0, "VHV_Wood_EndGrain", WOOD_WARM, "Warm textured timber for exposed cut ends"),
    _rule(1, "VHV_Wood_DarkAged", WOOD_DARK, "Dark aged PBR timber for structural posts"),
    _rule(2, "VHV_Wood_WarmWeathered", WOOD_WARM, "Weathered honey-brown PBR timber for rails/pickets"),
    _rule(3, "VHV_Metal_BlackIron", METAL_DARK, "Charcoal painted-metal PBR for hinges and nails"),
    _rule(4, "VHV_Wood_HoneyTimber", WOOD_WARM, "Rich warm timber accent"),
)

STONE_RULES = (
    _rule(0, "VHV_StoneFacade__UE_MI_vjqifhu", STONE_FACADE,
          "World-aligned curated stone facade for the masonry body"),
    _rule(1, "VHV_Stone_LightCapstone", STONE_CAP,
          "Lighter world-aligned stone floor for caps and wall tops"),
)

RULES = {
    "/Game/3D_Models/wallsnstuff/VHV_Fence_Wood_Corner_L": FENCE_RULES,
    "/Game/3D_Models/wallsnstuff/VHV_Fence_Wood_Gate_1p8m": FENCE_RULES,
    "/Game/3D_Models/wallsnstuff/VHV_Fence_Wood_Straight_3m": FENCE_RULES,
    "/Game/3D_Models/wallsnstuff/VHV_Stone_EntrySteps_WallComplete": STONE_RULES,
    "/Game/3D_Models/wallsnstuff/VHV_Stone_Pillar_Standalone": STONE_RULES,
    "/Game/3D_Models/wallsnstuff/VHV_Stone_Wall_Corner_L": STONE_RULES,
    "/Game/3D_Models/wallsnstuff/VHV_Stone_Wall_ShortEnd": STONE_RULES,
    "/Game/3D_Models/wallsnstuff/VHV_Stone_Wall_Straight_3m": STONE_RULES,
    "/Game/3D_Models/wallsnstuff/VHV_Riverbank_FishingPlatform_Complete": (
        _rule(0, "VHV_Wood_DarkAged", WOOD_DARK, "Dark weathered support posts/cross braces"),
        _rule(1, "VHV_Wood_WarmWeathered", WOOD_WARM, "Natural timber deck and stairs"),
        _rule(2, "VHV_Metal_BlackIron", METAL_DARK, "Dark weathered metal attachments"),
        _rule(3, "VHV_Wood_HoneyTimber", WOOD_WARM, "Warm timber handrails and bench"),
        _rule(4, "VHV_Rope_Hemp", ROPE_FALLBACK, "Brown rough PBR fallback for small hemp bindings"),
        _rule(5, "VHV_Bamboo_Aged", WOOD_WARM, "Closest installed textured PBR fallback for aged bamboo"),
        _rule(6, "VHV_Thatch_ShadedPalm", THATCH_DARK_FALLBACK, "Closest installed dark rough PBR fallback for shaded thatch"),
        _rule(7, "VHV_Thatch_DriedPalm", THATCH_LIGHT_FALLBACK, "Closest installed warm rough PBR fallback for dried thatch"),
        _rule(8, "VHV_Metal_Galvanized", GALVANIZED, "Tinted galvanized-steel PBR for fishing bucket"),
        _rule(9, "VHV_Clay_Pottery", CLAY_FALLBACK, "Warm rough PBR fallback for small terracotta pottery"),
    ),
    "/Game/3D_Models/wallsnstuff/VHV_Village_EntranceGate_NoSign_Complete": (
        _rule(0, "VHV_Wood_EndGrain", WOOD_WARM, "Warm timber exposed ends"),
        _rule(1, "VHV_Wood_DarkAged", WOOD_DARK, "Dark aged major columns and beams"),
        _rule(2, "VHV_Wood_WarmWeathered", WOOD_WARM, "Weathered contrasting cross braces"),
        _rule(3, "VHV_Metal_BlackIron", METAL_DARK, "Dark iron brackets and fasteners"),
        _rule(4, "VHV_Wood_HoneyTimber", WOOD_WARM, "Warm honey-brown decorative timber"),
        _rule(5, "VHV_Clay_Pottery", CLAY_FALLBACK, "Warm rough PBR fallback for small pottery"),
        _rule(6, "VHV_StoneFacade__UE_MI_vjqifhu", STONE_FACADE, "Curated stone facade for pillar bases"),
        _rule(7, "VHV_Stone_LightCapstone", STONE_CAP, "Lighter stone caps"),
        _rule(8, "VHV_Glass_LanternWarm", GLASS, "Existing translucent glass material for lantern glazing"),
        _rule(9, "VHV_Roof_WeatheredShingles", ROOF_WEATHERED, "Weathered painted-roof PBR for primary roof"),
        _rule(10, "VHV_Roof_DarkTimberShingles", ROOF_DARK_TIMBER, "Dark timber PBR for secondary roof detail"),
    ),
}


def _path(obj):
    return obj.get_path_name().split(".", 1)[0] if obj else ""


def _safe_get(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def _nanite_enabled(mesh):
    """Read only; this automation never writes Nanite settings."""
    return bool(_safe_get(_safe_get(mesh, "nanite_settings"), "enabled", False))


def _class_name(data):
    try:
        return str(data.asset_class_path.asset_name)
    except Exception:
        return str(data.asset_class)


def _mode():
    return "verify" if "-VHVVerifyNewPropsMaterials" in str(
        unreal.SystemLibrary.get_command_line()) else "apply"


def _folder():
    result = os.path.normpath(os.path.join(unreal.Paths.project_saved_dir(), "VHV_NewPropsMaterials"))
    os.makedirs(result, exist_ok=True)
    return result


def _write_report(stamp, report, rows):
    folder = _folder()
    json_path = os.path.join(folder, "VHV_NewPropsMaterials_{}_{}.json".format(report["mode"], stamp))
    csv_path = os.path.join(folder, "VHV_NewPropsMaterials_{}_{}.csv".format(report["mode"], stamp))
    with open(json_path, "x", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
    fields = ("Mesh", "SlotIndex", "ImportedSlotName", "OriginalMaterial", "AssignedMaterial", "Reason", "VerificationStatus")
    with open(csv_path, "x", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})
    return json_path, csv_path


def _asset(path, expected=None):
    value = unreal.EditorAssetLibrary.load_asset(path)
    if not value:
        raise RuntimeError("Missing required asset {}".format(path))
    if expected and not isinstance(value, expected):
        raise RuntimeError("{} has unexpected type".format(path))
    return value


def _slots(mesh):
    result = []
    for index, slot in enumerate(_safe_get(mesh, "static_materials", []) or []):
        result.append({"index": index,
                       "imported_slot_name": str(_safe_get(slot, "imported_material_slot_name", "") or ""),
                       "material": _path(_safe_get(slot, "material_interface"))})
    return result


def _discover_meshes():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = {}
    for data in registry.get_assets_by_path(ROOT, recursive=True):
        if _class_name(data) != "StaticMesh":
            continue
        mesh = data.get_asset()
        if mesh and mesh.get_name() in EXPECTED_NAMES:
            if mesh.get_name() in found:
                raise RuntimeError("Multiple meshes resolve as {}".format(mesh.get_name()))
            found[mesh.get_name()] = mesh
    missing = sorted(set(EXPECTED_NAMES) - set(found))
    if missing or len(found) != len(EXPECTED_NAMES):
        raise RuntimeError("Expected exactly ten target meshes; missing={}".format(missing))
    return {_path(mesh): mesh for mesh in found.values()}


def _material_library_summary():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    result = []
    for data in registry.get_assets_by_path(MATERIAL_ROOT, recursive=False):
        if _class_name(data) not in ("Material", "MaterialInstanceConstant"):
            continue
        material = data.get_asset()
        if not material:
            continue
        textures = []
        if isinstance(material, unreal.MaterialInstanceConstant):
            for name in unreal.MaterialEditingLibrary.get_texture_parameter_names(material):
                texture = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(material, name)
                if texture:
                    textures.append(_path(texture))
        elif isinstance(material, unreal.Material):
            textures = [_path(texture) for texture in unreal.MaterialEditingLibrary.get_used_textures(material)]
        result.append({"path": _path(material), "class": material.get_class().get_name(),
                       "parent": _path(_safe_get(material, "parent")),
                       "blend_mode": str(_safe_get(material, "blend_mode")),
                       "textures": sorted(set(textures))})
    return sorted(result, key=lambda item: item["path"])


def _set_parent(instance, parent):
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
    unreal.MaterialEditingLibrary.update_material_instance(instance)
    if _path(_safe_get(instance, "parent")) != _path(parent):
        raise RuntimeError("Could not set parent on {}".format(_path(instance)))


def _set_texture(instance, parameter, texture):
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(instance, parameter, texture)
    actual = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(instance, parameter)
    if _path(actual) != _path(texture):
        raise RuntimeError("Could not set {} on {}".format(parameter, _path(instance)))


def _set_vector(instance, parameter, values):
    expected = unreal.LinearColor(*values)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, parameter, expected)
    actual = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(instance, parameter)
    if any(abs(float(a) - float(b)) > 0.0001 for a, b in zip(
            (expected.r, expected.g, expected.b, expected.a),
            (actual.r, actual.g, actual.b, actual.a))):
        raise RuntimeError("Could not set {} on {}".format(parameter, _path(instance)))


def _ensure_galvanized(report):
    instance = unreal.EditorAssetLibrary.load_asset(GALVANIZED)
    created = False
    if instance:
        if not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError("{} exists with an unexpected type".format(GALVANIZED))
    else:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_VHV_WA_GalvanizedSteel", MATERIAL_ROOT,
            unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if not instance:
            raise RuntimeError("Could not create {}".format(GALVANIZED))
        created = True
    instance.modify()
    _set_parent(instance, _asset(MR_MASTER, unreal.Material))
    _set_texture(instance, "MetallicRoughness_Texture", _asset(MR_TEXTURE, unreal.Texture))
    _set_vector(instance, "TextureSize", (100.0, 100.0, 100.0, 1.0))
    _set_vector(instance, "BaseColor_Factor", (0.42, 0.47, 0.50, 1.0))
    unreal.MaterialEditingLibrary.update_material_instance(instance)
    if not unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(GALVANIZED))
    report["created_materials" if created else "updated_materials"].append(GALVANIZED)


def _build_plan(meshes, verify=False):
    plan, rows, conflicts = [], [], []
    for mesh_path, rules in sorted(RULES.items()):
        mesh = meshes.get(mesh_path)
        if not mesh:
            conflicts.append({"mesh": mesh_path, "reason": "mesh did not resolve"})
            continue
        slots = _slots(mesh)
        for rule in rules:
            index = rule["slot_index"]
            if index >= len(slots) or slots[index]["imported_slot_name"] != rule["slot_name"]:
                conflicts.append({"mesh": mesh_path, "slot_index": index, "reason": "slot signature mismatch"})
                continue
            current = slots[index]["material"]
            if current == rule["desired"]:
                status = "verified" if verify else "already_correct"
            elif verify:
                status = "verification_failed"
                conflicts.append({"mesh": mesh_path, "slot_index": index, "reason": "persisted assignment mismatch", "actual": current, "expected": rule["desired"]})
            elif current == rule["expected_current"]:
                status = "proposed"
                plan.append({"mesh": mesh_path, "slot_index": index, "slot_name": rule["slot_name"],
                             "previous_material": current, "desired_material": rule["desired"], "reason": rule["reason"]})
            else:
                status = "conflict"
                conflicts.append({"mesh": mesh_path, "slot_index": index, "reason": "current material differs from audited placeholder", "actual": current, "expected": rule["expected_current"]})
            rows.append({"Mesh": mesh_path, "SlotIndex": index, "ImportedSlotName": rule["slot_name"],
                         "OriginalMaterial": current, "AssignedMaterial": rule["desired"],
                         "Reason": rule["reason"], "VerificationStatus": status})
    return plan, rows, conflicts


def _apply(meshes, plan, rows, report):
    by_mesh = {}
    for item in plan:
        by_mesh.setdefault(item["mesh"], []).append(item)
    for mesh_path, changes in sorted(by_mesh.items()):
        mesh = meshes[mesh_path]
        mesh.modify()
        for change in sorted(changes, key=lambda item: item["slot_index"]):
            current = _slots(mesh)[change["slot_index"]]["material"]
            if current != change["previous_material"]:
                raise RuntimeError("Slot changed during apply: {}[{}]".format(mesh_path, change["slot_index"]))
            mesh.set_material(change["slot_index"], _asset(change["desired_material"]))
            if _slots(mesh)[change["slot_index"]]["material"] != change["desired_material"]:
                raise RuntimeError("In-memory assignment failed: {}[{}]".format(mesh_path, change["slot_index"]))
            report["changed_slots"].append(change)
        if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
            raise RuntimeError("Could not save {}".format(mesh_path))
        report["meshes_modified"].append(mesh_path)
    for row in rows:
        if row["VerificationStatus"] == "proposed":
            row["VerificationStatus"] = "applied"


def _validate(assets, report):
    validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    for asset in assets:
        result, errors, warnings = validator.is_object_valid(asset, unreal.DataValidationUsecase.MANUAL)
        item = {"asset": _path(asset), "result": str(result), "errors": [str(v) for v in errors], "warnings": [str(v) for v in warnings]}
        report["validation"].append(item)
        if result == unreal.DataValidationResult.INVALID:
            report["errors"].append("Data Validation rejected {}".format(_path(asset)))


def main():
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    mode = _mode()
    report = {"timestamp_utc": stamp, "mode": mode,
              "engine_version": str(unreal.SystemLibrary.get_engine_version()),
              "target_meshes_expected": list(EXPECTED_NAMES), "material_library": [],
              "limitations": LIMITATIONS, "plan": [], "conflicts": [], "changed_slots": [],
              "meshes_modified": [], "created_materials": [], "updated_materials": [],
              "validation": [], "nanite_enabled": {}, "errors": []}
    rows, report_path = [], ""
    try:
        report["material_library"] = _material_library_summary()
        meshes = _discover_meshes()
        if set(meshes) != set(RULES):
            raise RuntimeError("Resolved meshes do not match explicit rules")
        report["nanite_enabled"] = {path: _nanite_enabled(meshes[path]) for path in sorted(meshes)}
        if not all(report["nanite_enabled"].values()):
            raise RuntimeError("Nanite status differs from the audited enabled state")
        for path in sorted({rule["desired"] for rules in RULES.values() for rule in rules if rule["desired"] != GALVANIZED}):
            _asset(path)
        if mode == "apply":
            _ensure_galvanized(report)
        else:
            _asset(GALVANIZED, unreal.MaterialInstanceConstant)
        plan, rows, conflicts = _build_plan(meshes, verify=(mode == "verify"))
        report["plan"], report["conflicts"] = plan, conflicts
        if conflicts:
            raise RuntimeError("Preflight found {} conflict(s)".format(len(conflicts)))
        if mode == "apply":
            _apply(meshes, plan, rows, report)
            validation_assets = [meshes[path] for path in sorted(report["meshes_modified"])]
            validation_assets.append(_asset(GALVANIZED, unreal.MaterialInstanceConstant))
        else:
            validation_assets = [meshes[path] for path in sorted(meshes)]
            validation_assets.append(_asset(GALVANIZED, unreal.MaterialInstanceConstant))
        _validate(validation_assets, report)
        if report["errors"]:
            raise RuntimeError("Validation errors")
    except Exception as exc:
        report["errors"].append(str(exc))
        report["traceback"] = traceback.format_exc()
        unreal.log_error("{} {}".format(LOG, report["traceback"]))
    finally:
        report_path, csv_path = _write_report(stamp, report, rows)
        unreal.log("{} mode={} plan={} meshes={} slots={} created={} errors={} report={} csv={}".format(
            LOG, mode, len(report["plan"]), len(report["meshes_modified"]), len(report["changed_slots"]),
            len(report["created_materials"]), len(report["errors"]), report_path, csv_path))
    if report["errors"]:
        raise RuntimeError("VHV new-props material {} failed; see {}".format(mode, report_path))


main()
