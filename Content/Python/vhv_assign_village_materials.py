"""Safely audit, assign, and verify realistic VHV village mesh materials.

Default execution is a read-only dry run. Pass one command-line flag:
  -VHVApplyVillageMaterials
  -VHVVerifyVillageMaterials

Assignments use exact mesh paths, imported slot names, and expected current
materials. The script never loads/saves the village map and never uses Save All.
"""

import csv
import datetime
import json
import os
import traceback

import unreal


LOG = "[VHV Village Materials]"
MODEL_ROOT = "/Game/3D_Models"
MATERIAL_ROOT = "/Game/Materials"
REPORT_FOLDER = "VHV_VillageMaterialAssignments"
STANDARD_MASTER = "/Game/Materials/M_VHV_WorldAligned_Master"
MR_MASTER = "/Game/Materials/M_VHV_WorldAligned_MRFactor_Master"

BASE = "BaseColor_Texture"
NORMAL = "Normal_Texture"
ORM = "ORM_Texture"
MR = "MetallicRoughness_Texture"
SIZE = "TextureSize"
BASE_FACTOR = "BaseColor_Factor"

NEW_INSTANCES = {
    "/Game/Materials/MI_VHV_WA_ConcreteAggregate_Warm": {
        "parent": STANDARD_MASTER,
        "textures": {
            BASE: "/Game/Fab/Megascans/Surfaces/Concrete_Pavement_wjwjcis/Medium/wjwjcis_tier_2/Textures/T_wjwjcis_2K_B",
            NORMAL: "/Game/Fab/Megascans/Surfaces/Concrete_Pavement_wjwjcis/Medium/wjwjcis_tier_2/Textures/T_wjwjcis_2K_N",
            ORM: "/Game/Fab/Megascans/Surfaces/Concrete_Pavement_wjwjcis/Medium/wjwjcis_tier_2/Textures/T_wjwjcis_2K_ORM",
        },
        "vectors": {SIZE: (150.0, 150.0, 150.0, 1.0)},
        "purpose": "Warm natural aggregate for masonry/stone surfaces",
    },
    "/Game/Materials/MI_VHV_WA_ConcreteLight_Weathered": {
        "parent": STANDARD_MASTER,
        "textures": {
            BASE: "/Game/Fab/Megascans/Surfaces/Concrete_Pavement_wlrvaf3/Medium/wlrvaf3_tier_2/Textures/T_wlrvaf3_2K_B",
            NORMAL: "/Game/Fab/Megascans/Surfaces/Concrete_Pavement_wlrvaf3/Medium/wlrvaf3_tier_2/Textures/T_wlrvaf3_2K_N",
            ORM: "/Game/Fab/Megascans/Surfaces/Concrete_Pavement_wlrvaf3/Medium/wlrvaf3_tier_2/Textures/T_wlrvaf3_2K_ORM",
        },
        "vectors": {SIZE: (150.0, 150.0, 150.0, 1.0)},
        "purpose": "Light weathered concrete for foundations and steps",
    },
    "/Game/Materials/MI_VHV_WA_DarkPaintedMetal": {
        "parent": MR_MASTER,
        "textures": {
            MR: "/Game/Fab/Megascans/Surfaces/Dirty_Metal_rmmodbdp/Medium/rmmodbdp_tier_2/Textures/T_rmmodbdp_2K_MR",
        },
        "vectors": {
            SIZE: (100.0, 100.0, 100.0, 1.0),
            BASE_FACTOR: (0.055, 0.065, 0.075, 1.0),
        },
        "purpose": "Muted charcoal painted metal with roughness variation",
    },
}


def _source(slot_name):
    return "/Game/3D_Models/Mats/{}".format(slot_name)


def _rule(index, slot_name, desired, current=None, reason=""):
    return {
        "slot_index": index,
        "slot_name": slot_name,
        "expected_current": current or _source(slot_name),
        "desired": desired,
        "reason": reason,
    }


