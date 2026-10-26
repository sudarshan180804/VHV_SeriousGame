"""Plan, apply, and verify the guarded 24-actor production vegetation pilot.

Modes are selected with VHV_STATIC_VEGETATION_PILOT_MODE:
  plan   - read-only selection and dependency/reference logging (default)
  smoke  - read-only UE 5.7 Python API/accessor validation
  diagnose_spawn - isolated benchmark-map spawn/assign/verify/destroy probe
  diagnose_scan  - read-only per-vegetation production transform probe
  apply  - transactional in-memory replacement, validation, then one map save
  verify_pilot - reload and independently audit the persisted 24-actor pilot
  plan_full - read-only plan/reference scan for the remaining 281 actors
  apply_full - bounded 266 replacements + authorized 15 deletions, then save
  verify_full - reload and independently audit the completed rollout

The default is deliberately read-only. Apply requires a reviewed GO JSON whose
selected actor list exactly matches the generated plan.
"""

import hashlib
import json
import os
import traceback

import unreal

import vhv_static_vegetation_validate_and_replace as pipeline


PROJECT = unreal.SystemLibrary.get_project_directory()
REPORT_DIR = os.path.join(PROJECT, "Saved", "Reports")
INVENTORY_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_PVE_Inventory.json")
VALIDATION_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_PVE_Validation.json")
PLAN_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Pilot_Plan.json")
GO_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Pilot_GO_20261026_FullRollout.json")
AUDIT_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Pilot_Audit_20261026_Retry.json")
SMOKE_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_API_Smoke.json")
RELOAD_AUDIT_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Pilot_Reload_Audit_20261026.json")
FULL_PLAN_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Full_Rollout_Plan.json")
FULL_GO_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Full_Rollout_GO.json")
FULL_AUDIT_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Full_Rollout_Audit.json")
FULL_RELOAD_AUDIT_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Full_Reload_Audit.json")
CHECKPOINT_MANIFEST = os.path.join(
    PROJECT, "Saved", "StaticVegetationCheckpoints", "PrePVEStatic-20261026-155547", "manifest.json")
