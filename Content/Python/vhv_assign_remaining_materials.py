"""Assign existing VHV materials to P7, p8, and verified ThaiInfra meshes."""

import datetime
import json
import os
import traceback

import unreal


DRY_RUN = False
APPLY_CHANGES = True

MESH_ROOT = "/Game/3D_Models"
MATERIAL_ROOT = "/Game/Materials"
SAVED_FOLDER = "VHV_RemainingMaterialAssignments"
LOG_PREFIX = "[VHV Remaining Materials]"


def _rule(index, imported_name, material_name, role):
    return {
        "index": index,
        "imported_name": imported_name,
        "material_name": material_name,
        "role": role,
    }


P_RULES = {
    "P7": (
        _rule(1, "ThaiCommercial_Concrete", "MI_VHV_WA_peugfls0", "Concrete"),
        _rule(2, "ThaiCommercial_DarkMetal", "MI_VHV_WA_rmmodbdp", "Dark metal"),
        _rule(3, "ThaiCommercial_DarkWood", "MI_VHV_WA_whnfbg3", "Dark wood"),
        _rule(4, "ThaiCommercial_Wood", "MI_VHV_WA_wckqdbvs", "Wood"),
        _rule(7, "ThaiCommercial_ShutterMetal", "MI_VHV_WA_shlabh0c", "Shutter metal"),
        _rule(10, "ThaiCommercial_CreamPlaster", "MI_VHV_WA_pjBkT0", "Cream plaster"),
    ),
    "p8": (
        _rule(0, "ThaiCommercial_CreamPlaster", "MI_VHV_WA_pjBkT0", "Cream plaster"),
        _rule(2, "ThaiCommercial_DarkMetal", "MI_VHV_WA_rmmodbdp", "Dark metal"),
        _rule(5, "ThaiCommercial_Concrete", "MI_VHV_WA_peugfls0", "Concrete"),
        _rule(10, "ThaiCommercial_Wood", "MI_VHV_WA_wckqdbvs", "Wood"),
    ),
}