WOOD_DARK = "/Game/Materials/MI_VHV_WA_wckqdbvs"
WOOD_WORN = "/Game/Materials/MI_VHV_WA_whnfbg3"
WOOD_PAINTED = "/Game/Materials/MI_VHV_WA_veviddydy"
CONCRETE_AGGREGATE = "/Game/Materials/MI_VHV_WA_ConcreteAggregate_Warm"
CONCRETE_LIGHT = "/Game/Materials/MI_VHV_WA_ConcreteLight_Weathered"
CONCRETE_DARK = "/Game/Materials/MI_VHV_WA_peugfls0"
METAL_DARK = "/Game/Materials/MI_VHV_WA_DarkPaintedMetal"
ROOF_RUSTED = "/Game/Materials/MI_VHV_WA_vcenefcew"
ROOF_CORRUGATED = "/Game/Materials/MI_VHV_WA_teendf3q"
ROOF_PAINTED = "/Game/Materials/MI_VHV_WA_sf4jce1b"
PLASTER_LIGHT = "/Game/Materials/MI_VHV_WA_wi0oadulw"

U1_RULES = (
    _rule(0, "U1Thai_MAT_AgedTimber", WOOD_WORN, reason="Worn honey-brown timber with Base/Normal/ORM"),
    _rule(1, "U1Thai_MAT_Concrete", CONCRETE_LIGHT, reason="Light weathered world-aligned concrete"),
    _rule(2, "U1Thai_MAT_DarkTimber", WOOD_DARK, reason="Dark aged structural timber"),
    _rule(3, "U1Thai_MAT_RustedRoof", ROOF_RUSTED, reason="Dirty weathered corrugated roofing"),
    _rule(4, "U1Thai_MAT_CorrugatedMetal", ROOF_CORRUGATED, reason="Neutral corrugated PBR metal"),
    _rule(5, "U1Thai_MAT_LightTimber", WOOD_PAINTED, reason="Light painted wood accent"),
)