MAP_ASSET = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
MAP_FILE = os.path.join(PROJECT, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
MODE = os.environ.get("VHV_STATIC_VEGETATION_PILOT_MODE", "plan").strip().lower()
SPECIES = ("Beach_Rose", "Black_Alder", "Common_Hazel", "Goat_Willow", "Greasewood", "Japanese_Medlar")
ANCHORS = {
    # Authored landmark locations discovered read-only in Lvl_Village_Main:
    # HBCT_Location_VillageEntrance and VM_P4_MainMarket.
    "entrance": [-730.38, 481.556, 150.0],
    "market": [-2350.0, 2610.0, 150.0],
}
LOG = "[VHV Static Vegetation Pilot]"


def _sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _load(path):
    with open(path, "r", encoding="utf-8") as stream:
        return json.load(stream)


def _write(path, value):
    with open(path, "w", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)


def _path(value):
    return value.get_path_name() if value else ""


def _object_path(value):
    """Return a normalized full object path for either a UObject or package path."""
    path = _path(value) if hasattr(value, "get_path_name") else str(value or "")
    leaf = path.rsplit("/", 1)[-1]
    if path.startswith("/") and "." not in leaf:
        path = "{}.{}".format(path, leaf)
    return path


def _vec(value):
    return [round(float(value.x), 6), round(float(value.y), 6), round(float(value.z), 6)]


def _rot(value):
    return [round(float(value.pitch), 6), round(float(value.yaw), 6), round(float(value.roll), 6)]


def _close(left, right, tolerance=0.0001):
    return len(left) == len(right) and all(abs(float(a) - float(b)) <= tolerance for a, b in zip(left, right))


def _value(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def _component(actor, cls):
    values = list(actor.get_components_by_class(cls))
    if len(values) != 1:
        raise RuntimeError("Expected one {} on {}".format(cls, actor.get_path_name()))
    return values[0]


def _checkpoint_gate():
    manifest = _load(CHECKPOINT_MANIFEST)
    if not manifest.get("verified") or not manifest.get("records"):
        raise RuntimeError("Checkpoint manifest is not verified")
    verified = []
    for record in manifest["records"]:
        source_hash = _sha256(record["source"])
        checkpoint_hash = _sha256(record["checkpoint"])
        if source_hash != record["sha256"] or checkpoint_hash != record["sha256"]:
            raise RuntimeError("Checkpoint hash gate failed for {}".format(record["source"]))
        verified.append({
            "source": record["source"], "checkpoint": record["checkpoint"],
            "sha256": source_hash, "bytes": os.path.getsize(record["source"]),
        })
    return {"manifest": CHECKPOINT_MANIFEST, "verified": True, "records": verified}


def _source_mesh(component):
    try:
        return _path(component.get_skeletal_mesh_asset())
    except Exception:
        return _path(_value(component, "skeletal_mesh"))


def _material_paths(component):
    return [_path(component.get_material(index)) for index in range(component.get_num_materials())]


def _actor_snapshot(actor, component=None):
    snapshot = {
        "object_path": actor.get_path_name(),
        "name": actor.get_name(),
        "label": actor.get_actor_label(),
        "class": _path(actor.get_class()),
        "location": _vec(actor.get_actor_location()),
        "rotation": _rot(actor.get_actor_rotation()),
        "scale": _vec(actor.get_actor_scale3d()),
        "folder": str(actor.get_folder_path()),
        "tags": sorted(str(tag) for tag in list(_value(actor, "tags", []))),
        "attach_parent": _path(actor.get_attach_parent_actor()),
        "attached_actor_paths": sorted(_path(value) for value in list(actor.get_attached_actors())),
        "actor_collision_enabled": bool(actor.get_actor_enable_collision()),
        "hidden_in_game": bool(_value(actor, "hidden", False)),
    }
    if component:
        snapshot["component"] = {
            "name": component.get_name(),
            "mobility": str(_value(component, "mobility", "")),
            "materials": _material_paths(component),
            "cast_shadow": bool(_value(component, "cast_shadow", False)),
            "cast_dynamic_shadow": bool(_value(component, "cast_dynamic_shadow", False)),
            "cast_static_shadow": bool(_value(component, "cast_static_shadow", False)),
            "cast_contact_shadow": bool(_value(component, "cast_contact_shadow", False)),
            "visible": bool(_value(component, "visible", True)),
            "hidden_in_game": bool(_value(component, "hidden_in_game", False)),
            "render_in_main_pass": bool(_value(component, "render_in_main_pass", True)),
            "render_in_depth_pass": bool(_value(component, "render_in_depth_pass", True)),
            "use_as_occluder": bool(_value(component, "use_as_occluder", True)),
            "receives_decals": bool(_value(component, "receives_decals", True)),
            "generate_overlap_events": bool(_value(component, "generate_overlap_events", False)),
            "collision_enabled": str(component.get_collision_enabled()),
            "collision_profile": str(component.get_collision_profile_name()),
            "simulate_physics": bool(component.is_simulating_physics()),
        }
    return snapshot


def _unrelated_fingerprint(actors, excluded_paths):
    # Do not ask every arbitrary map actor to resolve its full world transform or
    # component state. The production map contains legacy BSP/model actors with
    # invalid vertex data; UE 5.7 can divide by zero while resolving those
    # transforms from Python. Identity/organization are sufficient here because
    # replacement code only calls mutators on the selected source/replacement
    # actors, while every selected vegetation transform is checked separately.
    rows = []
    for actor in actors:
        if actor.get_path_name() in excluded_paths:
            continue
        rows.append({
            "object_path": actor.get_path_name(),
            "name": actor.get_name(),
            "label": actor.get_actor_label(),
            "class": _path(actor.get_class()),
            "folder": str(actor.get_folder_path()),
            "tags": sorted(str(tag) for tag in list(_value(actor, "tags", []))),
        })
    encoded = json.dumps(sorted(rows, key=lambda row: row["object_path"]), sort_keys=True).encode("utf-8")
    return {"actor_count": len(rows), "sha256": hashlib.sha256(encoded).hexdigest()}


def _load_map_and_actors():
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_ASSET):
        raise RuntimeError("Could not load production map {}".format(MAP_ASSET))
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not subsystem:
        raise RuntimeError("EditorActorSubsystem unavailable")
    return subsystem, list(subsystem.get_all_level_actors())


def smoke():
    """Exercise every replacement-facing UE API without creating or saving actors."""
    checks = []

    def record(name, callback):
        try:
            value = callback()
            checks.append({"name": name, "passed": True, "value": str(value)[:500]})
            return value
        except Exception as exc:
            checks.append({"name": name, "passed": False, "error": repr(exc)})
            return None

    inventory, _, mapping, _ = _mapping_and_validation()
    target_package = inventory["source_to_target_mapping"][0]["target_static_mesh"]
    target_asset = record("EditorAssetLibrary.load_asset", lambda: unreal.EditorAssetLibrary.load_asset(target_package))
    record("target is StaticMesh", lambda: isinstance(target_asset, unreal.StaticMesh) or
           (_ for _ in ()).throw(RuntimeError("target asset class mismatch")))
    record("target full object path normalization", lambda: _object_path(target_asset))

    benchmark_map = "/Game/VHV_StaticVegetation/Benchmark/Lvl_VHV_Vegetation_Benchmark_B_Static"
    record("load isolated static benchmark map", lambda: unreal.EditorLoadingAndSavingUtils.load_map(benchmark_map))
    actor_subsystem = record("get EditorActorSubsystem", lambda: unreal.get_editor_subsystem(unreal.EditorActorSubsystem))
    benchmark_actors = record("get_all_level_actors", lambda: list(actor_subsystem.get_all_level_actors())) or []
    static_components = []
    for actor in benchmark_actors:
        static_components.extend(list(actor.get_components_by_class(unreal.StaticMeshComponent)))
    static_component = static_components[0] if static_components else None
    record("static benchmark component exists", lambda: static_component or
           (_ for _ in ()).throw(RuntimeError("no StaticMeshComponent in isolated static benchmark")))
    record("StaticMeshComponent.static_mesh property", lambda: _object_path(
        static_component.get_editor_property("static_mesh")))
    for method in ("set_static_mesh", "set_mobility", "set_collision_profile_name",
                   "set_collision_enabled", "set_simulate_physics",
                   "set_evaluate_world_position_offset",
                   "set_evaluate_world_position_offset_in_ray_tracing"):
        record("StaticMeshComponent.{} exposed".format(method), lambda method=method: callable(
            getattr(static_component, method)))
    for method in ("get_collision_enabled", "get_collision_profile_name", "is_simulating_physics",
                   "get_num_materials", "get_material"):
        record("StaticMeshComponent.{} call".format(method), lambda method=method: (
            getattr(static_component, method)(0) if method == "get_material" else getattr(static_component, method)()))

    subsystem, actors = _load_map_and_actors()
    source_actor = None
    source_component = None
    for actor in actors:
        components = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
        if len(components) == 1 and _source_mesh(components[0]) in mapping:
            source_actor, source_component = actor, components[0]
            break
    record("production source actor exists", lambda: source_actor or
           (_ for _ in ()).throw(RuntimeError("no mapped production source actor")))
    for method in ("get_path_name", "get_name", "get_actor_label", "get_class", "get_actor_location",
                   "get_actor_rotation", "get_actor_scale3d", "get_folder_path", "get_attach_parent_actor",
                   "get_attached_actors", "get_actor_enable_collision", "get_components_by_class",
                   "set_actor_scale3d", "set_actor_label", "set_folder_path", "set_actor_enable_collision",
                   "set_actor_hidden_in_game", "set_actor_location", "set_actor_rotation", "set_editor_property"):
        record("Actor.{} exposed".format(method), lambda method=method: callable(getattr(source_actor, method)))
    record("source actor snapshot getters", lambda: _actor_snapshot(source_actor, source_component))
    for method in ("spawn_actor_from_class", "destroy_actor", "get_all_level_actors"):
        record("EditorActorSubsystem.{} exposed".format(method), lambda method=method: callable(
            getattr(subsystem, method)))
    level_subsystem = record("get LevelEditorSubsystem", lambda: unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem))
    record("LevelEditorSubsystem.save_current_level exposed", lambda: callable(
        getattr(level_subsystem, "save_current_level")))
    record("CollisionEnabled.NO_COLLISION exposed", lambda: unreal.CollisionEnabled.NO_COLLISION)

    result = {
        "schema": 1, "mode": "read_only_api_smoke", "engine": "5.7.4",
        "map": MAP_ASSET, "checks": checks,
        "passed": bool(checks) and all(check["passed"] for check in checks),
        "production_map_saved": False, "actors_created_or_destroyed": 0,
    }
    _write(SMOKE_PATH, result)
    if not result["passed"]:
        raise RuntimeError("UE Python API smoke test failed: {}".format(
            [check for check in checks if not check["passed"]]))
    unreal.log("{} read-only API smoke passed ({} checks)".format(LOG, len(checks)))
    return result


