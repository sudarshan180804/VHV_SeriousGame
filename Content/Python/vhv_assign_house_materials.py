"""Safely assign existing VHV World-Aligned instances to H1-H8."""

import datetime
import json
import os
import re
import traceback

import unreal


DRY_RUN = False
APPLY_CHANGES = True

MESH_ROOT = "/Game/3D_Models"
MATERIAL_ROOT = "/Game/Materials"
HOUSE_NAMES = tuple("H{}".format(index) for index in range(1, 9))
LOG_PREFIX = "[VHV House Materials]"


def _rule(material, expected, role, warning=""):
    return {
        "material": material,
        "expected": tuple(expected),
        "role": role,
        "visual_warning": warning,
    }


# Imported-slot semantics are required for every replacement. H4 slots 0-2
# were identified from the mesh package and follow the user's strict rules.
RULES = {
    "H1": {
        0: _rule("MI_VHV_WA_peugfls0", ("concrete",), "Concrete/foundation/pillars"),
        1: _rule("MI_VHV_WA_wi0oadulw", ("warm cream",), "Main plaster walls", "Warm plaster color may change."),
        2: _rule("MI_VHV_WA_shlabh0c", ("dark metal",), "Metal/window frames"),
        4: _rule("MI_VHV_WA_whnfbg3", ("wood placeholder",), "Wooden doors/bench"),
        5: _rule("MI_VHV_WA_vd3kcjs", ("roof placeholder",), "Roof", "Roof color may change."),
        6: _rule("MI_VHV_WA_vhqkeff", ("faded peach",), "Faded lower plaster", "Faded peach color may change."),
    },
    "H2": {
        0: _rule("MI_VHV_WA_peugfls0", ("concrete",), "Concrete/foundation/stairs"),
        1: _rule("MI_VHV_WA_wckqdbvs", ("mat timber",), "Upper timber walls/balcony"),
        2: _rule("MI_VHV_WA_wi0oadulw", ("warm plaster",), "Lower plaster walls", "Warm plaster color may change."),
        3: _rule("MI_VHV_WA_shlabh0c", ("mat metal",), "Metal frames/railings"),
        5: _rule("MI_VHV_WA_whnfbg3", ("timber dark",), "Wooden door/bench"),
    },
    "H3": {
        0: _rule("MI_VHV_WA_peugfls0", ("old concrete",), "Old concrete"),
        1: _rule("MI_VHV_WA_whnfbg3", ("weathered dark wood",), "Weathered timber"),
        2: _rule("MI_VHV_WA_wckqdbvs", ("weathered wood",), "Secondary weathered timber"),
        3: _rule("MI_VHV_WA_veviddydy", ("aged light wood",), "Light aged wood", "Light wood color may change."),
        5: _rule("MI_VHV_WA_vcenefcew", ("rusty corrugated roof",), "Rusty corrugated roof", "Roof color may change."),
        6: _rule("MI_VHV_WA_rmmodbdp", ("dark metal",), "Dark metal"),
    },
    "H4": {
        0: _rule("MI_VHV_WA_peugfls0", ("mat concrete",), "Concrete"),
        1: _rule("MI_VHV_WA_vhqkeff", ("aged warm plaster",), "Aged warm plaster", "Warm plaster color may change."),
        2: _rule("MI_VHV_WA_whnfbg3", ("weathered wood",), "Weathered wood"),
        3: _rule("MI_VHV_WA_vhqkeff", ("faded plaster",), "Faded plaster", "Faded plaster color may change."),
        4: _rule("MI_VHV_WA_whnfbg3", ("dark wood",), "Dark wood"),
        5: _rule("MI_VHV_WA_rmmodbdp", ("dark metal",), "Dark metal"),
        7: _rule("MI_VHV_WA_sf4jce1b", ("metal awning",), "Metal awning", "Awning color may change."),
        8: _rule("MI_VHV_WA_tduwejdc", ("aged metal roof",), "Aged metal", "Aged metal color may change."),
        10: _rule("MI_VHV_WA_veviddydy", ("light aged wood",), "Light aged wood", "Light wood color may change."),
    },
    "H5": {
        1: _rule("MI_VHV_WA_peugfls0", ("cement block",), "Cement"),
        2: _rule("MI_VHV_WA_peugfls0", ("raw concrete",), "Raw concrete"),
        4: _rule("MI_VHV_WA_peugfls0", ("dark concrete",), "Dark concrete"),
        5: _rule("MI_VHV_WA_wckqdbvs", ("scaffold plank",), "Wooden scaffolding"),
        6: _rule("MI_VHV_WA_pjBkT0", ("unfinished plaster",), "Unfinished wall"),
        8: _rule("MI_VHV_WA_teendf3q", ("corrugated roof",), "Corrugated sheet"),
        9: _rule("MI_VHV_WA_shlabh0c", ("mat metal",), "Metal"),
    },
    "H6": {
        0: _rule("MI_VHV_WA_peugfls0", ("footing concrete",), "Footing/concrete"),
        1: _rule("MI_VHV_WA_whnfbg3", ("dark structure",), "Dark structural wood"),
        2: _rule("MI_VHV_WA_wckqdbvs", ("main timber",), "Main timber"),
        4: _rule("MI_VHV_WA_veviddydy", ("decorative timber",), "Decorative timber", "Decorative timber color may change."),
    },
    "H7": {
        0: _rule("MI_VHV_WA_peugfls0", ("mat concrete",), "Concrete"),
        1: _rule("MI_VHV_WA_pjBkT0", ("warm plaster",), "Warm plaster", "Warm plaster color may change."),
        2: _rule("MI_VHV_WA_wi0oadulw", ("light plaster",), "Light plaster", "Light plaster color may change."),
        4: _rule("MI_VHV_WA_whnfbg3", ("dark timber",), "Dark timber"),
        5: _rule("MI_VHV_WA_wckqdbvs", ("warm timber",), "Warm timber", "Warm timber color may change."),
        7: _rule("MI_VHV_WA_shlabh0c", ("dark gate metal",), "Dark gate metal", "Gate color may change."),
    },
    "H8": {
        0: _rule("MI_VHV_WA_peugfls0", ("mat concrete",), "Concrete"),
        1: _rule("MI_VHV_WA_pjBkT0", ("warm cream plaster",), "Warm cream plaster", "Cream color may change."),
        4: _rule("MI_VHV_WA_whnfbg3", ("dark wood",), "Dark wood"),
        5: _rule("MI_VHV_WA_wckqdbvs", ("door wood",), "Door wood"),
        6: _rule("MI_VHV_WA_shlabh0c", ("dark metal",), "Dark metal"),
    },
}