# Infrastructure targets are resolved by a unique, complete, ordered imported
# slot signature from the one bounded ThaiInfra registry query.
INFRA_SPECS = {
    "streetlight_concrete_pole": {
        "signature": (
            "ThaiInfra_Concrete", "ThaiInfra_ConcreteDark", "ThaiInfra_DarkMetal",
            "ThaiInfra_LampHousing", "ThaiInfra_LampLens"),
        "rules": (
            _rule(0, "ThaiInfra_Concrete", "MI_VHV_WA_peugfls0", "Concrete"),
            _rule(2, "ThaiInfra_DarkMetal", "MI_VHV_WA_rmmodbdp", "Dark metal"),
        ),
    },
    "straight_light_pole": {
        "signature": (
            "ThaiInfra_ConcreteDark", "ThaiInfra_DarkMetal",
            "ThaiInfra_LampHousing", "ThaiInfra_GalvanizedMetal"),
        "rules": (
            _rule(1, "ThaiInfra_DarkMetal", "MI_VHV_WA_rmmodbdp", "Dark metal"),
        ),
    },
    "lamp_pole_assembly": {
        "signature": ("ThaiInfra_Concrete", "ThaiInfra_DarkMetal", "ThaiInfra_LampHousing"),
        "rules": (
            _rule(0, "ThaiInfra_Concrete", "MI_VHV_WA_peugfls0", "Concrete"),
            _rule(1, "ThaiInfra_DarkMetal", "MI_VHV_WA_rmmodbdp", "Dark metal"),
        ),
    },
    "metal_railing": {
        "signature": ("ThaiInfra_DarkMetal",),
        "rules": (
            _rule(0, "ThaiInfra_DarkMetal", "MI_VHV_WA_rmmodbdp", "Dark metal"),
        ),
    },
    "mixed_concrete_metal_post": {
        "signature": ("ThaiInfra_Concrete", "ThaiInfra_ConcreteDark", "ThaiInfra_DarkMetal"),
        "rules": (
            _rule(0, "ThaiInfra_Concrete", "MI_VHV_WA_peugfls0", "Concrete"),
            _rule(2, "ThaiInfra_DarkMetal", "MI_VHV_WA_rmmodbdp", "Dark metal"),
        ),
    },
    "mixed_concrete_black_fence": {
        "signature": ("ThaiInfra_Concrete", "ThaiInfra_ConcreteDark", "ThaiInfra_Black"),
        "rules": (
            _rule(0, "ThaiInfra_Concrete", "MI_VHV_WA_peugfls0", "Concrete"),
        ),
    },
    "small_concrete_block": {
        "signature": ("ThaiInfra_Concrete", "ThaiInfra_ConcreteLight"),
        "rules": (
            _rule(0, "ThaiInfra_Concrete", "MI_VHV_WA_peugfls0", "Concrete"),
        ),
    },
    "signboard": {
        "signature": ("ThaiInfra_DarkMetal", "ThaiInfra_SignBoard"),
        "rules": (
            _rule(0, "ThaiInfra_DarkMetal", "MI_VHV_WA_rmmodbdp", "Dark metal"),
        ),
    },
    "stone_structure": {
        "signature": ("ThaiInfra_ConcreteDark", "ThaiInfra_Stone", "ThaiInfra_StoneLight"),
        "rules": (),
    },
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
    result = []
    for slot in _safe_get(mesh, "static_materials", []) or []:
        result.append(str(_safe_get(slot, "imported_material_slot_name", "") or ""))
    return tuple(result)


def _discover_targets(report):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = list(registry.get_assets_by_path(MESH_ROOT, recursive=True))

    p_data = {"P7": [], "p8": []}
    infra_meshes = []
    for asset_data in assets:
        if _class_name(asset_data) != "StaticMesh":
            continue
        name = str(asset_data.asset_name)
        if name in p_data:
            p_data[name].append(asset_data)
        elif name.startswith("ThaiInfra"):
            mesh = asset_data.get_asset()
            if mesh:
                infra_meshes.append(mesh)

    targets = {}
    for name in ("P7", "p8"):
        matches = p_data[name]
        if len(matches) != 1:
            report["errors"].append(
                "Expected one exact StaticMesh named {}; found {}".format(name, len(matches)))
            continue
        mesh = matches[0].get_asset()
        if not mesh or not isinstance(mesh, unreal.StaticMesh):
            report["errors"].append("Could not load exact StaticMesh {}".format(name))
            continue
        targets[name] = {"mesh": mesh, "rules": P_RULES[name], "kind": "commercial"}

    by_signature = {}
    for mesh in infra_meshes:
        by_signature.setdefault(_slot_signature(mesh), []).append(mesh)

    for label, spec in INFRA_SPECS.items():
        matches = sorted(by_signature.get(spec["signature"], []), key=_package_path)
        paths = [_package_path(mesh) for mesh in matches]
        if len(matches) == 1:
            key = "infra:{}".format(label)
            targets[key] = {"mesh": matches[0], "rules": spec["rules"], "kind": label}
            report["resolved_infrastructure"][label] = paths[0]
        elif len(matches) > 1:
            report["ambiguous_infrastructure"].append({
                "target": label,
                "signature": list(spec["signature"]),
                "matches": paths,
                "reason": "Multiple ThaiInfra meshes share the screenshot slot signature",
            })
        else:
            report["skipped_entries"].append({
                "target": label,
                "signature": list(spec["signature"]),
                "reason": "No ThaiInfra mesh matched the screenshot slot signature",
            })

    matched_paths = {_package_path(item["mesh"]) for item in targets.values() if item["kind"] != "commercial"}
    ambiguous_paths = {
        path for entry in report["ambiguous_infrastructure"] for path in entry["matches"]
    }
    report["unmatched_infrastructure"] = sorted(
        _package_path(mesh) for mesh in infra_meshes
        if _package_path(mesh) not in matched_paths and _package_path(mesh) not in ambiguous_paths)
    return targets


def _load_materials(report):
    names = sorted({
        rule["material_name"]
        for rules in list(P_RULES.values()) + [spec["rules"] for spec in INFRA_SPECS.values()]
        for rule in rules
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


def _build_plan(targets, materials, report):
    plan = []
    for target_key, target in sorted(targets.items()):
        mesh = target["mesh"]
        slots = _safe_get(mesh, "static_materials", []) or []
        report["slot_counts"][_package_path(mesh)] = len(slots)
        mapped_indices = {rule["index"] for rule in target["rules"]}

        for index, slot in enumerate(slots):
            imported_name = str(_safe_get(slot, "imported_material_slot_name", "") or "")
            slot_name = str(_safe_get(slot, "material_slot_name", "") or "")
            current = _safe_get(slot, "material_interface")
            common = {
                "mesh": _package_path(mesh),
                "target": target_key,
                "slot_index": index,
                "slot_name": slot_name,
                "imported_slot_name": imported_name,
                "original_material": _package_path(current),
            }
            if index not in mapped_indices:
                unchanged = dict(common)
                unchanged["reason"] = "KEEP: no approved mapping"
                report["unchanged_slots"].append(unchanged)

        for rule in target["rules"]:
            index = rule["index"]
            if index >= len(slots):
                report["skipped_entries"].append({
                    "mesh": _package_path(mesh),
                    "slot_index": index,
                    "expected_imported_slot_name": rule["imported_name"],
                    "reason": "Mapped slot index is outside the mesh slot count",
                })
                continue
            slot = slots[index]
            imported_name = str(_safe_get(slot, "imported_material_slot_name", "") or "")
            slot_name = str(_safe_get(slot, "material_slot_name", "") or "")
            current = _safe_get(slot, "material_interface")
            current_path = _package_path(current)
            target_material = materials.get(rule["material_name"])
            requested_path = "{}/{}".format(MATERIAL_ROOT, rule["material_name"])
            common = {
                "mesh": _package_path(mesh),
                "target": target_key,
                "slot_index": index,
                "slot_name": slot_name,
                "imported_slot_name": imported_name,
                "original_material": current_path,
            }
            if not target_material:
                skipped = dict(common)
                skipped.update({"requested_material": requested_path, "reason": "Replacement material is missing"})
                report["skipped_entries"].append(skipped)
                continue
            if imported_name != rule["imported_name"]:
                skipped = dict(common)
                skipped.update({
                    "requested_material": _package_path(target_material),
                    "expected_imported_slot_name": rule["imported_name"],
                    "reason": "Exact imported slot name mismatch",
                })
                report["skipped_entries"].append(skipped)
                continue
            if current_path == _package_path(target_material):
                unchanged = dict(common)
                unchanged["reason"] = "Already assigned correctly"
                report["unchanged_slots"].append(unchanged)
                continue
            change = dict(common)
            change.update({
                "replacement_material": _package_path(target_material),
                "semantic_role": rule["role"],
            })
            plan.append(change)
    return plan


def _saved_folder():
    return os.path.normpath(os.path.join(unreal.Paths.project_saved_dir(), SAVED_FOLDER))


def _write_json(file_name, data, exclusive=False):
    folder = _saved_folder()
    os.makedirs(folder, exist_ok=True)
    path = os.path.normpath(os.path.join(folder, file_name))
    with open(path, "x" if exclusive else "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
    return path


def _apply(plan, targets, materials, report, timestamp):
    report["backup_path"] = _write_json(
        "VHV_RemainingMaterialBackup_{}.json".format(timestamp),
        {"schema": 1, "created_utc": timestamp, "changes": plan},
        exclusive=True)

    meshes = {_package_path(item["mesh"]): item["mesh"] for item in targets.values()}
    materials_by_path = {_package_path(material): material for material in materials.values()}
    changes_by_mesh = {}
    for change in plan:
        changes_by_mesh.setdefault(change["mesh"], []).append(change)

    for mesh_path, changes in sorted(changes_by_mesh.items()):
        mesh = meshes[mesh_path]
        modified = False
        for change in sorted(changes, key=lambda item: item["slot_index"]):
            slots = _safe_get(mesh, "static_materials", []) or []
            index = change["slot_index"]
            current = _safe_get(slots[index], "material_interface") if index < len(slots) else None
            if _package_path(current) != change["original_material"]:
                report["errors"].append("Slot changed after validation: {}[{}]".format(mesh_path, index))
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
                report["updated_meshes"].append(mesh_path)
            else:
                report["errors"].append("Could not save {}".format(mesh_path))


def run():
    timestamp = datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%S%fZ")
    applying = not DRY_RUN and APPLY_CHANGES
    report = {
        "mode": "apply" if applying else "dry_run",
        "timestamp_utc": timestamp,
        "settings": {"DRY_RUN": DRY_RUN, "APPLY_CHANGES": APPLY_CHANGES},
        "resolved_infrastructure": {},
        "ambiguous_infrastructure": [],
        "unmatched_infrastructure": [],
        "slot_counts": {},
        "replacement_plan": [],
        "updated_meshes": [],
        "changed_slots": [],
        "unchanged_slots": [],
        "skipped_entries": [],
        "errors": [],
        "backup_path": "",
    }

    targets = _discover_targets(report)
    materials = _load_materials(report)
    plan = _build_plan(targets, materials, report)
    report["replacement_plan"] = plan

    unreal.log("{} Exact replacement plan ({} slots):".format(LOG_PREFIX, len(plan)))
    for change in plan:
        unreal.log("{} {}[{}] {} -> {}".format(
            LOG_PREFIX, change["mesh"], change["slot_index"],
            change["original_material"], change["replacement_material"]))

    if applying:
        _apply(plan, targets, materials, report, timestamp)
    elif not DRY_RUN and not APPLY_CHANGES:
        report["errors"].append(
            "Apply blocked: APPLY_CHANGES must also be True when DRY_RUN is False")

    report_path = _write_json(
        "VHV_RemainingMaterialReport_{}.json".format(timestamp), report)
    unreal.log(
        "{} {} complete: plan={}, changed={}, unchanged={}, skipped={}, ambiguous={}, errors={}; report={}".format(
            LOG_PREFIX, report["mode"], len(plan), len(report["changed_slots"]),
            len(report["unchanged_slots"]), len(report["skipped_entries"]),
            len(report["ambiguous_infrastructure"]), len(report["errors"]), report_path))
    return report


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG_PREFIX, traceback.format_exc()))
        raise