def diagnose_spawn():
    """Isolate the editor actor-spawn path without touching the production map."""
    benchmark_map = "/Game/VHV_StaticVegetation/Benchmark/Lvl_VHV_Vegetation_Benchmark_B_Static"
    if not unreal.EditorLoadingAndSavingUtils.load_map(benchmark_map):
        raise RuntimeError("Could not load isolated benchmark map")
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    target_package = _load(INVENTORY_PATH)["source_to_target_mapping"][0]["target_static_mesh"]
    target_asset = unreal.EditorAssetLibrary.load_asset(target_package)
    if not isinstance(target_asset, unreal.StaticMesh):
        raise RuntimeError("Diagnostic target is not a StaticMesh")
    unreal.log("{} DIAGNOSE_SPAWN before spawn".format(LOG))
    actor = subsystem.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 5000.0),
                                             unreal.Rotator(0.0, 0.0, 0.0))
    if not actor:
        raise RuntimeError("Diagnostic spawn returned no actor")
    unreal.log("{} DIAGNOSE_SPAWN after spawn".format(LOG))
    component = _component(actor, unreal.StaticMeshComponent)
    component.set_static_mesh(target_asset)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_simulate_physics(False)
    component.set_evaluate_world_position_offset(False)
    component.set_evaluate_world_position_offset_in_ray_tracing(False)
    if _object_path(_value(component, "static_mesh")) != _object_path(target_package):
        raise RuntimeError("Diagnostic static-mesh assignment mismatch")
    unreal.log("{} DIAGNOSE_SPAWN after assign/verify".format(LOG))
    if not subsystem.destroy_actor(actor):
        raise RuntimeError("Diagnostic actor destruction failed")
    unreal.log("{} DIAGNOSE_SPAWN PASS; benchmark map not saved".format(LOG))
    return {"passed": True, "production_map_loaded": False, "map_saved": False}


def diagnose_scan():
    """Identify any mapped vegetation actor whose native transform accessor faults."""
    inventory, _, mapping, _ = _mapping_and_validation()
    _, actors = _load_map_and_actors()
    mapped = 0
    for index, actor in enumerate(actors):
        components = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
        if len(components) != 1:
            continue
        source = _source_mesh(components[0])
        if source not in mapping:
            continue
        mapped += 1
        unreal.log("{} DIAGNOSE_SCAN before {}/? {}".format(LOG, mapped, actor.get_path_name()))
        _vec(actor.get_actor_location())
        _rot(actor.get_actor_rotation())
        _vec(actor.get_actor_scale3d())
        unreal.log("{} DIAGNOSE_SCAN after {} {}".format(LOG, mapped, actor.get_path_name()))
    if mapped != inventory["eligible_megaplant_actor_count"]:
        raise RuntimeError("Diagnostic scan mapped {} instead of {}".format(
            mapped, inventory["eligible_megaplant_actor_count"]))
    unreal.log("{} DIAGNOSE_SCAN PASS: {} mapped transforms; map not saved".format(LOG, mapped))
    return {"passed": True, "mapped": mapped, "map_saved": False}


def _mapping_and_validation():
    inventory = _load(INVENTORY_PATH)
    validation = _load(VALIDATION_PATH)
    mapping = {item["source_skeletal_mesh"]: item for item in inventory["source_to_target_mapping"]}
    validation_by_source = {row["mapping"]["source_skeletal_mesh"]: row for row in validation["variants"]}
    return inventory, validation, mapping, validation_by_source


def _candidate_rows(actors, inventory, mapping, validation_by_source):
    protected = {row["object_path"]: row for row in inventory["actors"]}
    candidates = []
    blocked = []
    for actor in actors:
        components = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
        if len(components) != 1:
            continue
        component = components[0]
        source = _source_mesh(component)
        if source not in mapping:
            continue
        row = protected.get(actor.get_path_name())
        if not row:
            raise RuntimeError("Mapped vegetation actor absent from protected inventory: {}".format(actor.get_path_name()))
        variant = validation_by_source[source]
        entry = {
            "actor": actor, "component": component, "inventory": row,
            "source": source, "target": mapping[source]["target_static_mesh"],
            "species": mapping[source]["species"], "variant": mapping[source]["variant"],
        }
        if variant["automated_checks_passed"]:
            candidates.append(entry)
        else:
            blocked.append(entry)
    return candidates, blocked


def _distance_sq(entry, anchor):
    location = entry["actor"].get_actor_location()
    return (float(location.x) - anchor[0]) ** 2 + (float(location.y) - anchor[1]) ** 2


def _select(candidates):
    selected = []
    for species in SPECIES:
        pool = [entry for entry in candidates if entry["species"] == species]
        safe = [entry for entry in pool if not entry["actor"].get_attach_parent_actor()
                and not list(entry["actor"].get_attached_actors())]
        if len(safe) < 4:
            raise RuntimeError("Fewer than four unattached cleared actors for {}".format(species))
        chosen = []
        for region in ("entrance", "market"):
            available = [entry for entry in safe if entry not in chosen]
            chosen.extend(sorted(available, key=lambda entry: (_distance_sq(entry, ANCHORS[region]),
                                                                entry["actor"].get_path_name()))[:2])
        if len(chosen) != 4:
            raise RuntimeError("Selection failed for {}".format(species))
        selected.extend(chosen)
    if len(selected) != 24 or len({entry["actor"].get_path_name() for entry in selected}) != 24:
        raise RuntimeError("Pilot selection is not exactly 24 unique actors")
    return selected


def plan():
    checkpoint = _checkpoint_gate()
    inventory, validation, mapping, validation_by_source = _mapping_and_validation()
    if _sha256(MAP_FILE) != inventory["map_file"]["sha256"]:
        raise RuntimeError("Production map differs from protected inventory")
    if validation["cleared_actor_count"] != 290 or validation["blocked_actor_count"] != 15:
        raise RuntimeError("Expected the reviewed 290-cleared / 15-blocked validation state")
    subsystem, actors = _load_map_and_actors()
    candidates, blocked = _candidate_rows(actors, inventory, mapping, validation_by_source)
    if len(candidates) != 290 or len(blocked) != 15:
        raise RuntimeError("Live map eligibility changed: cleared={} blocked={}".format(len(candidates), len(blocked)))
    selected = _select(candidates)
    selected_rows = []
    for entry in selected:
        actor = entry["actor"]
        snapshot = _actor_snapshot(actor, entry["component"])
        if not (_close(snapshot["location"], entry["inventory"]["location"])
                and _close(snapshot["rotation"], entry["inventory"]["rotation"])
                and _close(snapshot["scale"], entry["inventory"]["scale"])):
            raise RuntimeError("Selected transform differs from inventory: {}".format(actor.get_path_name()))
        selected_rows.append({
            "species": entry["species"], "variant": entry["variant"],
            "source_skeletal_mesh": entry["source"], "target_static_mesh": entry["target"],
            "selection_region": min(ANCHORS, key=lambda name: _distance_sq(entry, ANCHORS[name])),
            "before": snapshot,
        })
    landmarks = []
    for actor in actors:
        text = "{} {}".format(actor.get_actor_label(), actor.get_class().get_name()).lower()
        if any(token in text for token in ("playerstart", "entrance", "market")):
            landmarks.append({"label": actor.get_actor_label(), "class": actor.get_class().get_name(),
                              "location": _vec(actor.get_actor_location())})
    result = {
        "schema": 1, "mode": "read_only_plan", "map": MAP_ASSET,
        "map_sha256": _sha256(MAP_FILE), "checkpoint": checkpoint,
        "selection_anchors": ANCHORS, "landmarks": landmarks,
        "selected_actor_count": len(selected_rows), "selected": selected_rows,
        "selected_by_species": {species: sum(row["species"] == species for row in selected_rows)
                                for species in SPECIES},
        "cleared_live_actor_count": len(candidates), "blocked_live_actor_count": len(blocked),
        "blocked_actor_paths": sorted(entry["actor"].get_path_name() for entry in blocked),
        "attachment_gate_passed": all(not row["before"]["attach_parent"]
                                      and not row["before"]["attached_actor_paths"] for row in selected_rows),
        "reference_scan": "OBJ REFS direct/external/minimal emitted to Unreal log between per-actor markers",
        "modified": False,
    }
    _write(PLAN_PATH, result)
    for row in selected_rows:
        path = row["before"]["object_path"]
        unreal.log("{} REFERENCE_SCAN_BEGIN {}".format(LOG, path))
        unreal.SystemLibrary.execute_console_command(actors[0], "OBJ REFS NAME={} DIRECT EXTERNAL MINIMAL".format(path))
        unreal.log("{} REFERENCE_SCAN_END {}".format(LOG, path))
    unreal.log("{} planned exactly 24 actors (4 per species); no map save".format(LOG))
    return result


