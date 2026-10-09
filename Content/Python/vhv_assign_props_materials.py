"""Assign existing VHV World-Aligned instances to P1-P6 safely."""

import datetime
import json
import os
import traceback

import unreal


DRY_RUN = False
APPLY_CHANGES = True

MESH_ROOT = "/Game/3D_Models"
MATERIAL_ROOT = "/Game/Materials"
SAVED_FOLDER = "VHV_PropMaterialAssignments"
PROP_NAMES = ("P1", "P2", "P3", "P4", "P5", "P6")
LOG_PREFIX = "[VHV Prop Materials]"


def _rule(imported_name, material_name, role):
    return {
        "imported_name": imported_name,
        "material_name": material_name,
        "role": role,
    }


RULES = {
    "P1": {
        0: _rule("P1Thai_MAT_Concrete", "MI_VHV_WA_peugfls0", "Concrete"),
        1: _rule("P1Thai_MAT_AgedCreamPlaster", "MI_VHV_WA_vhqkeff", "Aged cream plaster"),
        4: _rule("P1Thai_MAT_DarkMetal", "MI_VHV_WA_shlabh0c", "Dark metal"),
    },
    "P2": {
        1: _rule("P2Thai_MAT_Concrete", "MI_VHV_WA_peugfls0", "Concrete"),
        3: _rule("P2Thai_MAT_DarkTimber", "MI_VHV_WA_whnfbg3", "Dark timber"),
        4: _rule("P2Thai_MAT_Timber", "MI_VHV_WA_wckqdbvs", "Timber"),
        5: _rule("P2Thai_MAT_CorrugatedRoof", "MI_VHV_WA_teendf3q", "Corrugated roof"),
        14: _rule("P2Thai_MAT_Metal", "MI_VHV_WA_shlabh0c", "Metal"),
    },
    "P3": {
        1: _rule("P3Thai_MAT_Concrete", "MI_VHV_WA_peugfls0", "Concrete"),
        7: _rule("P3Thai_MAT_Wood", "MI_VHV_WA_wckqdbvs", "Wood"),
    },
    "P4": {
        0: _rule("P4Thai_MAT_DarkWood", "MI_VHV_WA_whnfbg3", "Dark wood"),
        2: _rule("P4Thai_MAT_Metal", "MI_VHV_WA_shlabh0c", "Metal"),
        8: _rule("P4Thai_MAT_MarketWood", "MI_VHV_WA_wckqdbvs", "Market wood"),
    },
    "P5": {
        0: _rule("P5Thai_MAT_DarkWood", "MI_VHV_WA_whnfbg3", "Dark wood"),
        1: _rule("P5Thai_MAT_CorrugatedRoof", "MI_VHV_WA_teendf3q", "Corrugated roof"),
        2: _rule("P5Thai_MAT_WeatheredWood", "MI_VHV_WA_wckqdbvs", "Weathered wood"),
        13: _rule("P5Thai_MAT_DarkMetal", "MI_VHV_WA_rmmodbdp", "Dark metal"),
        17: _rule("P5Thai_MAT_Concrete", "MI_VHV_WA_peugfls0", "Concrete"),
        22: _rule("P5Thai_MAT_AgedPlaster", "MI_VHV_WA_vhqkeff", "Aged plaster"),
    },
    "P6": {
        2: _rule("C1Thai_MAT_Metal", "MI_VHV_WA_shlabh0c", "Metal"),
        14: _rule("C1Thai_MAT_TableWood", "MI_VHV_WA_wckqdbvs", "Table wood"),
    },
}


def _package_path(asset):
    return asset.get_path_name().split(".", 1)[0] if asset else ""


def _safe_get(obj, property_name, default=None):
    try:
        return obj.get_editor_property(property_name)
    except Exception:
        return default


def _load_meshes(report):
    meshes = {}
    for name in PROP_NAMES:
        path = "{}/{}".format(MESH_ROOT, name)
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if not mesh or not isinstance(mesh, unreal.StaticMesh) or mesh.get_name() != name:
            report["errors"].append("Missing exact StaticMesh {}".format(path))
            continue
        meshes[name] = mesh
    return meshes


def _load_materials(report):
    names = sorted({
        rule["material_name"]
        for prop_rules in RULES.values()
        for rule in prop_rules.values()
    })
    materials = {}
    for name in names:
        path = "{}/{}".format(MATERIAL_ROOT, name)
        material = unreal.EditorAssetLibrary.load_asset(path)
        if not material or not isinstance(material, unreal.MaterialInstanceConstant):
            report["errors"].append("Missing MaterialInstanceConstant {}".format(path))
            continue
        materials[name] = material
    return materials