RULES = {
    "/Game/3D_Models/SPIRITHOUSE": (
        _rule(2, "R3Thai_MAT_GoldPaint", _source("R3Thai_MAT_GoldPaint"),
              current=WOOD_DARK,
              reason="Restore the purpose-built gold finish from an incorrect wood texture"),
        _rule(4, "R3Thai_MAT_DeepRed", _source("R3Thai_MAT_DeepRed"),
              current=WOOD_WORN,
              reason="Restore the purpose-built deep-red finish from an incorrect wood texture"),
    ),
    "/Game/3D_Models/General/ThaiInfra_I6C_Stone_06": (
        _rule(0, "ThaiInfra_ConcreteDark", CONCRETE_DARK, reason="Aged dark cement foundation"),
        _rule(1, "ThaiInfra_Stone", CONCRETE_AGGREGATE, reason="Warm aggregate gives masonry irregular detail"),
        _rule(2, "ThaiInfra_StoneLight", CONCRETE_LIGHT, reason="Lighter weathered masonry variation"),
    ),
    "/Game/3D_Models/General/ThaiInfra_I8A_Post_-1": (
        _rule(0, "ThaiInfra_DarkMetal", METAL_DARK,
              current="/Game/Materials/MI_VHV_WA_rmmodbdp",
              reason="Existing MR instance has a white BaseColor factor"),
    ),
    "/Game/3D_Models/VHV_Bench_01": (
        _rule(0, "VHV_WoodDark", WOOD_DARK, reason="Dark aged wood for legs/frame"),
        _rule(1, "VHV_WoodMid", WOOD_WORN, reason="Rich honey-brown worn wood for seat/back"),
    ),
    "/Game/3D_Models/U1Thai_A_Crate01_Bottom": U1_RULES,
    "/Game/3D_Models/U1Thai_B_BackTopBeam": U1_RULES,
    "/Game/3D_Models/U1Thai_C_EntryStep_00": U1_RULES[:5],
    "/Game/3D_Models/U1Thai_D_BackWall_Plank_00": U1_RULES,
    "/Game/3D_Models/VHV_Gate_Generic_01": (
        _rule(0, "VHV_WoodDark", WOOD_DARK, reason="Dark structural wood"),
        _rule(1, "VHV_WoodMid", WOOD_WORN, reason="Medium worn wood contrast"),
        _rule(3, "VHV_Metal", METAL_DARK, reason="Muted charcoal metal; preserve sign face"),
    ),
    "/Game/3D_Models/VHV_Gate_HealthCenter_01": (
        _rule(0, "VHV_REF_stone_dark", CONCRETE_DARK, reason="Dark aged masonry"),
        _rule(1, "VHV_REF_stone", CONCRETE_AGGREGATE, reason="Warm aggregate masonry"),
        _rule(2, "VHV_REF_stone_light", CONCRETE_LIGHT, reason="Light weathered masonry"),
        _rule(3, "VHV_REF_wood_medium", WOOD_WORN, reason="Medium worn timber"),
        _rule(4, "VHV_REF_wood_struct", WOOD_DARK, reason="Dark structural timber"),
        _rule(5, "VHV_REF_iron", METAL_DARK, reason="Dark painted iron"),
        _rule(6, "VHV_REF_wood_honey", WOOD_WORN, reason="Honey-brown timber"),
        _rule(8, "VHV_REF_plaster", PLASTER_LIGHT, reason="Natural off-white plaster"),
        _rule(11, "VHV_REF_wood_trim", WOOD_PAINTED, reason="Light painted wood trim"),
        _rule(12, "VHV_REF_roof_light", ROOF_PAINTED, reason="Light painted corrugated roof"),
        _rule(13, "VHV_REF_roof_wood", WOOD_DARK, reason="Dark roof timber"),
    ),
    "/Game/3D_Models/VHV_Gate_VillageEntrance_01": (
        _rule(0, "VHV_REF_stone_dark", CONCRETE_DARK, reason="Dark aged masonry"),
        _rule(1, "VHV_REF_stone", CONCRETE_AGGREGATE, reason="Warm aggregate masonry"),
        _rule(2, "VHV_REF_stone_light", CONCRETE_LIGHT, reason="Light weathered masonry"),
        _rule(3, "VHV_REF_wood_medium", WOOD_WORN, reason="Medium worn timber"),
        _rule(4, "VHV_REF_wood_struct", WOOD_DARK, reason="Dark structural timber"),
        _rule(5, "VHV_REF_iron", METAL_DARK, reason="Dark painted iron"),
        _rule(6, "VHV_REF_wood_honey", WOOD_WORN, reason="Honey-brown timber"),
        _rule(9, "VHV_REF_wood_trim", WOOD_PAINTED, reason="Light painted wood trim"),
        _rule(12, "VHV_REF_wood_board", WOOD_WORN, reason="Worn timber board"),
        _rule(13, "VHV_REF_wood_aged", WOOD_DARK, reason="Dark aged timber"),
    ),
    "/Game/3D_Models/VHV_Shelter_Instructor_01": (
        _rule(0, "VHV_REF_stone_dark", CONCRETE_DARK, reason="Dark aged masonry"),
        _rule(1, "VHV_REF_stone", CONCRETE_AGGREGATE, reason="Warm aggregate masonry"),
        _rule(2, "VHV_REF_stone_light", CONCRETE_LIGHT, reason="Light weathered masonry"),
        _rule(3, "VHV_REF_wood_medium", WOOD_WORN, reason="Medium worn timber"),
        _rule(4, "VHV_REF_wood_struct", WOOD_DARK, reason="Dark structural timber"),
        _rule(5, "VHV_REF_iron", METAL_DARK, reason="Dark painted iron"),
        _rule(6, "VHV_REF_wood_honey", WOOD_WORN, reason="Honey-brown timber"),
        _rule(8, "VHV_REF_wood_trim", WOOD_PAINTED, reason="Light painted wood trim"),
        _rule(9, "VHV_REF_roof_light", ROOF_PAINTED, reason="Light painted corrugated roof"),
        _rule(10, "VHV_REF_roof_wood", WOOD_DARK, reason="Dark roof timber"),
        _rule(12, "VHV_REF_wood_board", WOOD_WORN, reason="Worn timber board"),
        _rule(13, "VHV_REF_wood_aged", WOOD_DARK, reason="Dark aged timber"),
    ),
}

PRESERVED_SPECIAL_SLOTS = {
    "/Game/3D_Models/SPIRITHOUSE": [
        "R3Thai_MAT_DarkInterior", "R3Thai_MAT_GoldPaint", "R3Thai_MAT_DeepRed"],
    "/Game/3D_Models/General/ThaiInfra_I8A_Post_-1": ["ThaiInfra_SignBoard"],
    "/Game/3D_Models/VHV_Gate_Generic_01": ["VHV_SignBoard"],
    "/Game/3D_Models/VHV_Gate_HealthCenter_01": ["VHV_REF_blue", "VHV_REF_red", "VHV_REF_ivory", "VHV_REF_brass"],
    "/Game/3D_Models/VHV_Gate_VillageEntrance_01": ["VHV_REF_blue", "VHV_REF_ivory", "VHV_REF_brass", "VHV_REF_rope", "VHV_REF_lantern"],
    "/Game/3D_Models/VHV_Shelter_Instructor_01": ["VHV_REF_ivory", "VHV_REF_rope", "VHV_REF_paver_a", "VHV_REF_paver_b", "VHV_REF_paver_c", "VHV_REF_paver_d"],
}