def _copy_optional_properties(source, target):
    for name in (
        "cast_hidden_shadow", "cast_cinematic_shadow", "cast_far_shadow",
        "affect_distance_field_lighting", "affect_dynamic_indirect_lighting",
        "affect_indirect_lighting_while_hidden", "visible_in_ray_tracing",
        "render_custom_depth", "custom_depth_stencil_value", "custom_depth_stencil_write_mask",
        "owner_no_see", "only_owner_see", "bounds_scale", "min_draw_distance",
        "ld_max_draw_distance", "ray_tracing_group_id",
    ):
        value = _value(source, name, None)
        if value is not None:
            try:
                target.set_editor_property(name, value)
            except Exception:
                pass


def _verify_new_actor(actor, row, target_asset):
    component = _component(actor, unreal.StaticMeshComponent)
    failures = []
    if _object_path(_value(component, "static_mesh")) != _object_path(row["target_static_mesh"]):
        failures.append("static mesh mismatch")
    for key, actual in (("location", _vec(actor.get_actor_location())),
                        ("rotation", _rot(actor.get_actor_rotation())),
                        ("scale", _vec(actor.get_actor_scale3d()))):
        if not _close(actual, row["before"][key]):
            failures.append("{} mismatch".format(key))
    if actor.get_actor_label() != row["before"]["label"]:
        failures.append("label mismatch")
    if str(actor.get_folder_path()) != row["before"]["folder"]:
        failures.append("folder mismatch")
    if sorted(str(tag) for tag in list(_value(actor, "tags", []))) != row["before"]["tags"]:
        failures.append("tag mismatch")
    if bool(_value(actor, "hidden", False)) != row["before"]["hidden_in_game"]:
        failures.append("actor hidden state mismatch")
    if actor.get_actor_enable_collision():
        failures.append("actor collision enabled")
    if component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
        failures.append("collision is not NoCollision")
    if component.is_simulating_physics():
        failures.append("physics simulation enabled")
    if bool(_value(component, "evaluate_world_position_offset", True)):
        failures.append("WPO enabled")
    if bool(_value(component, "evaluate_world_position_offset_in_ray_tracing", True)):
        failures.append("ray-tracing WPO enabled")
    expected = row["before"]["component"]
    if str(_value(component, "mobility", "")) != expected["mobility"]:
        failures.append("mobility mismatch")
    for name in ("cast_shadow", "cast_dynamic_shadow", "cast_static_shadow", "cast_contact_shadow",
                 "visible", "hidden_in_game", "render_in_main_pass", "render_in_depth_pass",
                 "use_as_occluder", "receives_decals"):
        if bool(_value(component, name, not expected[name])) != expected[name]:
            failures.append("{} mismatch".format(name))
    if _material_paths(component) != _material_paths(_component_from_asset(target_asset)):
        failures.append("target default material mismatch")
    return component, failures


def _component_from_asset(asset):
    # StaticMesh material validation without creating another component.
    class AssetMaterialProxy:
        def get_num_materials(self_inner):
            return len(list(asset.get_editor_property("static_materials")))

        def get_material(self_inner, index):
            return _value(list(asset.get_editor_property("static_materials"))[index], "material_interface")

    return AssetMaterialProxy()


