"""Install the Q5 morning/afternoon/evening sequence without moving existing actors."""

import datetime
import hashlib
import json
import math
import os
import traceback

import unreal


TARGET_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
PROTECTED_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout"
SEQUENCE_LABEL = "VHV_Q5_TimeSkip_DailyRecording"
AFTERNOON_TARGET_LABEL = "VHV_Q5_TimeSkip_Arrival_Afternoon"
EVENING_TARGET_LABEL = "VHV_Q5_TimeSkip_Arrival_Evening"
OUTPUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "VHV_Q5TimeSkip")


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def transform_values(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return [
        float(location.x), float(location.y), float(location.z),
        float(rotation.pitch), float(rotation.yaw), float(rotation.roll),
        float(scale.x), float(scale.y), float(scale.z),
    ]


def map_path(world):
    outer = world.get_outer()
    package = outer.get_path_name() if outer else world.get_path_name()
    return package.split(".")[0]


def decode_hit_result(hit):
    if hit is None:
        return None
    if isinstance(hit, tuple) and len(hit) == 2 and isinstance(hit[0], bool):
        if not hit[0]:
            return None
        hit = hit[1]
    try:
        values = hit.to_tuple()
    except Exception:
        return None
    if len(values) < 18:
        return None
    return {
        "Blocking": bool(values[0]),
        "InitialOverlap": bool(values[1]),
        "Impact": values[5],
        "Actor": values[9],
        "Component": values[10],
    }


def label(actor):
    return actor.get_actor_label() if actor else ""


def exact_actor(by_label, actor_label):
    matches = by_label.get(actor_label, [])
    if len(matches) != 1:
        raise RuntimeError("Expected exactly one actor '%s'; found %d" % (actor_label, len(matches)))
    return matches[0]


def resolve_player_capsule():
    game_mode_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode")
    if not game_mode_class:
        raise RuntimeError("Could not load BP_ThirdPersonGameMode to verify the live pawn capsule")
    game_mode_default = unreal.get_default_object(game_mode_class)
    pawn_class = game_mode_default.get_editor_property("default_pawn_class")
    if not pawn_class:
        raise RuntimeError("BP_ThirdPersonGameMode has no DefaultPawnClass")
    pawn_default = unreal.get_default_object(pawn_class)
    capsules = list(pawn_default.get_components_by_class(unreal.CapsuleComponent) or [])
    if len(capsules) != 1:
        raise RuntimeError("Default player pawn resolves %d CapsuleComponents" % len(capsules))
    capsule = capsules[0]
    return pawn_class.get_name(), float(capsule.get_scaled_capsule_radius()), float(
        capsule.get_scaled_capsule_half_height())


def support_and_clearance(world, plane, xy, yaw, radius, half_height):
    start = unreal.Vector(float(xy[0]), float(xy[1]), 5000.0)
    end = unreal.Vector(float(xy[0]), float(xy[1]), -5000.0)
    hits = unreal.SystemLibrary.line_trace_multi(
        world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
        unreal.DrawDebugTrace.NONE, True) or []
    plane_hit = None
    hit_rows = []
    for raw_hit in hits:
        hit = decode_hit_result(raw_hit)
        if not hit:
            continue
        hit_rows.append({
            "Actor": label(hit["Actor"]),
            "Component": hit["Component"].get_name() if hit["Component"] else "",
            "Blocking": hit["Blocking"],
            "Impact": [float(hit["Impact"].x), float(hit["Impact"].y), float(hit["Impact"].z)],
        })
        if hit["Blocking"] and hit["Actor"] == plane:
            plane_hit = hit
            break
    if not plane_hit:
        raise RuntimeError("Arrival XY %s has no blocking Visibility hit on Plane; hits=%s" % (xy, hit_rows))
    ground_z = float(plane_hit["Impact"].z)
    center = unreal.Vector(float(xy[0]), float(xy[1]), ground_z + half_height + 3.0)
    finish = unreal.Vector(center.x, center.y, center.z + 1.0)
    raw_clearance = unreal.SystemLibrary.capsule_trace_single(
        world, center, finish, radius, half_height,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [plane],
        unreal.DrawDebugTrace.NONE, False, unreal.LinearColor(), unreal.LinearColor(), 0.0)
    clearance_hit = decode_hit_result(raw_clearance)
    if clearance_hit and clearance_hit["Blocking"]:
        raise RuntimeError(
            "Arrival capsule at %s is blocked by %s/%s" % (
                [center.x, center.y, center.z], label(clearance_hit["Actor"]),
                clearance_hit["Component"].get_name() if clearance_hit["Component"] else ""))
    return center, unreal.Rotator(pitch=0.0, yaw=float(yaw), roll=0.0), hit_rows


def make_stage(stage_id, objective_id, text, scene, actors, arrival, plane, trigger,
               auto_start, delay):
    stage = unreal.VHVTimeSkipStage()
    stage.set_editor_property("stage_id", stage_id)
    stage.set_editor_property("objective_id", objective_id)
    stage.set_editor_property("transition_text", text)
    stage.set_editor_property("observation_scene", scene)
    stage.set_editor_property("stage_actors", actors)
    stage.set_editor_property("player_arrival_target", arrival)
    stage.set_editor_property("player_arrival_support_actor", plane)
    stage.set_editor_property("legacy_trigger", trigger)
    stage.set_editor_property("auto_start_observation_after_transition", auto_start)
    stage.set_editor_property("post_transition_observation_delay", float(delay))
    return stage


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world or map_path(world) != TARGET_MAP:
        raise RuntimeError("Lvl_Village_Main must be the loaded Editor world")
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors_before = list(actor_subsystem.get_all_level_actors())
    by_label = {}
    for actor in actors_before:
        by_label.setdefault(actor.get_actor_label(), []).append(actor)
    for new_label in (SEQUENCE_LABEL, AFTERNOON_TARGET_LABEL, EVENING_TARGET_LABEL):
        if by_label.get(new_label):
            raise RuntimeError("Refusing duplicate installation; actor already exists: " + new_label)

    main_file = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir() + "VHV_Stuff/Maps/Lvl_Village_Main.umap")
    blockout_file = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir() + "VHV_Stuff/Maps/Lvl_Village_Blockout.umap")
    main_hash_before = sha256(main_file)
    blockout_hash_before = sha256(blockout_file)
    transforms_before = {actor.get_path_name(): transform_values(actor) for actor in actors_before}
    q3_sequences = [actor for actor in actors_before
                    if actor.get_class().get_name() == "VHVTimeSkipSequence"
                    and str(actor.get_editor_property("required_quest_id")) == "Q_HBCT_03_ROLE_MODEL"]
    q3_state_before = [{
        "Path": actor.get_path_name(),
        "Transform": transform_values(actor),
        "FadeIn": float(actor.get_editor_property("fade_in_duration")),
        "Hold": float(actor.get_editor_property("text_hold_duration")),
        "FadeOut": float(actor.get_editor_property("fade_out_duration")),
        "Stages": len(list(actor.get_editor_property("stages") or [])),
    } for actor in q3_sequences]

    plane = exact_actor(by_label, "Plane")
    breakfast_scene = exact_actor(by_label, "HBCT_SM_Ambient_Breakfast")
    afternoon_scene = exact_actor(by_label, "HBCT_SM_Ambient_AfternoonDrink")
    evening_scene = exact_actor(by_label, "HBCT_SM_Ambient_EveningWalk")
    breakfast_trigger = exact_actor(by_label, "HBCT_SM_Trigger_Breakfast")
    afternoon_trigger = exact_actor(by_label, "HBCT_SM_Trigger_Afternoon")
    evening_trigger = exact_actor(by_label, "HBCT_SM_Trigger_WalkStart")

    stage_actor_labels = {
        "MorningBreakfast": [
            "HBCT_SM_Ambient_Breakfast", "HBCT_SM_Ambient_RecordBreakfast",
            "HBCT_SM_Display_BreakfastRecord"],
        "AfternoonSweetDrink": [
            "HBCT_SM_Ambient_AfternoonDrink", "HBCT_SM_Ambient_RecordAfternoon",
            "HBCT_SM_Display_AfternoonRecord", "HBCT_SM_NPC_SelfMonitoringFriend"],
        "EveningWalk": [
            "HBCT_SM_Ambient_EveningWalk", "HBCT_SM_Ambient_RecordWalk",
            "HBCT_SM_Display_CompleteDay"],
    }
    stage_actors = {key: [exact_actor(by_label, item) for item in values]
                    for key, values in stage_actor_labels.items()}

    pawn_class_name, capsule_radius, capsule_half_height = resolve_player_capsule()
    plane_rotation = plane.get_actor_rotation()
    plane_scale = plane.get_actor_scale3d()
    yaw = math.radians(float(plane_rotation.yaw))
    local_x = (math.cos(yaw), math.sin(yaw))
    local_y = (-math.sin(yaw), math.cos(yaw))
    if abs(float(plane_scale.x)) >= abs(float(plane_scale.y)):
        axis = local_x
    else:
        axis = local_y
    endpoint_direction = (-10346.0 - -2149.0, 18286.0 - 18305.0)
    if axis[0] * endpoint_direction[0] + axis[1] * endpoint_direction[1] < 0.0:
        axis = (-axis[0], -axis[1])

    created = []
    afternoon_location = afternoon_trigger.get_actor_location()
    evening_location = evening_trigger.get_actor_location()
    afternoon_xy = (afternoon_location.x - axis[0] * 350.0,
                    afternoon_location.y - axis[1] * 350.0)
    evening_xy = (evening_location.x - axis[0] * 350.0,
                  evening_location.y - axis[1] * 350.0)
    afternoon_yaw = math.degrees(math.atan2(
        afternoon_location.y - afternoon_xy[1], afternoon_location.x - afternoon_xy[0]))
    evening_yaw = math.degrees(math.atan2(
        evening_location.y - evening_xy[1], evening_location.x - evening_xy[0]))

    afternoon_center, afternoon_rotation, afternoon_hits = support_and_clearance(
        world, plane, afternoon_xy, afternoon_yaw, capsule_radius, capsule_half_height)
    evening_center, evening_rotation, evening_hits = support_and_clearance(
        world, plane, evening_xy, evening_yaw, capsule_radius, capsule_half_height)

    try:
        afternoon_target = actor_subsystem.spawn_actor_from_class(
            unreal.TargetPoint, afternoon_center, afternoon_rotation)
        evening_target = actor_subsystem.spawn_actor_from_class(
            unreal.TargetPoint, evening_center, evening_rotation)
        sequence = actor_subsystem.spawn_actor_from_class(
            unreal.VHVTimeSkipSequence, unreal.Vector(0.0, 0.0, 0.0),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
        for actor, actor_label in ((afternoon_target, AFTERNOON_TARGET_LABEL),
                                   (evening_target, EVENING_TARGET_LABEL),
                                   (sequence, SEQUENCE_LABEL)):
            if not actor:
                raise RuntimeError("Failed to create " + actor_label)
            actor.set_actor_label(actor_label)
            actor.set_folder_path("Village_Main/Q5_TimeSkip")
            created.append(actor)

        stages = [
            make_stage(
                "MorningBreakfast", "O07_RecordBreakfast", "Later that afternoon...",
                breakfast_scene, stage_actors["MorningBreakfast"], None, plane,
                breakfast_trigger, False, 0.0),
            make_stage(
                "AfternoonSweetDrink", "O08_RecordAfternoon", "That evening...",
                afternoon_scene, stage_actors["AfternoonSweetDrink"], afternoon_target,
                plane, afternoon_trigger, True, 2.0),
            make_stage(
                "EveningWalk", "O09_RecordEveningWalk", "",
                evening_scene, stage_actors["EveningWalk"], evening_target,
                plane, evening_trigger, True, 2.0),
        ]
        sequence.set_editor_property("required_quest_id", "Q_HBCT_05_SELF_MONITORING")
        sequence.set_editor_property("required_objective_id", "O07_RecordBreakfast")
        sequence.set_editor_property("progress_counter_id", "")
        sequence.set_editor_property("advance_on_objective_completion", True)
        sequence.set_editor_property("require_navigation_projection", False)
        sequence.set_editor_property("fade_in_duration", 0.40)
        sequence.set_editor_property("text_hold_duration", 1.10)
        sequence.set_editor_property("fade_out_duration", 0.45)
        sequence.set_editor_property("transition_fail_safe_duration", 3.0)
        sequence.set_editor_property("stages", stages)

        actors_after = list(actor_subsystem.get_all_level_actors())
        existing_transform_changes = []
        for actor in actors_after:
            before = transforms_before.get(actor.get_path_name())
            if before is not None and transform_values(actor) != before:
                existing_transform_changes.append(actor.get_actor_label())
        if existing_transform_changes:
            raise RuntimeError("Pre-existing actor transforms changed: %s" % existing_transform_changes)
        if len(actors_after) != len(actors_before) + 3:
            raise RuntimeError("Expected exactly three new actors; before=%d after=%d" %
                               (len(actors_before), len(actors_after)))

        q3_state_after = [{
            "Path": actor.get_path_name(),
            "Transform": transform_values(actor),
            "FadeIn": float(actor.get_editor_property("fade_in_duration")),
            "Hold": float(actor.get_editor_property("text_hold_duration")),
            "FadeOut": float(actor.get_editor_property("fade_out_duration")),
            "Stages": len(list(actor.get_editor_property("stages") or [])),
        } for actor in q3_sequences]
        if q3_state_after != q3_state_before:
            raise RuntimeError("Existing Q3 TimeSkipSequence instance changed")
        if sha256(blockout_file) != blockout_hash_before:
            raise RuntimeError("Protected blockout changed before save")

        level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not level_editor.save_current_level():
            raise RuntimeError("Failed to save Lvl_Village_Main")
        if sha256(blockout_file) != blockout_hash_before:
            raise RuntimeError("Protected blockout changed after save")

        report = {
            "Status": "SUCCESS",
            "GeneratedAtLocal": datetime.datetime.now().isoformat(),
            "TargetMap": TARGET_MAP,
            "Sequence": SEQUENCE_LABEL,
            "SequenceClass": sequence.get_class().get_name(),
            "ObjectiveIDs": ["O07_RecordBreakfast", "O08_RecordAfternoon", "O09_RecordEveningWalk"],
            "CompletionSignal": "UVHVQuestSubsystem::OnObjectiveCompleted",
            "TransitionSettings": {"FadeIn": 0.40, "TextHold": 1.10, "FadeOut": 0.45,
                                   "FailSafe": 3.0, "PostTransitionObservationDelay": 2.0},
            "Stages": [
                {"StageID": "MorningBreakfast", "ObjectiveID": "O07_RecordBreakfast",
                 "Scene": label(breakfast_scene), "TransitionText": "Later that afternoon...",
                 "StageActors": stage_actor_labels["MorningBreakfast"], "ArrivalTarget": ""},
                {"StageID": "AfternoonSweetDrink", "ObjectiveID": "O08_RecordAfternoon",
                 "Scene": label(afternoon_scene), "TransitionText": "That evening...",
                 "StageActors": stage_actor_labels["AfternoonSweetDrink"],
                 "ArrivalTarget": AFTERNOON_TARGET_LABEL,
                 "ArrivalXYZ": transform_values(afternoon_target)[:3]},
                {"StageID": "EveningWalk", "ObjectiveID": "O09_RecordEveningWalk",
                 "Scene": label(evening_scene), "TransitionText": "",
                 "StageActors": stage_actor_labels["EveningWalk"],
                 "ArrivalTarget": EVENING_TARGET_LABEL,
                 "ArrivalXYZ": transform_values(evening_target)[:3]},
            ],
            "SharedChaiHandling": "Existing objective ActivationNPCReadiness/NPCMoveStages; not a StageActor",
            "PlayerPawnClass": pawn_class_name,
            "PlayerCapsuleRadius": capsule_radius,
            "PlayerCapsuleHalfHeight": capsule_half_height,
            "ArrivalSupportActor": label(plane),
            "ArrivalSupportTrace": {"Afternoon": afternoon_hits, "Evening": evening_hits},
            "ExistingActorTransformsChanged": 0,
            "Q3SequenceInstancesVerifiedUnchanged": len(q3_sequences),
            "ActorsCreated": [SEQUENCE_LABEL, AFTERNOON_TARGET_LABEL, EVENING_TARGET_LABEL],
            "MainMapSaved": True,
            "MainMapHashBefore": main_hash_before,
            "MainMapHashAfter": sha256(main_file),
            "BlockoutHashBefore": blockout_hash_before,
            "BlockoutHashAfter": sha256(blockout_file),
            "BlockoutUntouched": True,
            "ManualPIERequired": True,
        }
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        report_path = os.path.join(
            OUTPUT_DIR, "q5_time_skip_install_%s.json" % datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))
        with open(report_path, "w", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2)
        unreal.log("Q5_TIME_SKIP_INSTALL_SUCCESS=" + json.dumps({
            "Sequence": SEQUENCE_LABEL, "Report": report_path, "MapSaved": True}))
        return report
    except Exception:
        for actor in reversed(created):
            if actor:
                actor_subsystem.destroy_actor(actor)
        raise


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        failure_path = os.path.join(
            OUTPUT_DIR, "q5_time_skip_install_failed_%s.json" %
            datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))
        with open(failure_path, "w", encoding="utf-8") as stream:
            json.dump({"Status": "FAILED", "Error": str(exc),
                       "Traceback": traceback.format_exc(), "MainMapSaved": False},
                      stream, indent=2)
        unreal.log_error("Q5_TIME_SKIP_INSTALL_FAILED=" + str(exc))
        raise