def _build_plan(meshes, materials, report):
    plan = []
    for prop_name in PROP_NAMES:
        mesh = meshes.get(prop_name)
        if not mesh:
            continue
        slots = _safe_get(mesh, "static_materials", []) or []
        report["slot_counts"][prop_name] = len(slots)
        rules = RULES[prop_name]

        for index, slot in enumerate(slots):
            imported_name = str(_safe_get(slot, "imported_material_slot_name", "") or "")
            slot_name = str(_safe_get(slot, "material_slot_name", "") or "")
            current = _safe_get(slot, "material_interface")
            current_path = _package_path(current)
            common = {
                "mesh": _package_path(mesh),
                "prop": prop_name,
                "slot_index": index,
                "slot_name": slot_name,
                "imported_slot_name": imported_name,
                "original_material": current_path,
            }
            rule = rules.get(index)
            if not rule:
                unchanged = dict(common)
                unchanged["reason"] = "KEEP: no approved mapping"
                report["unchanged_slots"].append(unchanged)
                continue

            target = materials.get(rule["material_name"])
            target_path = "{}/{}".format(MATERIAL_ROOT, rule["material_name"])
            if not target:
                skipped = dict(common)
                skipped.update({
                    "requested_material": target_path,
                    "reason": "Replacement material is missing",
                })
                report["skipped_entries"].append(skipped)
                continue
            if imported_name != rule["imported_name"]:
                skipped = dict(common)
                skipped.update({
                    "requested_material": _package_path(target),
                    "expected_imported_slot_name": rule["imported_name"],
                    "reason": "Exact imported slot name mismatch",
                })
                report["skipped_entries"].append(skipped)
                continue
            if current_path == _package_path(target):
                unchanged = dict(common)
                unchanged["reason"] = "Already assigned correctly"
                report["unchanged_slots"].append(unchanged)
                continue

            change = dict(common)
            change.update({
                "replacement_material": _package_path(target),
                "semantic_role": rule["role"],
            })
            plan.append(change)

        for index, rule in sorted(rules.items()):
            if index >= len(slots):
                report["skipped_entries"].append({
                    "mesh": _package_path(mesh),
                    "prop": prop_name,
                    "slot_index": index,
                    "expected_imported_slot_name": rule["imported_name"],
                    "requested_material": "{}/{}".format(MATERIAL_ROOT, rule["material_name"]),
                    "reason": "Mapped slot index is outside the mesh slot count",
                })
    return plan


def _saved_folder():
    return os.path.normpath(os.path.join(
        unreal.Paths.project_saved_dir(), SAVED_FOLDER))


def _write_json(file_name, data):
    folder = _saved_folder()
    os.makedirs(folder, exist_ok=True)
    path = os.path.normpath(os.path.join(folder, file_name))
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
    return path


def _apply(plan, meshes, materials, report, timestamp):
    report["backup_path"] = _write_json(
        "VHV_PropMaterialBackup_{}.json".format(timestamp),
        {"schema": 1, "created_utc": timestamp, "changes": plan})

    materials_by_path = {_package_path(material): material for material in materials.values()}
    changes_by_prop = {}
    for change in plan:
        changes_by_prop.setdefault(change["prop"], []).append(change)

    for prop_name, changes in sorted(changes_by_prop.items()):
        mesh = meshes[prop_name]
        modified = False
        for change in sorted(changes, key=lambda item: item["slot_index"]):
            slots = _safe_get(mesh, "static_materials", []) or []
            index = change["slot_index"]
            current = _safe_get(slots[index], "material_interface") if index < len(slots) else None
            if _package_path(current) != change["original_material"]:
                report["errors"].append(
                    "Slot changed after validation: {}[{}]".format(prop_name, index))
                continue
            replacement = materials_by_path.get(change["replacement_material"])
            if not replacement:
                report["errors"].append(
                    "Replacement unavailable during apply: {}".format(change["replacement_material"]))
                continue
            if not modified:
                mesh.modify()
            mesh.set_material(index, replacement)
            modified = True
            report["changed_slots"].append(change)
        if modified:
            if unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
                report["updated_meshes"].append(_package_path(mesh))
            else:
                report["errors"].append("Could not save {}".format(_package_path(mesh)))


def run():
    timestamp = datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
    applying = not DRY_RUN and APPLY_CHANGES
    report = {
        "mode": "apply" if applying else "dry_run",
        "timestamp_utc": timestamp,
        "settings": {"DRY_RUN": DRY_RUN, "APPLY_CHANGES": APPLY_CHANGES},
        "explicit_mapping_counts": {name: len(RULES[name]) for name in PROP_NAMES},
        "slot_counts": {},
        "replacement_plan": [],
        "updated_meshes": [],
        "changed_slots": [],
        "unchanged_slots": [],
        "skipped_entries": [],
        "errors": [],
        "backup_path": "",
    }

    meshes = _load_meshes(report)
    materials = _load_materials(report)
    plan = _build_plan(meshes, materials, report)
    report["replacement_plan"] = plan

    unreal.log("{} Exact replacement plan ({} slots):".format(LOG_PREFIX, len(plan)))
    for change in plan:
        unreal.log("{} {}[{}] {} -> {}".format(
            LOG_PREFIX, change["prop"], change["slot_index"],
            change["original_material"], change["replacement_material"]))

    if applying:
        _apply(plan, meshes, materials, report, timestamp)
    elif not DRY_RUN and not APPLY_CHANGES:
        report["errors"].append(
            "Apply blocked: APPLY_CHANGES must also be True when DRY_RUN is False")

    report_path = _write_json(
        "VHV_PropMaterialReport_{}.json".format(timestamp), report)
    unreal.log(
        "{} {} complete: plan={}, changed={}, unchanged={}, skipped={}, errors={}; report={}".format(
            LOG_PREFIX, report["mode"], len(plan), len(report["changed_slots"]),
            len(report["unchanged_slots"]), len(report["skipped_entries"]),
            len(report["errors"]), report_path))
    return report


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG_PREFIX, traceback.format_exc()))
        raise