def apply():
    checkpoint = _checkpoint_gate()
    plan_data = _load(PLAN_PATH)
    go = _load(GO_PATH)
    if go.get("approved") is not True or go.get("plan_sha256") != _sha256(PLAN_PATH):
        raise RuntimeError("Pilot GO file is absent, unapproved, or does not bind the current plan")
    if plan_data.get("selected_actor_count") != 24 or any(
            plan_data["selected_by_species"].get(species) != 4 for species in SPECIES):
        raise RuntimeError("Pilot plan is not exactly four actors per species")
    before_hash = _sha256(MAP_FILE)
    if before_hash != plan_data["map_sha256"] or before_hash != checkpoint["records"][0]["sha256"]:
        raise RuntimeError("Production map changed after pilot planning")
    inventory, validation, mapping, validation_by_source = _mapping_and_validation()
    subsystem, actors = _load_map_and_actors()
    unreal.log("{} apply preflight: map loaded; {} actors".format(LOG, len(actors)))
    actor_by_path = {actor.get_path_name(): actor for actor in actors}
    selected_paths = {row["before"]["object_path"] for row in plan_data["selected"]}
    unrelated_before = _unrelated_fingerprint(actors, selected_paths)
    unreal.log("{} apply preflight: unrelated identity fingerprint complete".format(LOG))
    all_vegetation_before = {}
    for actor in actors:
        components = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
        if len(components) == 1 and _source_mesh(components[0]) in mapping:
            all_vegetation_before[actor.get_path_name()] = {
                "source": _source_mesh(components[0]), "location": _vec(actor.get_actor_location()),
                "rotation": _rot(actor.get_actor_rotation()), "scale": _vec(actor.get_actor_scale3d()),
            }
    if len(all_vegetation_before) != 305:
        raise RuntimeError("Expected 305 original vegetation actors before pilot")
    created = []
    try:
        for row in plan_data["selected"]:
            source_actor = actor_by_path.get(row["before"]["object_path"])
            if not source_actor:
                raise RuntimeError("Selected source actor missing: {}".format(row["before"]["object_path"]))
            source_component = _component(source_actor, unreal.SkeletalMeshComponent)
            if _source_mesh(source_component) != row["source_skeletal_mesh"]:
                raise RuntimeError("Selected source mesh changed: {}".format(source_actor.get_path_name()))
            current = _actor_snapshot(source_actor, source_component)
            for key in ("location", "rotation", "scale"):
                if not _close(current[key], row["before"][key]):
                    raise RuntimeError("Selected source {} changed: {}".format(key, source_actor.get_path_name()))
            if source_actor.get_attach_parent_actor() or list(source_actor.get_attached_actors()):
                raise RuntimeError("Selected actor gained an attachment: {}".format(source_actor.get_path_name()))
            target_asset = unreal.EditorAssetLibrary.load_asset(row["target_static_mesh"])
            if not isinstance(target_asset, unreal.StaticMesh):
                raise RuntimeError("Validated target is not a StaticMesh: {}".format(row["target_static_mesh"]))
            variant = validation_by_source[row["source_skeletal_mesh"]]
            if not variant["automated_checks_passed"]:
                raise RuntimeError("Selected target variant is no longer cleared")
            location = source_actor.get_actor_location()
            rotation = source_actor.get_actor_rotation()
            replacement = subsystem.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
            if not replacement:
                raise RuntimeError("Could not spawn replacement for {}".format(source_actor.get_path_name()))
            created.append({"actor": replacement, "source": source_actor, "row": row, "asset": target_asset})
            replacement.set_actor_scale3d(source_actor.get_actor_scale3d())
            replacement.set_actor_label(row["before"]["label"])
            replacement.set_folder_path(row["before"]["folder"])
            replacement.set_editor_property("tags", list(row["before"]["tags"]))
            replacement.set_actor_enable_collision(False)
            replacement.set_actor_hidden_in_game(row["before"]["hidden_in_game"])
            target_component = _component(replacement, unreal.StaticMeshComponent)
            target_component.set_static_mesh(target_asset)
            pipeline._copy_component_settings(source_component, target_component)
            _copy_optional_properties(source_component, target_component)
            target_component.set_collision_profile_name("NoCollision")
            target_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            target_component.set_simulate_physics(False)
            target_component.set_evaluate_world_position_offset(False)
            target_component.set_evaluate_world_position_offset_in_ray_tracing(False)
            replacement.set_actor_location(location, False, False)
            replacement.set_actor_rotation(rotation, False)
            replacement.set_actor_scale3d(source_actor.get_actor_scale3d())
        initial_failures = []
        for item in created:
            _, failures = _verify_new_actor(item["actor"], item["row"], item["asset"])
            if failures:
                initial_failures.append({"source": item["source"].get_path_name(), "failures": failures})
        if initial_failures:
            raise RuntimeError("Replacement verification failed before source removal: {}".format(initial_failures))
        for item in created:
            if not subsystem.destroy_actor(item["source"]):
                raise RuntimeError("Could not remove verified source actor {}".format(item["row"]["before"]["object_path"]))
        final_actors = list(subsystem.get_all_level_actors())
        final_paths = {actor.get_path_name() for actor in final_actors}
        if selected_paths & final_paths:
            raise RuntimeError("One or more selected skeletal source actors remain")
        unrelated_after = _unrelated_fingerprint(
            [actor for actor in final_actors if actor not in [item["actor"] for item in created]], set())
        if unrelated_before != unrelated_after:
            raise RuntimeError("Unrelated actor fingerprint changed")
        remaining = {}
        for actor in final_actors:
            components = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
            if len(components) == 1 and _source_mesh(components[0]) in mapping:
                remaining[actor.get_path_name()] = {
                    "source": _source_mesh(components[0]), "location": _vec(actor.get_actor_location()),
                    "rotation": _rot(actor.get_actor_rotation()), "scale": _vec(actor.get_actor_scale3d()),
                }
        expected_remaining = {path: value for path, value in all_vegetation_before.items() if path not in selected_paths}
        if remaining != expected_remaining or len(remaining) != 281:
            raise RuntimeError("The other 281 vegetation actors did not remain byte-equivalent by identity/transform/source")
        blocked_paths = set(plan_data["blocked_actor_paths"])
        if not blocked_paths.issubset(set(remaining)) or len(blocked_paths) != 15:
            raise RuntimeError("Blocked vegetation actors were changed or removed")
        final_rows = []
        for item in created:
            component, failures = _verify_new_actor(item["actor"], item["row"], item["asset"])
            if failures:
                raise RuntimeError("Final replacement verification failed: {}".format(failures))
            final_rows.append({
                "source_actor": item["row"]["before"]["object_path"],
                "replacement_actor": item["actor"].get_path_name(),
                "species": item["row"]["species"], "variant": item["row"]["variant"],
                "source_skeletal_mesh": item["row"]["source_skeletal_mesh"],
                "target_static_mesh": item["row"]["target_static_mesh"],
                "before": item["row"]["before"],
                "after": _actor_snapshot(item["actor"], component),
                "verified": True,
            })
        if len(final_rows) != 24 or len({row["replacement_actor"] for row in final_rows}) != 24:
            raise RuntimeError("Final pilot does not contain exactly 24 unique replacements")
        audit = {
            "schema": 1, "mode": "production_pilot_apply", "map": MAP_ASSET,
            "checkpoint": checkpoint, "plan_sha256": _sha256(PLAN_PATH),
            "map_sha256_before": before_hash, "map_sha256_after": None,
            "selected_count": 24, "replaced_count": 24,
            "remaining_original_vegetation_count": len(remaining),
            "remaining_blocked_actor_count": len(blocked_paths),
            "remaining_cleared_not_in_pilot_count": len(remaining) - len(blocked_paths),
            "unrelated_actor_fingerprint_before": unrelated_before,
            "unrelated_actor_fingerprint_after": unrelated_after,
            "all_replacements_verified": True,
            "collision_policy": "NoCollision; simulate_physics=False; WPO disabled",
            "production_map_saved": False, "replacements": final_rows,
        }
        _write(AUDIT_PATH, audit)
        if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
            raise RuntimeError("Pilot passed in-memory verification but production map save failed")
        audit["map_sha256_after"] = _sha256(MAP_FILE)
        audit["production_map_saved"] = True
        _write(AUDIT_PATH, audit)
        unreal.log("{} SUCCESS: 24 replaced, 281 originals retained, production map saved once".format(LOG))
        return audit
    except Exception:
        # No save occurs before every validation above passes. Destructive
        # in-memory work is discarded when this commandlet exits on failure.
        failure = {
            "schema": 1, "mode": "production_pilot_apply", "production_map_saved": False,
            "map_sha256_on_disk": _sha256(MAP_FILE), "error": traceback.format_exc(),
            "recovery": "If the on-disk hash differs from the checkpoint, restore its verified map package after Unreal exits.",
        }
        _write(AUDIT_PATH, failure)
        raise