def _package_path(asset):
    return asset.get_path_name().split(".", 1)[0] if asset else ""


def _safe_get(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def _mode():
    command_line = str(unreal.SystemLibrary.get_command_line())
    apply_flag = "-VHVApplyVillageMaterials" in command_line
    verify_flag = "-VHVVerifyVillageMaterials" in command_line
    if apply_flag and verify_flag:
        raise RuntimeError("Apply and verify flags are mutually exclusive")
    return "apply" if apply_flag else ("verify" if verify_flag else "dry_run")


def _timestamp():
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")


def _report_folder():
    path = os.path.normpath(os.path.join(unreal.Paths.project_saved_dir(), REPORT_FOLDER))
    os.makedirs(path, exist_ok=True)
    return path


def _write_json(prefix, stamp, data):
    path = os.path.join(_report_folder(), "{}_{}.json".format(prefix, stamp))
    with open(path, "x", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
    return os.path.normpath(path)


def _write_csv(prefix, stamp, rows):
    path = os.path.join(_report_folder(), "{}_{}.csv".format(prefix, stamp))
    fields = ["Mesh", "SlotIndex", "SlotName", "PreviousMaterial", "AssignedMaterial", "Status", "Reason"]
    with open(path, "x", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})
    return os.path.normpath(path)


def _asset(path, expected_type=None):
    value = unreal.EditorAssetLibrary.load_asset(path)
    if not value:
        raise RuntimeError("Missing asset {}".format(path))
    if expected_type and not isinstance(value, expected_type):
        raise RuntimeError("{} has unexpected type".format(path))
    return value


def _slot_info(mesh):
    result = []
    for index, slot in enumerate(_safe_get(mesh, "static_materials", []) or []):
        result.append({
            "index": index,
            "slot_name": str(_safe_get(slot, "material_slot_name", "") or ""),
            "imported_slot_name": str(_safe_get(slot, "imported_material_slot_name", "") or ""),
            "material": _package_path(_safe_get(slot, "material_interface")),
        })
    return result


def _count_static_meshes():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    count = 0
    for data in registry.get_assets_by_path(MODEL_ROOT, recursive=True):
        try:
            class_name = str(data.asset_class_path.asset_name)
        except Exception:
            class_name = str(data.asset_class)
        if class_name == "StaticMesh":
            count += 1
    return count


def _validate_prerequisites(report):
    existing = sorted({rule["desired"] for rules in RULES.values() for rule in rules if rule["desired"] not in NEW_INSTANCES})
    for path in existing:
        _asset(path, unreal.MaterialInstanceConstant)
    for path, definition in sorted(NEW_INSTANCES.items()):
        _asset(definition["parent"], unreal.Material)
        for parameter, texture_path in sorted(definition["textures"].items()):
            texture = _asset(texture_path, unreal.Texture)
            report["source_texture_checks"].append({
                "instance": path,
                "parameter": parameter,
                "texture": _package_path(texture),
                "srgb": bool(_safe_get(texture, "srgb", False)),
                "compression_settings": str(_safe_get(texture, "compression_settings", "")),
            })
    report["materials_reused"] = existing


def _build_plan(report, verification=False):
    rows, plan, conflicts = [], [], []
    for mesh_path, rules in sorted(RULES.items()):
        mesh = _asset(mesh_path, unreal.StaticMesh)
        slots = _slot_info(mesh)
        report["target_mesh_slots"][mesh_path] = slots
        for rule in rules:
            index = rule["slot_index"]
            if index >= len(slots):
                conflicts.append({"mesh": mesh_path, "slot_index": index, "reason": "slot index out of range"})
                continue
            slot = slots[index]
            if slot["imported_slot_name"] != rule["slot_name"]:
                conflicts.append({"mesh": mesh_path, "slot_index": index, "reason": "imported slot mismatch", "expected": rule["slot_name"], "actual": slot["imported_slot_name"]})
                continue
            current, desired = slot["material"], rule["desired"]
            if current == desired:
                status = "verified" if verification else "already_correct"
            elif verification:
                status = "verification_failed"
                conflicts.append({"mesh": mesh_path, "slot_index": index, "reason": "persisted assignment mismatch", "expected": desired, "actual": current})
            elif current == rule["expected_current"]:
                status = "proposed"
                plan.append({"mesh": mesh_path, "slot_index": index, "slot_name": rule["slot_name"], "previous_material": current, "desired_material": desired, "reason": rule["reason"]})
            else:
                status = "conflict"
                conflicts.append({"mesh": mesh_path, "slot_index": index, "reason": "current material changed outside this plan", "expected_current": rule["expected_current"], "actual": current, "desired": desired})
            rows.append({"Mesh": mesh_path, "SlotIndex": index, "SlotName": rule["slot_name"], "PreviousMaterial": current, "AssignedMaterial": desired, "Status": status, "Reason": rule["reason"]})
    report["plan"], report["conflicts"] = plan, conflicts
    return plan, rows


def _set_parent(instance, parent):
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
    unreal.MaterialEditingLibrary.update_material_instance(instance)
    if _package_path(_safe_get(instance, "parent")) != _package_path(parent):
        raise RuntimeError("Could not set parent on {}".format(_package_path(instance)))


def _set_texture(instance, parameter, texture):
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(instance, parameter, texture)
    actual = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(instance, parameter)
    if _package_path(actual) != _package_path(texture):
        raise RuntimeError("Could not set {} on {}".format(parameter, _package_path(instance)))


def _set_vector(instance, parameter, values):
    value = unreal.LinearColor(*values)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, parameter, value)
    actual = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(instance, parameter)
    expected = (value.r, value.g, value.b, value.a)
    observed = (actual.r, actual.g, actual.b, actual.a)
    if any(abs(float(a) - float(b)) > 0.0001 for a, b in zip(expected, observed)):
        raise RuntimeError("Could not set {} on {}".format(parameter, _package_path(instance)))


