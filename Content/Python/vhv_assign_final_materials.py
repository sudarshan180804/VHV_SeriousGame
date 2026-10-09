"""Directly assign the final approved VHV mesh materials after full validation."""

import datetime
import json
import os
import traceback

import unreal


MESH_ROOT = "/Game/3D_Models"
MATERIAL_ROOT = "/Game/Materials"
SAVED_FOLDER = "VHV_FinalMaterialAssignments"
LOG_PREFIX = "[VHV Final Materials]"

EXACT_MESH_NAMES = (
    "SPIRITHOUSE",
    "ThaiInfra_I5D_MeshB_12",
    "ThaiInfra_I6B_StepWall_2",
    "ThaiInfra_I6B_StepWall_0",
    "ThaiInfra_I6B_StepCap_1",
)
STONE_SUFFIX = "Stone_06"

EXPECTED_SIGNATURES = {
    "SPIRITHOUSE": (
        "R3Thai_MAT_Cream",
        "R3Thai_MAT_DarkInterior",
        "R3Thai_MAT_GoldPaint",
        "R3Thai_MAT_WhitePlaster",
        "R3Thai_MAT_DeepRed",
        "R3Thai_MAT_Concrete",
    ),
    "ThaiInfra_I5D_MeshB_12": ("ThaiInfra_GalvanizedMetal",),
    "ThaiInfra_I6B_StepWall_2": ("ThaiInfra_Concrete", "ThaiInfra_ConcreteLight"),
    "ThaiInfra_I6B_StepWall_0": ("ThaiInfra_Concrete", "ThaiInfra_ConcreteLight"),
    "ThaiInfra_I6B_StepCap_1": ("ThaiInfra_Concrete", "ThaiInfra_ConcreteLight"),
    "stone": ("ThaiInfra_ConcreteDark", "ThaiInfra_Stone", "ThaiInfra_StoneLight"),
}

ASSIGNMENTS = {
    "SPIRITHOUSE": (
        (3, "R3Thai_MAT_WhitePlaster", "MI_VHV_WA_wi0oadulw"),
        (5, "R3Thai_MAT_Concrete", "MI_VHV_WA_peugfls0"),
    ),
    "ThaiInfra_I5D_MeshB_12": (
        (0, "ThaiInfra_GalvanizedMetal", "MI_VHV_WA_shlabh0c"),
    ),
    "ThaiInfra_I6B_StepWall_2": (
        (0, "ThaiInfra_Concrete", "MI_VHV_WA_peugfls0"),
    ),
    "ThaiInfra_I6B_StepWall_0": (
        (0, "ThaiInfra_Concrete", "MI_VHV_WA_peugfls0"),
    ),
    "ThaiInfra_I6B_StepCap_1": (
        (0, "ThaiInfra_Concrete", "MI_VHV_WA_peugfls0"),
    ),
}


def _package_path(asset):
    return asset.get_path_name().split(".", 1)[0] if asset else ""


def _class_name(asset_data):
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        return str(asset_data.asset_class)


def _safe_get(obj, property_name, default=None):
    try:
        return obj.get_editor_property(property_name)
    except Exception:
        return default


def _slot_signature(mesh):
    return tuple(
        str(_safe_get(slot, "imported_material_slot_name", "") or "")
        for slot in (_safe_get(mesh, "static_materials", []) or [])
    )


def _saved_folder():
    return os.path.normpath(os.path.join(unreal.Paths.project_saved_dir(), SAVED_FOLDER))