def verify_pilot():
    audit = _load(AUDIT_PATH)
    if not audit.get("production_map_saved") or audit.get("replaced_count") != 24:
        raise RuntimeError("Successful saved pilot audit is required")
    if _sha256(MAP_FILE) != audit["map_sha256_after"]:
        raise RuntimeError("On-disk map differs from the saved pilot audit")
    inventory, _, mapping, validation_by_source = _mapping_and_validation()
    subsystem, actors = _load_map_and_actors()
    actor_by_path = {actor.get_path_name(): actor for actor in actors}
    replacement_paths = {row["replacement_actor"] for row in audit["replacements"]}
    source_paths = {row["source_actor"] for row in audit["replacements"]}
    if source_paths & set(actor_by_path):
        raise RuntimeError("One or more pilot skeletal source actors persisted after reload")
    verified = []
    for row in audit["replacements"]:
        actor = actor_by_path.get(row["replacement_actor"])
        if not actor:
            raise RuntimeError("Pilot replacement missing after reload: {}".format(row["replacement_actor"]))
        target = unreal.EditorAssetLibrary.load_asset(row["target_static_mesh"])
        component, failures = _verify_new_actor(actor, row, target)
        if failures:
            raise RuntimeError("Reloaded pilot replacement failed: {} {}".format(
                row["replacement_actor"], failures))
        verified.append(_actor_snapshot(actor, component))
    unrelated = _unrelated_fingerprint(actors, replacement_paths)
    if unrelated != audit["unrelated_actor_fingerprint_before"]:
        raise RuntimeError("Unrelated actor identity/organization fingerprint changed after reload")
    protected = {row["object_path"]: row for row in inventory["actors"]}
    expected_remaining = {}
    for path, row in protected.items():
        if path not in source_paths:
            expected_remaining[path] = {
                "source": row["eligible_components"][0]["skeletal_mesh"],
                "location": row["location"], "rotation": row["rotation"], "scale": row["scale"],
            }
    actual_remaining = {}
    for actor in actors:
        components = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
        if len(components) == 1 and _source_mesh(components[0]) in mapping:
            actual_remaining[actor.get_path_name()] = {
                "source": _source_mesh(components[0]), "location": _vec(actor.get_actor_location()),
                "rotation": _rot(actor.get_actor_rotation()), "scale": _vec(actor.get_actor_scale3d()),
            }
    if actual_remaining != expected_remaining or len(actual_remaining) != 281:
        raise RuntimeError("Reloaded pilot did not preserve exactly the other 281 original vegetation actors")
    blocked = [path for path, row in actual_remaining.items()
               if not validation_by_source[row["source"]]["automated_checks_passed"]]
    if len(blocked) != 15:
        raise RuntimeError("Reloaded pilot blocked-actor count is not 15")
    result = {
        "schema": 1, "mode": "pilot_reload_audit", "passed": True,
        "map_sha256": _sha256(MAP_FILE), "replacement_count": len(verified),
        "remaining_original_vegetation_count": len(actual_remaining),
        "remaining_blocked_count": len(blocked), "source_paths_absent": True,
        "unrelated_fingerprint_matches": True, "map_saved": False,
    }
    _write(RELOAD_AUDIT_PATH, result)
    unreal.log("{} RELOAD PASS: 24 static replacements, 281 originals, 15 blocked".format(LOG))
    return result


def plan_full():
    pilot = _load(AUDIT_PATH)
    reload_audit = _load(RELOAD_AUDIT_PATH)
    if not pilot.get("production_map_saved") or not reload_audit.get("passed"):
        raise RuntimeError("Saved-and-reloaded pilot must pass before full planning")
    if _sha256(MAP_FILE) != reload_audit["map_sha256"]:
        raise RuntimeError("Production map changed after the pilot reload audit")
    inventory, validation, mapping, validation_by_source = _mapping_and_validation()
    _, actors = _load_map_and_actors()
    candidates, blocked = _candidate_rows(actors, inventory, mapping, validation_by_source)
    if len(candidates) != 266 or len(blocked) != 15:
        raise RuntimeError("Post-pilot plan expected 266 cleared and 15 blocked; found {}/{}".format(
            len(candidates), len(blocked)))
    remaining = candidates + blocked
    rows = []
    for entry in remaining:
        snapshot = _actor_snapshot(entry["actor"], entry["component"])
        protected = entry["inventory"]
        if not (_close(snapshot["location"], protected["location"])
                and _close(snapshot["rotation"], protected["rotation"])
                and _close(snapshot["scale"], protected["scale"])):
            raise RuntimeError("Post-pilot source transform differs from inventory: {}".format(
                snapshot["object_path"]))
        rows.append({
            "action": "replace" if entry in candidates else "delete_rejected",
            "species": entry["species"], "variant": entry["variant"],
            "source_skeletal_mesh": entry["source"], "target_static_mesh": entry["target"],
            "before": snapshot,
        })
    rows.sort(key=lambda row: row["before"]["object_path"])
    pilot_paths = [row["replacement_actor"] for row in pilot["replacements"]]
    result = {
        "schema": 1, "mode": "read_only_full_rollout_plan", "map": MAP_ASSET,
        "map_sha256": _sha256(MAP_FILE), "pilot_audit_sha256": _sha256(AUDIT_PATH),
        "pilot_reload_audit_sha256": _sha256(RELOAD_AUDIT_PATH),
        "pilot_replacement_paths": sorted(pilot_paths),
        "remaining_actor_count": len(rows), "replace_count": len(candidates),
        "delete_rejected_count": len(blocked),
        "attachment_free": all(not row["before"]["attach_parent"] and
                               not row["before"]["attached_actor_paths"] for row in rows),
        "rows": rows, "modified": False,
        "reference_scan": "OBJ REFS direct/external/minimal emitted for all remaining 281 actors",
    }
    _write(FULL_PLAN_PATH, result)
    for index, row in enumerate(rows, 1):
        path = row["before"]["object_path"]
        unreal.log("{} FULL_REF_BEGIN {}/{} {}".format(LOG, index, len(rows), path))
        unreal.SystemLibrary.execute_console_command(
            actors[0], "OBJ REFS NAME={} DIRECT EXTERNAL MINIMAL".format(path))
        unreal.log("{} FULL_REF_END {}/{} {}".format(LOG, index, len(rows), path))
    unreal.log("{} FULL PLAN PASS: replace=266 delete=15; no map save".format(LOG))
    return result


def _spawn_verified_replacement(subsystem, source_actor, source_component, row, validation_by_source):
    if _source_mesh(source_component) != row["source_skeletal_mesh"]:
        raise RuntimeError("Source mesh changed: {}".format(source_actor.get_path_name()))
    current = _actor_snapshot(source_actor, source_component)
    for key in ("location", "rotation", "scale"):
        if not _close(current[key], row["before"][key]):
            raise RuntimeError("Source {} changed: {}".format(key, source_actor.get_path_name()))
    if source_actor.get_attach_parent_actor() or list(source_actor.get_attached_actors()):
        raise RuntimeError("Source actor has an attachment: {}".format(source_actor.get_path_name()))
    variant = validation_by_source[row["source_skeletal_mesh"]]
    if not variant["automated_checks_passed"]:
        raise RuntimeError("Target variant is not validated: {}".format(row["source_skeletal_mesh"]))
    target_asset = unreal.EditorAssetLibrary.load_asset(row["target_static_mesh"])
    if not isinstance(target_asset, unreal.StaticMesh):
        raise RuntimeError("Target is not a StaticMesh: {}".format(row["target_static_mesh"]))
    location = source_actor.get_actor_location()
    rotation = source_actor.get_actor_rotation()
    replacement = subsystem.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
    if not replacement:
        raise RuntimeError("Could not spawn replacement for {}".format(source_actor.get_path_name()))
    replacement.set_actor_scale3d(source_actor.get_actor_scale3d())
    replacement.set_actor_label(row["before"]["label"])
    replacement.set_folder_path(row["before"]["folder"])
    replacement.set_editor_property("tags", list(row["before"]["tags"]))
    replacement.set_actor_enable_collision(False)
    replacement.set_actor_hidden_in_game(row["before"]["hidden_in_game"])
    target_component = _component(replacement, unreal.StaticMeshComponent)
    target_component.set_static_mesh(target_asset)
    pipeline._copy_component_settings(source_component, target_component)
    _copy_optional_properties(source_component, target_component)
    target_component.set_collision_profile_name("NoCollision")
    target_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    target_component.set_simulate_physics(False)
    target_component.set_evaluate_world_position_offset(False)
    target_component.set_evaluate_world_position_offset_in_ray_tracing(False)
    replacement.set_actor_location(location, False, False)
    replacement.set_actor_rotation(rotation, False)
    replacement.set_actor_scale3d(source_actor.get_actor_scale3d())
    _, failures = _verify_new_actor(replacement, row, target_asset)
    if failures:
        raise RuntimeError("New actor verification failed for {}: {}".format(
            source_actor.get_path_name(), failures))
    return {"actor": replacement, "source": source_actor, "row": row, "asset": target_asset}


