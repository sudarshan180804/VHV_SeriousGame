"""Audit and safely correct converted VHV Mixamo NPC component orientation.

Modes are selected with VHV_NPC_ORIENTATION_MODE:
  AUDIT     - read-only inspection (default)
  TEST_ONE  - correct only VHV_NPC_ORIENTATION_TEST_LABEL
  APPLY_ALL - correct all 78 converted production NPCs

The correction is an absolute SkeletalMeshComponent relative rotation, never an
additive actor rotation. World transforms and gameplay components are verified
unchanged before the level is saved.
"""

import json
import os
import traceback

import unreal


MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
BP_PATH = "/Game/VHV_Stuff/NPC/BP_VHVNPCBase"
OUTPUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "Diagnostics", "vhv_npc_orientation_result.json")
MODE = os.environ.get("VHV_NPC_ORIENTATION_MODE", "AUDIT").upper()
TEST_LABEL = os.environ.get("VHV_NPC_ORIENTATION_TEST_LABEL", "HBCT_Market_Vendor")
EXPECTED_TARGET_COUNT = 78


def path_name(value):
    return value.get_path_name() if value else ""


def vector(value):
    return [round(value.x, 4), round(value.y, 4), round(value.z, 4)]


def rotator(value):
    return [round(value.pitch, 4), round(value.yaw, 4), round(value.roll, 4)]


def transform(actor):
    return {
        "location": vector(actor.get_actor_location()),
        "rotation": rotator(actor.get_actor_rotation()),
        "scale": vector(actor.get_actor_scale3d()),
    }


def same_rotator(value, pitch, yaw, roll, tolerance=0.01):
    return (abs(value.pitch - pitch) <= tolerance
            and abs(value.yaw - yaw) <= tolerance
            and abs(value.roll - roll) <= tolerance)


def participant_id(actor):
    component = actor.get_quest_participant_component()
    return str(component.get_effective_participant_id()) if component else ""


def gameplay_components(actor):
    return (
        actor.get_quest_participant_component(), actor.get_dialogue_component(),
        actor.get_behavior_component(), actor.get_npc_interaction_component(),
        actor.get_patrol_component(), actor.get_quest_command_component())


def component_bounds(component):
    try:
        mesh = component.get_skeletal_mesh_asset()
        bounds = mesh.get_bounds()
        return {
            "local_origin": vector(bounds.origin), "local_extent": vector(bounds.box_extent),
            "local_bottom_z": round(bounds.origin.z - bounds.box_extent.z, 4),
            "radius": round(bounds.sphere_radius, 4),
        }
    except Exception as exc:
        return {"unavailable": str(exc)}


def imported_bounds(mesh):
    try:
        bounds = mesh.get_imported_bounds()
        return {
            "origin": vector(bounds.origin), "extent": vector(bounds.box_extent),
            "largest_axis": max(
                (("X", bounds.box_extent.x), ("Y", bounds.box_extent.y),
                 ("Z", bounds.box_extent.z)), key=lambda pair: pair[1])[0],
        }
    except Exception as exc:
        return {"unavailable": str(exc)}


def blueprint_default():
    try:
        bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_PATH)
        default = unreal.get_default_object(bp_class)
        mesh = default.get_editor_property("mesh")
        return {
            "mesh_relative_location": vector(mesh.get_editor_property("relative_location")),
            "mesh_relative_rotation": rotator(mesh.get_editor_property("relative_rotation")),
            "mesh_relative_scale": vector(mesh.get_editor_property("relative_scale3d")),
        }
    except Exception as exc:
        return {"unavailable": str(exc)}


def desired_relative_location(actor, component):
    mesh_path = path_name(component.get_skeletal_mesh_asset())
    mesh_z = -70.0 if "/kid1/" in mesh_path or "/kid2/" in mesh_path else -96.0
    return unreal.Vector(0.0, 0.0, mesh_z)


def desired_relative_rotation(actor):
    return unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0)