def _write_unique_json(prefix, timestamp, data):
    folder = _saved_folder()
    os.makedirs(folder, exist_ok=True)
    path = os.path.normpath(os.path.join(folder, "{}_{}.json".format(prefix, timestamp)))
    with open(path, "x", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
    return path


def _discover_meshes(report):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    asset_data_items = list(registry.get_assets_by_path(MESH_ROOT, recursive=True))
    exact_matches = {name: [] for name in EXACT_MESH_NAMES}
    stone_matches = []

    for asset_data in asset_data_items:
        if _class_name(asset_data) != "StaticMesh":
            continue
        name = str(asset_data.asset_name)
        if name in exact_matches:
            exact_matches[name].append(asset_data)
        if name.endswith(STONE_SUFFIX) and name.startswith("ThaiInfra"):
            stone_matches.append(asset_data)

    meshes = {}
    for name in EXACT_MESH_NAMES:
        matches = exact_matches[name]
        if len(matches) != 1:
            report["errors"].append(
                "Expected one exact StaticMesh named {}; found {}".format(name, len(matches)))
            continue
        mesh = matches[0].get_asset()
        if not mesh or not isinstance(mesh, unreal.StaticMesh):
            report["errors"].append("Could not load StaticMesh {}".format(name))
            continue
        meshes[name] = mesh

    if len(stone_matches) != 1:
        report["errors"].append(
            "Expected one ThaiInfra StaticMesh ending with {}; found {}".format(
                STONE_SUFFIX, len(stone_matches)))
    else:
        stone = stone_matches[0].get_asset()
        if not stone or not isinstance(stone, unreal.StaticMesh):
            report["errors"].append("Could not load unique ThaiInfra {} mesh".format(STONE_SUFFIX))
        else:
            meshes["stone"] = stone
            report["resolved_stone_mesh"] = _package_path(stone)
    return meshes


def _load_materials(report):
    names = sorted({entry[2] for entries in ASSIGNMENTS.values() for entry in entries})
    materials = {}
    for name in names:
        path = "{}/{}".format(MATERIAL_ROOT, name)
        material = unreal.EditorAssetLibrary.load_asset(path)
        if not material or not isinstance(material, unreal.MaterialInstanceConstant):
            report["errors"].append("Missing MaterialInstanceConstant {}".format(path))
            continue
        materials[name] = material
    return materials


def _validate_and_plan(meshes, materials, report):
    if len(meshes) != 6:
        report["errors"].append("Validation requires exactly six resolved target meshes")
        return []

    for key, mesh in meshes.items():
        expected = EXPECTED_SIGNATURES[key]
        actual = _slot_signature(mesh)
        if actual != expected:
            report["errors"].append(
                "Slot signature mismatch for {}: expected {}; found {}".format(
                    _package_path(mesh), list(expected), list(actual)))

    if report["errors"]:
        return []

    plan = []
    mapped_by_mesh = {}
    for mesh_key, entries in ASSIGNMENTS.items():
        mesh = meshes[mesh_key]
        slots = _safe_get(mesh, "static_materials", []) or []
        mapped_by_mesh[_package_path(mesh)] = {entry[0] for entry in entries}
        for index, imported_name, material_name in entries:
            slot = slots[index]
            actual_imported = str(_safe_get(slot, "imported_material_slot_name", "") or "")
            if actual_imported != imported_name:
                report["errors"].append(
                    "Imported slot mismatch for {}[{}]".format(_package_path(mesh), index))
                continue
            replacement = materials.get(material_name)
            if not replacement:
                report["errors"].append(
                    "Validated replacement is unavailable: {}/{}".format(MATERIAL_ROOT, material_name))
                continue
            current = _safe_get(slot, "material_interface")
            record = {
                "mesh": _package_path(mesh),
                "slot_index": index,
                "slot_name": str(_safe_get(slot, "material_slot_name", "") or ""),
                "imported_slot_name": actual_imported,
                "original_material": _package_path(current),
                "replacement_material": _package_path(replacement),
            }
            if record["original_material"] == record["replacement_material"]:
                record["reason"] = "Already assigned correctly"
                report["unchanged_slots"].append(record)
            else:
                record["original_object"] = current
                record["replacement_object"] = replacement
                plan.append(record)

    for mesh in meshes.values():
        mesh_path = _package_path(mesh)
        mapped_indices = mapped_by_mesh.get(mesh_path, set())
        for index, slot in enumerate(_safe_get(mesh, "static_materials", []) or []):
            if index in mapped_indices:
                continue
            report["unchanged_slots"].append({
                "mesh": mesh_path,
                "slot_index": index,
                "slot_name": str(_safe_get(slot, "material_slot_name", "") or ""),
                "imported_slot_name": str(_safe_get(slot, "imported_material_slot_name", "") or ""),
                "original_material": _package_path(_safe_get(slot, "material_interface")),
                "reason": "KEEP: no approved replacement",
            })
    return [] if report["errors"] else plan


def _backup_snapshot(meshes, plan, timestamp):
    snapshots = []
    for mesh in sorted(meshes.values(), key=_package_path):
        slots = []
        for index, slot in enumerate(_safe_get(mesh, "static_materials", []) or []):
            slots.append({
                "slot_index": index,
                "slot_name": str(_safe_get(slot, "material_slot_name", "") or ""),
                "imported_slot_name": str(_safe_get(slot, "imported_material_slot_name", "") or ""),
                "original_material": _package_path(_safe_get(slot, "material_interface")),
            })
        snapshots.append({"mesh": _package_path(mesh), "slots": slots})
    serializable_plan = [
        {key: value for key, value in change.items() if not key.endswith("_object")}
        for change in plan
    ]
    return _write_unique_json(
        "VHV_FinalMaterialBackup", timestamp,
        {"schema": 1, "created_utc": timestamp, "meshes": snapshots, "planned_changes": serializable_plan})


def _apply(meshes, plan, report):
    by_mesh = {}
    for change in plan:
        by_mesh.setdefault(change["mesh"], []).append(change)

    applied = []
    for mesh_path, changes in sorted(by_mesh.items()):
        mesh = next(mesh for mesh in meshes.values() if _package_path(mesh) == mesh_path)
        mesh.modify()
        for change in changes:
            mesh.set_material(change["slot_index"], change["replacement_object"])
            slots = _safe_get(mesh, "static_materials", []) or []
            assigned = _safe_get(slots[change["slot_index"]], "material_interface")
            if _package_path(assigned) != change["replacement_material"]:
                report["failed_slots"].append({
                    "mesh": mesh_path,
                    "slot_index": change["slot_index"],
                    "reason": "Assignment read-back verification failed",
                })
            else:
                applied.append(change)

    if report["failed_slots"]:
        for change in plan:
            mesh = next(mesh for mesh in meshes.values() if _package_path(mesh) == change["mesh"])
            mesh.set_material(change["slot_index"], change["original_object"])
        report["errors"].append("Assignment failed; all in-memory changes were restored and nothing was saved")
        return

    for mesh_path, changes in sorted(by_mesh.items()):
        mesh = next(mesh for mesh in meshes.values() if _package_path(mesh) == mesh_path)
        if unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
            report["changed_slots"].extend(
                {key: value for key, value in change.items() if not key.endswith("_object")}
                for change in changes)
            report["updated_meshes"].append(mesh_path)
        else:
            for change in changes:
                mesh.set_material(change["slot_index"], change["original_object"])
            report["errors"].append("Could not save {}; its in-memory assignments were restored".format(mesh_path))
            report["failed_slots"].extend({
                "mesh": mesh_path,
                "slot_index": change["slot_index"],
                "reason": "Static Mesh save failed",
            } for change in changes)


def run():
    timestamp = datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%S%fZ")
    report = {
        "mode": "direct_apply",
        "timestamp_utc": timestamp,
        "resolved_stone_mesh": "",
        "backup_path": "",
        "updated_meshes": [],
        "changed_slots": [],
        "unchanged_slots": [],
        "failed_slots": [],
        "errors": [],
    }

    meshes = _discover_meshes(report)
    materials = _load_materials(report)
    plan = _validate_and_plan(meshes, materials, report)

    if report["errors"]:
        report_path = _write_unique_json("VHV_FinalMaterialReport", timestamp, report)
        unreal.log_error("{} Validation failed; no assets changed. Report={}".format(
            LOG_PREFIX, report_path))
        return report

    if plan:
        report["backup_path"] = _backup_snapshot(meshes, plan, timestamp)
        _apply(meshes, plan, report)

    report_path = _write_unique_json("VHV_FinalMaterialReport", timestamp, report)
    unreal.log(
        "{} Complete: meshes_updated={}, changed={}, unchanged={}, failed={}, errors={}; report={}".format(
            LOG_PREFIX, len(report["updated_meshes"]), len(report["changed_slots"]),
            len(report["unchanged_slots"]), len(report["failed_slots"]),
            len(report["errors"]), report_path))
    return report


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG_PREFIX, traceback.format_exc()))
        raise