def _ensure_instances(report):
    for path, definition in sorted(NEW_INSTANCES.items()):
        instance = unreal.EditorAssetLibrary.load_asset(path)
        created = False
        if instance:
            if not isinstance(instance, unreal.MaterialInstanceConstant):
                raise RuntimeError("{} exists with an unexpected type".format(path))
        else:
            name = path.rsplit("/", 1)[-1]
            factory = unreal.MaterialInstanceConstantFactoryNew()
            instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MATERIAL_ROOT, unreal.MaterialInstanceConstant, factory)
            if not instance:
                raise RuntimeError("Could not create {}".format(path))
            created = True
        instance.modify()
        _set_parent(instance, _asset(definition["parent"], unreal.Material))
        for parameter, texture_path in sorted(definition["textures"].items()):
            _set_texture(instance, parameter, _asset(texture_path, unreal.Texture))
        for parameter, values in sorted(definition["vectors"].items()):
            _set_vector(instance, parameter, values)
        unreal.MaterialEditingLibrary.update_material_instance(instance)
        if not unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False):
            raise RuntimeError("Could not save {}".format(path))
        report["instances_created" if created else "instances_reused"].append(path)


def _verify_instance(path, definition):
    instance = _asset(path, unreal.MaterialInstanceConstant)
    errors = []
    if _package_path(_safe_get(instance, "parent")) != definition["parent"]:
        errors.append("parent mismatch")
    for parameter, texture_path in sorted(definition["textures"].items()):
        actual = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(instance, parameter)
        if _package_path(actual) != texture_path:
            errors.append("{} mismatch: {}".format(parameter, _package_path(actual)))
    for parameter, values in sorted(definition["vectors"].items()):
        actual = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(instance, parameter)
        observed = (actual.r, actual.g, actual.b, actual.a)
        if any(abs(float(a) - float(b)) > 0.0001 for a, b in zip(values, observed)):
            errors.append("{} mismatch: {}".format(parameter, observed))
    return instance, errors