KEEP_SLOTS = {
    "H1": {3: "Glass"},
    "H2": {4: "Glass", 6: "Terracotta roof"},
    "H3": {4: "Dark glass"},
    "H4": {6: "Dark glass", 9: "Signboard"},
    "H5": {0: "Cement bags", 3: "Bamboo", 7: "Green construction fabric/fence", 10: "Safety material", 11: "Sand"},
    "H6": {3: "Dark window/glass", 5: "Roof is tiles, not confirmed sheet metal", 6: "Roof trim is not confirmed timber"},
    "H7": {3: "Dark glass", 6: "Roof", 8: "Black material", 9: "Pot"},
    "H8": {2: "Pale-green plaster", 3: "Dark glass", 7: "Roof is not confirmed corrugated metal", 8: "Green gable finish", 9: "Water tank is not confirmed metallic"},
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


def _normalized(value):
    value = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", str(value))
    return re.sub(r"[^a-z0-9]+", " ", value.lower()).strip()


def _slot_details(slot):
    imported = str(_safe_get(slot, "imported_material_slot_name", "") or "")
    slot_name = str(_safe_get(slot, "material_slot_name", "") or "")
    identity = imported if imported and imported.lower() != "none" else slot_name
    return imported, slot_name, _normalized(identity)


def _find_houses(report):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    matches = {name: [] for name in HOUSE_NAMES}
    for asset_data in registry.get_assets_by_path(MESH_ROOT, recursive=True):
        name = str(asset_data.asset_name)
        if name in matches and _class_name(asset_data) == "StaticMesh":
            matches[name].append(asset_data)

    houses = {}
    for name in HOUSE_NAMES:
        found = matches[name]
        if len(found) != 1:
            report["errors"].append(
                "Expected one StaticMesh named {}; found {}".format(name, len(found)))
            continue
        mesh = found[0].get_asset()
        if not mesh or not isinstance(mesh, unreal.StaticMesh):
            report["errors"].append("Could not load StaticMesh {}".format(name))
            continue
        houses[name] = mesh
    return houses


def _load_targets(report):
    names = sorted({rule["material"] for house_rules in RULES.values() for rule in house_rules.values()})
    targets = {}
    for name in names:
        path = "{}/{}".format(MATERIAL_ROOT, name)
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset or not isinstance(asset, unreal.MaterialInstanceConstant):
            report["errors"].append("Missing MaterialInstanceConstant {}".format(path))
            continue
        targets[name] = asset
    return targets


def _build_plan(houses, targets, report):
    plan = []
    for house_name in HOUSE_NAMES:
        mesh = houses.get(house_name)
        if not mesh:
            continue
        slots = _safe_get(mesh, "static_materials", []) or []
        report["slot_counts"][house_name] = len(slots)
        rules = RULES[house_name]
        keep = KEEP_SLOTS.get(house_name, {})

        for index, slot in enumerate(slots):
            imported, slot_name, identity = _slot_details(slot)
            current = _safe_get(slot, "material_interface")
            current_path = _package_path(current)
            base = {
                "mesh": _package_path(mesh),
                "house": house_name,
                "slot_index": index,
                "slot_name": slot_name,
                "imported_slot_name": imported,
                "original_material": current_path,
            }

            if index in keep:
                unchanged = dict(base)
                unchanged["reason"] = "KEEP: {}".format(keep[index])
                report["unchanged_slots"].append(unchanged)
                continue
            rule = rules.get(index)
            if not rule:
                unchanged = dict(base)
                unchanged["reason"] = "No approved mapping"
                report["unchanged_slots"].append(unchanged)
                continue
            target = targets.get(rule["material"])
            if not target:
                skipped = dict(base)
                skipped["reason"] = "Replacement asset is missing"
                skipped["requested_material"] = "{}/{}".format(MATERIAL_ROOT, rule["material"])
                report["skipped_entries"].append(skipped)
                continue
            if not identity or not any(_normalized(expected) in identity for expected in rule["expected"]):
                skipped = dict(base)
                skipped["reason"] = "Imported slot identity did not match expected semantics: {}".format(
                    ", ".join(rule["expected"]))
                skipped["requested_material"] = _package_path(target)
                report["skipped_entries"].append(skipped)
                continue
            if current_path == _package_path(target):
                unchanged = dict(base)
                unchanged["reason"] = "Already assigned correctly"
                report["unchanged_slots"].append(unchanged)
                continue

            change = dict(base)
            change.update({
                "replacement_material": _package_path(target),
                "semantic_role": rule["role"],
                "visual_warning": rule["visual_warning"],
            })
            plan.append(change)
    return plan


def _saved_folder():
    return os.path.normpath(os.path.join(
        unreal.Paths.project_saved_dir(), "VHV_MaterialAssignments"))


def _write_json(file_name, data):
    folder = _saved_folder()
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, file_name)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
    return os.path.normpath(path)