def run():
    if MODE not in {"AUDIT", "TEST_ONE", "APPLY_ALL"}:
        raise RuntimeError("Unsupported VHV_NPC_ORIENTATION_MODE: " + MODE)
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load " + MAP_PATH)

    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    npcs = [actor for actor in subsystem.get_all_level_actors()
            if isinstance(actor, unreal.VHVNPCCharacter)]
    targets = []
    protected = []
    for actor in npcs:
        mesh = actor.get_editor_property("mesh").get_skeletal_mesh_asset()
        mesh_path = path_name(mesh)
        if participant_id(actor) == "Instructor":
            protected.append(actor)
        elif mesh_path.startswith("/Game/Character/"):
            targets.append(actor)

    if len(targets) != EXPECTED_TARGET_COUNT:
        raise RuntimeError("Expected {} converted NPCs; found {}".format(
            EXPECTED_TARGET_COUNT, len(targets)))
    if len(protected) != 1:
        raise RuntimeError("Expected exactly one protected Instructor; found {}".format(
            len(protected)))

    before_world = {actor.get_path_name(): transform(actor) for actor in npcs}
    before_gameplay = {actor.get_path_name(): gameplay_components(actor) for actor in npcs}
    actor_rows = []
    for actor in sorted(targets, key=lambda value: value.get_actor_label()):
        component = actor.get_editor_property("mesh")
        rotation = component.get_editor_property("relative_rotation")
        expected_location = desired_relative_location(actor, component)
        expected_rotation = desired_relative_rotation(actor)
        mesh = component.get_skeletal_mesh_asset()
        actor_rows.append({
            "label": actor.get_actor_label(),
            "participant_id": participant_id(actor),
            "mesh": path_name(mesh),
            "skeleton": path_name(mesh.get_editor_property("skeleton")),
            "world_transform_before": before_world[actor.get_path_name()],
            "mesh_relative_location_before": vector(component.get_editor_property("relative_location")),
            "mesh_relative_rotation_before": rotator(rotation),
            "mesh_relative_scale_before": vector(component.get_editor_property("relative_scale3d")),
            "expected_relative_location": vector(expected_location),
            "expected_relative_rotation": rotator(expected_rotation),
            "mesh_bounds_before": component_bounds(component),
        })

    if MODE == "TEST_ONE":
        selected = [actor for actor in targets if actor.get_actor_label() == TEST_LABEL]
        if len(selected) != 1:
            raise RuntimeError("Expected exactly one test actor labelled {}; found {}".format(
                TEST_LABEL, len(selected)))
        actors_to_change = selected
    elif MODE == "APPLY_ALL":
        actors_to_change = targets
    else:
        actors_to_change = []

    changed_labels = []
    for actor in actors_to_change:
        component = actor.get_editor_property("mesh")
        expected_location = desired_relative_location(actor, component)
        expected_rotation = desired_relative_rotation(actor)
        current_location = component.get_editor_property("relative_location")
        current_rotation = component.get_editor_property("relative_rotation")
        if (vector(current_location) != vector(expected_location)
                or rotator(current_rotation) != rotator(expected_rotation)):
            actor.modify()
            component.modify()
            component.set_relative_location(expected_location, False, False)
            component.set_relative_rotation(expected_rotation, False, False)
            changed_labels.append(actor.get_actor_label())

    failures = []
    rows_by_label = {row["label"]: row for row in actor_rows}
    for actor in npcs:
        actor_path = actor.get_path_name()
        if transform(actor) != before_world[actor_path]:
            failures.append(actor.get_actor_label() + ": world transform changed")
        if gameplay_components(actor) != before_gameplay[actor_path]:
            failures.append(actor.get_actor_label() + ": gameplay component changed")
    for actor in targets:
        component = actor.get_editor_property("mesh")
        row = rows_by_label[actor.get_actor_label()]
        row["world_transform_after"] = transform(actor)
        row["mesh_relative_location_after"] = vector(component.get_editor_property("relative_location"))
        row["mesh_relative_rotation_after"] = rotator(component.get_editor_property("relative_rotation"))
        row["mesh_relative_scale_after"] = vector(component.get_editor_property("relative_scale3d"))
        row["mesh_world_rotation_after"] = rotator(component.get_world_rotation())
        row["mesh_world_location_after"] = vector(component.get_world_location())
        row["mesh_bounds_after"] = component_bounds(component)
        row["world_transform_preserved"] = row["world_transform_before"] == row["world_transform_after"]
        if actor in actors_to_change:
            relative_rotation = component.get_editor_property("relative_rotation")
            if not same_rotator(relative_rotation, 0.0, -90.0, 0.0):
                failures.append(actor.get_actor_label() + ": component correction was not applied")

    instructor = protected[0]
    instructor_component = instructor.get_editor_property("mesh")
    if transform(instructor) != before_world[instructor.get_path_name()]:
        failures.append("Instructor world transform changed")
    if failures:
        raise RuntimeError("Safety validation failed: " + "; ".join(failures))

    if actors_to_change and not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Failed to save Lvl_Village_Main")

    representative_meshes = {}
    for actor in targets:
        mesh = actor.get_editor_property("mesh").get_skeletal_mesh_asset()
        skeleton_path = path_name(mesh.get_editor_property("skeleton"))
        if skeleton_path not in representative_meshes:
            representative_meshes[skeleton_path] = {
                "actor": actor.get_actor_label(), "mesh": path_name(mesh),
                "imported_bounds": imported_bounds(mesh),
            }

    report = {
        "mode": MODE,
        "map": MAP_PATH,
        "converted_actor_count": len(targets),
        "changed_actor_count": len(changed_labels),
        "changed_labels": sorted(changed_labels),
        "verified_base_correction": {"pitch": 0.0, "yaw": -90.0, "roll": 0.0},
        "runtime_note": (
            "CharacterMovement normalizes authored actor Pitch/Roll during PIE; one shared "
            "Pitch=0/Yaw=-90/Roll=0 component correction is therefore used without compensation."),
        "blueprint_default": blueprint_default(),
        "protected_instructor": {
            "label": instructor.get_actor_label(),
            "world_transform": transform(instructor),
            "mesh": path_name(instructor_component.get_skeletal_mesh_asset()),
            "mesh_relative_rotation": rotator(
                instructor_component.get_editor_property("relative_rotation")),
            "unchanged": True,
        },
        "representative_meshes_by_skeleton": list(representative_meshes.values()),
        "actors": actor_rows,
        "validation": {
            "all_world_transforms_preserved": True,
            "all_gameplay_components_preserved": True,
            "instructor_unchanged": True,
            "absolute_component_rotation_used": True,
        },
    }
    with open(OUTPUT_PATH, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
    unreal.log_warning("VHV_NPC_ORIENTATION=" + OUTPUT_PATH)
    unreal.log_warning("VHV_NPC_ORIENTATION_COMPLETE mode={} changed={}".format(
        MODE, len(changed_labels)))
    return report


try:
    run()
except Exception:
    unreal.log_error(traceback.format_exc())
    raise