def _apply(plan, rows, report):
    _ensure_instances(report)
    by_mesh = {}
    for change in plan:
        by_mesh.setdefault(change["mesh"], []).append(change)
    for mesh_path, changes in sorted(by_mesh.items()):
        mesh = _asset(mesh_path, unreal.StaticMesh)
        mesh.modify()
        for change in sorted(changes, key=lambda item: item["slot_index"]):
            current = _slot_info(mesh)[change["slot_index"]]["material"]
            if current != change["previous_material"]:
                raise RuntimeError("Slot changed during apply: {}[{}]".format(mesh_path, change["slot_index"]))
            mesh.set_material(change["slot_index"], _asset(change["desired_material"], unreal.MaterialInterface))
            observed = _slot_info(mesh)[change["slot_index"]]["material"]
            if observed != change["desired_material"]:
                raise RuntimeError("In-memory assignment failed: {}[{}]".format(mesh_path, change["slot_index"]))
            report["changed_slots"].append(change)
        if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
            raise RuntimeError("Could not save {}".format(mesh_path))
        report["meshes_modified"].append(mesh_path)
    for row in rows:
        if row["Status"] == "proposed":
            row["Status"] = "applied"


def _editor_validate(assets, report):
    subsystem = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    for asset in assets:
        result, errors, warnings = subsystem.is_object_valid(asset, unreal.DataValidationUsecase.MANUAL)
        entry = {"asset": _package_path(asset), "result": str(result), "errors": [str(v) for v in errors], "warnings": [str(v) for v in warnings]}
        report["validation"].append(entry)
        if result == unreal.DataValidationResult.INVALID:
            report["errors"].append("Validation rejected {}: {}".format(_package_path(asset), entry["errors"]))


def main():
    stamp, mode = _timestamp(), _mode()
    report = {
        "timestamp_utc": stamp, "mode": mode,
        "engine_version": str(unreal.SystemLibrary.get_engine_version()),
        "static_meshes_inspected": _count_static_meshes(),
        "target_mesh_count": len(RULES), "target_mesh_slots": {},
        "preserved_special_slots": PRESERVED_SPECIAL_SLOTS,
        "new_instance_definitions": NEW_INSTANCES,
        "source_texture_checks": [], "materials_reused": [], "plan": [],
        "conflicts": [], "instances_created": [], "instances_reused": [],
        "meshes_modified": [], "changed_slots": [], "validation": [], "errors": [],
    }
    rows, json_path = [], ""
    try:
        _validate_prerequisites(report)
        if mode == "verify":
            for path, definition in sorted(NEW_INSTANCES.items()):
                unused, errors = _verify_instance(path, definition)
                if errors:
                    report["errors"].append("{}: {}".format(path, errors))
            unused, rows = _build_plan(report, verification=True)
            assets = [_asset(path, unreal.StaticMesh) for path in sorted(RULES)]
            assets.extend(_asset(path, unreal.MaterialInstanceConstant) for path in sorted(NEW_INSTANCES))
            _editor_validate(assets, report)
        else:
            plan, rows = _build_plan(report, verification=False)
            if report["conflicts"]:
                raise RuntimeError("Preflight found {} assignment conflicts".format(len(report["conflicts"])))
            if mode == "apply":
                _apply(plan, rows, report)
                assets = [_asset(path, unreal.StaticMesh) for path in sorted(set(report["meshes_modified"]))]
                assets.extend(_asset(path, unreal.MaterialInstanceConstant) for path in sorted(NEW_INSTANCES))
                _editor_validate(assets, report)
        if report["conflicts"] or report["errors"]:
            raise RuntimeError("Material assignment completed with verification errors")
    except Exception as exc:
        report["errors"].append(str(exc))
        report["traceback"] = traceback.format_exc()
        unreal.log_error("{} {}".format(LOG, report["traceback"]))
    finally:
        json_path = _write_json("VHV_VillageMaterials_{}".format(mode), stamp, report)
        csv_path = _write_csv("VHV_VillageMaterials_{}".format(mode), stamp, rows)
        unreal.log("{} mode={} inspected={} targets={} proposed={} modified_meshes={} changed_slots={} errors={}".format(LOG, mode, report["static_meshes_inspected"], report["target_mesh_count"], len(report["plan"]), len(report["meshes_modified"]), len(report["changed_slots"]), len(report["errors"])))
        unreal.log("{} report={} csv={}".format(LOG, json_path, csv_path))
    if report["errors"]:
        raise RuntimeError("VHV village material {} failed; see {}".format(mode, json_path))


main()