def _apply(plan, houses, targets, report, timestamp):
    backup = {
        "schema": 1,
        "created_utc": timestamp,
        "changes": plan,
    }
    report["backup_path"] = _write_json(
        "VHV_HouseMaterialBackup_{}.json".format(timestamp), backup)

    target_by_path = {_package_path(asset): asset for asset in targets.values()}
    by_house = {}
    for change in plan:
        by_house.setdefault(change["house"], []).append(change)

    for house_name, changes in sorted(by_house.items()):
        mesh = houses[house_name]
        modified = False
        for change in sorted(changes, key=lambda item: item["slot_index"]):
            slots = _safe_get(mesh, "static_materials", []) or []
            index = change["slot_index"]
            current = _safe_get(slots[index], "material_interface") if index < len(slots) else None
            if _package_path(current) != change["original_material"]:
                report["errors"].append(
                    "Slot changed after validation: {}[{}]".format(house_name, index))
                continue
            replacement = target_by_path.get(change["replacement_material"])
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
        "slot_counts": {},
        "explicit_mapping_counts": {name: len(RULES[name]) for name in HOUSE_NAMES},
        "replacement_plan": [],
        "updated_meshes": [],
        "changed_slots": [],
        "unchanged_slots": [],
        "skipped_entries": [],
        "errors": [],
        "backup_path": "",
    }

    houses = _find_houses(report)
    targets = _load_targets(report)
    plan = _build_plan(houses, targets, report)
    report["replacement_plan"] = plan

    unreal.log("{} Exact replacement plan ({} slots):".format(LOG_PREFIX, len(plan)))
    for change in plan:
        warning = " WARNING: {}".format(change["visual_warning"]) if change["visual_warning"] else ""
        unreal.log("{} {}[{}] {} -> {}{}".format(
            LOG_PREFIX, change["house"], change["slot_index"],
            change["original_material"], change["replacement_material"], warning))

    if applying:
        _apply(plan, houses, targets, report, timestamp)
    elif not DRY_RUN and not APPLY_CHANGES:
        report["errors"].append(
            "Apply blocked: APPLY_CHANGES must also be True when DRY_RUN is False")

    report_path = _write_json(
        "VHV_HouseMaterialReport_{}.json".format(timestamp), report)
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