def apply_full():
    plan_data = _load(FULL_PLAN_PATH)
    go = _load(FULL_GO_PATH)
    pilot = _load(AUDIT_PATH)
    pilot_reload = _load(RELOAD_AUDIT_PATH)
    if go.get("approved") is not True or go.get("plan_sha256") != _sha256(FULL_PLAN_PATH):
        raise RuntimeError("Full-rollout GO is absent, disabled, or not bound to the plan")
    if not pilot_reload.get("passed") or plan_data.get("replace_count") != 266 \
            or plan_data.get("delete_rejected_count") != 15 or not plan_data.get("attachment_free"):
        raise RuntimeError("Full-rollout plan/pilot gates are not satisfied")
    before_hash = _sha256(MAP_FILE)
    if before_hash != plan_data["map_sha256"] or before_hash != go["map_sha256_before"]:
        raise RuntimeError("Production map changed after full planning")
    delete_rows = [row for row in plan_data["rows"] if row["action"] == "delete_rejected"]
    replace_rows = [row for row in plan_data["rows"] if row["action"] == "replace"]
    delete_paths = {row["before"]["object_path"] for row in delete_rows}
    if delete_paths != set(go.get("delete_allowed_paths", [])):
        raise RuntimeError("Authorized rejected-actor deletion set differs from the plan")
    if sorted((row["species"], row["variant"]) for row in delete_rows).count(("Beach_Rose", "C")) != 11 \
            or sorted((row["species"], row["variant"]) for row in delete_rows).count(("Common_Hazel", "B")) != 4:
        raise RuntimeError("Rejected deletion variants/counts are incorrect")
    inventory, _, mapping, validation_by_source = _mapping_and_validation()
    subsystem, actors = _load_map_and_actors()
    actor_by_path = {actor.get_path_name(): actor for actor in actors}
    pilot_paths = {row["replacement_actor"] for row in pilot["replacements"]}
    source_paths = {row["before"]["object_path"] for row in plan_data["rows"]}
    if not source_paths.issubset(set(actor_by_path)) or not pilot_paths.issubset(set(actor_by_path)):
        raise RuntimeError("Full-rollout sources or pilot replacements are missing")
    unrelated_before = _unrelated_fingerprint(actors, source_paths | pilot_paths)
    # Re-verify all persisted pilot actors before touching remaining sources.
    for row in pilot["replacements"]:
        actor = actor_by_path[row["replacement_actor"]]
        target = unreal.EditorAssetLibrary.load_asset(row["target_static_mesh"])
        _, failures = _verify_new_actor(actor, row, target)
        if failures:
            raise RuntimeError("Pilot actor changed before full rollout: {}".format(failures))
    created = []
    batch_size = 40
    try:
        for start in range(0, len(replace_rows), batch_size):
            batch_rows = replace_rows[start:start + batch_size]
            batch = []
            current_by_path = {actor.get_path_name(): actor for actor in subsystem.get_all_level_actors()}
            for row in batch_rows:
                source_actor = current_by_path.get(row["before"]["object_path"])
                if not source_actor:
                    raise RuntimeError("Planned source missing: {}".format(row["before"]["object_path"]))
                source_component = _component(source_actor, unreal.SkeletalMeshComponent)
                item = _spawn_verified_replacement(
                    subsystem, source_actor, source_component, row, validation_by_source)
                batch.append(item)
            # No source is removed until every replacement in this batch verifies.
            for item in batch:
                if not subsystem.destroy_actor(item["source"]):
                    raise RuntimeError("Could not remove verified source {}".format(
                        item["row"]["before"]["object_path"]))
            created.extend(batch)
            live = list(subsystem.get_all_level_actors())
            remaining_mapped = 0
            for actor in live:
                components = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
                if len(components) == 1 and _source_mesh(components[0]) in mapping:
                    remaining_mapped += 1
            expected_remaining = 281 - len(created)
            if remaining_mapped != expected_remaining:
                raise RuntimeError("Batch integrity count mismatch: {} != {}".format(
                    remaining_mapped, expected_remaining))
            unreal.log("{} FULL BATCH PASS: replaced {}/266; mapped skeletal remaining={}".format(
                LOG, len(created), remaining_mapped))
        # Delete only the exact rejected, reference-reviewed, attachment-free set.
        current_by_path = {actor.get_path_name(): actor for actor in subsystem.get_all_level_actors()}
        deleted = []
        for row in delete_rows:
            actor = current_by_path.get(row["before"]["object_path"])
            if not actor:
                raise RuntimeError("Rejected actor missing before authorized deletion")
            component = _component(actor, unreal.SkeletalMeshComponent)
            if _source_mesh(component) != row["source_skeletal_mesh"]:
                raise RuntimeError("Rejected actor source changed")
            if actor.get_attach_parent_actor() or list(actor.get_attached_actors()):
                raise RuntimeError("Rejected actor gained an attachment; retaining it")
            current = _actor_snapshot(actor, component)
            for key in ("location", "rotation", "scale"):
                if not _close(current[key], row["before"][key]):
                    raise RuntimeError("Rejected actor transform changed; retaining it")
            if not subsystem.destroy_actor(actor):
                raise RuntimeError("Could not delete authorized rejected actor")
            deleted.append(row)
        final_actors = list(subsystem.get_all_level_actors())
        mapped_skeletal = []
        for actor in final_actors:
            components = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
            if len(components) == 1 and _source_mesh(components[0]) in mapping:
                mapped_skeletal.append(actor.get_path_name())
        if mapped_skeletal:
            raise RuntimeError("Mapped skeletal vegetation remains: {}".format(mapped_skeletal))
        target_paths = {_object_path(row["target_static_mesh"])
                        for row in inventory["source_to_target_mapping"]
                        if validation_by_source[row["source_skeletal_mesh"]]["automated_checks_passed"]}
        static_vegetation = []
        for actor in final_actors:
            components = list(actor.get_components_by_class(unreal.StaticMeshComponent))
            if len(components) == 1 and _object_path(_value(components[0], "static_mesh")) in target_paths:
                static_vegetation.append(actor)
        if len(static_vegetation) != 290:
            raise RuntimeError("Expected 290 target static vegetation actors, found {}".format(
                len(static_vegetation)))
        replacement_rows = list(pilot["replacements"])
        for item in created:
            component, failures = _verify_new_actor(item["actor"], item["row"], item["asset"])
            if failures:
                raise RuntimeError("Final replacement verification failed: {}".format(failures))
            replacement_rows.append({
                "source_actor": item["row"]["before"]["object_path"],
                "replacement_actor": item["actor"].get_path_name(),
                "species": item["row"]["species"], "variant": item["row"]["variant"],
                "source_skeletal_mesh": item["row"]["source_skeletal_mesh"],
                "target_static_mesh": item["row"]["target_static_mesh"],
                "before": item["row"]["before"], "after": _actor_snapshot(item["actor"], component),
                "verified": True,
            })
        if len(replacement_rows) != 290 or len({row["replacement_actor"] for row in replacement_rows}) != 290:
            raise RuntimeError("Final replacement audit is not exactly 290 unique actors")
        replacement_paths = {row["replacement_actor"] for row in replacement_rows}
        unrelated_after = _unrelated_fingerprint(final_actors, replacement_paths)
        if unrelated_after != unrelated_before:
            raise RuntimeError("Unrelated actor identity/organization fingerprint changed")
        final_paths = {actor.get_path_name() for actor in final_actors}
        if delete_paths & final_paths or source_paths & final_paths:
            raise RuntimeError("A source/rejected actor remains after final in-memory audit")
        audit = {
            "schema": 1, "mode": "production_full_rollout", "map": MAP_ASSET,
            "plan_sha256": _sha256(FULL_PLAN_PATH), "map_sha256_before": before_hash,
            "map_sha256_after": None, "batch_size": batch_size, "batch_count": 7,
            "pilot_replacement_count": 24, "additional_replacement_count": len(created),
            "total_static_vegetation_count": len(replacement_rows),
            "remaining_mapped_skeletal_count": 0, "deleted_rejected_count": len(deleted),
            "deleted_by_variant": {"Beach_Rose_C": 11, "Common_Hazel_B": 4},
            "unrelated_actor_fingerprint_before": unrelated_before,
            "unrelated_actor_fingerprint_after": unrelated_after,
            "all_replacements_verified": True,
            "collision_policy": "NoCollision; simulate_physics=False; WPO disabled",
            "production_map_saved": False, "replacements": replacement_rows,
            "deleted": [{"actor": row["before"]["object_path"], "species": row["species"],
                         "variant": row["variant"], "source": row["source_skeletal_mesh"],
                         "before": row["before"]} for row in deleted],
        }
        _write(FULL_AUDIT_PATH, audit)
        if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
            raise RuntimeError("Full rollout passed in memory but production map save failed")
        audit["map_sha256_after"] = _sha256(MAP_FILE)
        audit["production_map_saved"] = True
        _write(FULL_AUDIT_PATH, audit)
        unreal.log("{} FULL SUCCESS: 290 static, 0 mapped skeletal, 15 rejected deleted".format(LOG))
        return audit
    except Exception:
        failure = {
            "schema": 1, "mode": "production_full_rollout", "production_map_saved": False,
            "map_sha256_on_disk": _sha256(MAP_FILE), "error": traceback.format_exc(),
            "recovery": "No save occurs before all batch/final gates pass; use the post-pilot checkpoint if the disk hash changed.",
        }
        _write(FULL_AUDIT_PATH, failure)
        raise


def verify_full():
    audit = _load(FULL_AUDIT_PATH)
    if not audit.get("production_map_saved") or audit.get("total_static_vegetation_count") != 290:
        raise RuntimeError("Successful saved full-rollout audit is required")
    if _sha256(MAP_FILE) != audit["map_sha256_after"]:
        raise RuntimeError("On-disk map differs from the full-rollout audit")
    inventory, _, mapping, _ = _mapping_and_validation()
    _, actors = _load_map_and_actors()
    actor_by_path = {actor.get_path_name(): actor for actor in actors}
    replacement_paths = {row["replacement_actor"] for row in audit["replacements"]}
    if len(replacement_paths) != 290 or not replacement_paths.issubset(set(actor_by_path)):
        raise RuntimeError("One or more of the 290 replacements is absent after reload")
    for row in audit["replacements"]:
        actor = actor_by_path[row["replacement_actor"]]
        target = unreal.EditorAssetLibrary.load_asset(row["target_static_mesh"])
        _, failures = _verify_new_actor(actor, row, target)
        if failures:
            raise RuntimeError("Reloaded full replacement failed: {} {}".format(
                row["replacement_actor"], failures))
    mapped_skeletal = []
    for actor in actors:
        components = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
        if len(components) == 1 and _source_mesh(components[0]) in mapping:
            mapped_skeletal.append(actor.get_path_name())
    if mapped_skeletal:
        raise RuntimeError("Mapped skeletal vegetation remains after reload")
    deleted_paths = {row["actor"] for row in audit["deleted"]}
    if len(deleted_paths) != 15 or deleted_paths & set(actor_by_path):
        raise RuntimeError("Rejected deletion set did not persist after reload")
    unrelated = _unrelated_fingerprint(actors, replacement_paths)
    if unrelated != audit["unrelated_actor_fingerprint_before"]:
        raise RuntimeError("Unrelated actor fingerprint changed after full reload")
    result = {
        "schema": 1, "mode": "full_rollout_reload_audit", "passed": True,
        "map_sha256": _sha256(MAP_FILE), "static_vegetation_count": 290,
        "mapped_skeletal_vegetation_count": 0, "deleted_rejected_count": 15,
        "all_mesh_transform_render_collision_checks_passed": True,
        "unrelated_fingerprint_matches": True, "map_saved": False,
    }
    _write(FULL_RELOAD_AUDIT_PATH, result)
    unreal.log("{} FULL RELOAD PASS: 290 static, 0 skeletal, 15 deleted".format(LOG))
    return result


def run():
    if MODE == "plan":
        return plan()
    if MODE == "smoke":
        return smoke()
    if MODE == "diagnose_spawn":
        return diagnose_spawn()
    if MODE == "diagnose_scan":
        return diagnose_scan()
    if MODE == "apply":
        return apply()
    if MODE == "verify_pilot":
        return verify_pilot()
    if MODE == "plan_full":
        return plan_full()
    if MODE == "apply_full":
        return apply_full()
    if MODE == "verify_full":
        return verify_full()
    raise RuntimeError("Unsupported VHV_STATIC_VEGETATION_PILOT_MODE={}".format(MODE))


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} FAILED:\n{}".format(LOG, traceback.format_exc()))
        raise
