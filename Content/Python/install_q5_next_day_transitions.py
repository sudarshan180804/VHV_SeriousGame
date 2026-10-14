"""Install two Q5 next-day entry transitions and update one Q5 ordering activity."""

import datetime
import hashlib
import json
import math
import os
import traceback

import unreal


TARGET_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
LEVEL_DATA = "/Game/VHV_Stuff/HBCT/Activities/DA_Level_HBCT"
QUEST_ARC = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
QUEST_ID = "Q_HBCT_05_SELF_MONITORING"
ACTIVITY_ID = "VHV.Activity.HBCT.SelfMonitoring.ProgressGraphOrdering"
CONTROLLER_LABEL = "VHV_Q5_ObjectiveDisplayVisibility"
PLANE_LABEL = "Plane"
TRANSITIONS = [
    {
        "Sequence": "VHV_Q5_TimeSkip_NextDay_TargetBehavior",
        "Target": "VHV_Q5_TimeSkip_Arrival_TargetBehavior",
        "SourceObjective": "O02_LearnWhyRecordsMatter",
        "TargetObjective": "O03_IdentifyTargetBehavior",
        "StageID": "TargetBehaviorNextDay",
        "Reference": "HBCT_SM_Trigger_TargetBehaviorArea",
    },
    {
        "Sequence": "VHV_Q5_TimeSkip_NextDay_SelfMonitor",
        "Target": "VHV_Q5_TimeSkip_Arrival_SelfMonitor",
        "SourceObjective": "O16_SetSelfReminder",
        "TargetObjective": "O17_WatchChaiSelfMonitor",
        "StageID": "ChaiIndependentNextDay",
        "Reference": "HBCT_SM_Trigger_SelfMonitorPayoff",
    },
]
OUTPUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "VHV_Q5NextDay")


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def transform(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return [
        float(location.x), float(location.y), float(location.z),
        float(rotation.pitch), float(rotation.yaw), float(rotation.roll),
        float(scale.x), float(scale.y), float(scale.z),
    ]


def actor_label(actor):
    return actor.get_actor_label() if actor else ""


def exact_actor(by_label, label):
    matches = by_label.get(label, [])
    if len(matches) != 1:
        raise RuntimeError("Expected exactly one actor '{}'; found {}".format(label, len(matches)))
    return matches[0]


def decode_hit(raw_hit):
    if raw_hit is None:
        return None
    if isinstance(raw_hit, tuple) and len(raw_hit) == 2 and isinstance(raw_hit[0], bool):
        if not raw_hit[0]:
            return None
        raw_hit = raw_hit[1]
    values = raw_hit.to_tuple()
    if len(values) < 18:
        return None
    return {
        "Blocking": bool(values[0]),
        "Impact": values[5],
        "Actor": values[9],
        "Component": values[10],
    }


def resolve_player_capsule():
    game_mode_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode")
    if not game_mode_class:
        raise RuntimeError("Could not load BP_ThirdPersonGameMode")
    pawn_class = unreal.get_default_object(game_mode_class).get_editor_property("default_pawn_class")
    pawn_default = unreal.get_default_object(pawn_class)
    capsules = list(pawn_default.get_components_by_class(unreal.CapsuleComponent) or [])
    if len(capsules) != 1:
        raise RuntimeError("Default player pawn must have exactly one capsule")
    capsule = capsules[0]
    return (pawn_class.get_name(), float(capsule.get_scaled_capsule_radius()),
            float(capsule.get_scaled_capsule_half_height()))


def validate_arrival(world, plane, xy, yaw, radius, half_height):
    start = unreal.Vector(float(xy[0]), float(xy[1]), 5000.0)
    end = unreal.Vector(float(xy[0]), float(xy[1]), -5000.0)
    raw_hits = unreal.SystemLibrary.line_trace_multi(
        world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
        unreal.DrawDebugTrace.NONE, True) or []
    plane_hit = None
    hit_rows = []
    for raw_hit in raw_hits:
        hit = decode_hit(raw_hit)
        if not hit:
            continue
        hit_rows.append({
            "Actor": actor_label(hit["Actor"]),
            "Component": hit["Component"].get_name() if hit["Component"] else "",
            "Blocking": hit["Blocking"],
            "Impact": [float(hit["Impact"].x), float(hit["Impact"].y), float(hit["Impact"].z)],
        })
        if hit["Blocking"] and hit["Actor"] == plane:
            plane_hit = hit
            break
    if not plane_hit:
        raise RuntimeError("Arrival {} has no blocking Plane support: {}".format(xy, hit_rows))

    ground_z = float(plane_hit["Impact"].z)
    center = unreal.Vector(float(xy[0]), float(xy[1]), ground_z + half_height + 3.0)
    raw_clearance = unreal.SystemLibrary.capsule_trace_single(
        world, center, unreal.Vector(center.x, center.y, center.z + 1.0),
        radius, half_height, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [plane],
        unreal.DrawDebugTrace.NONE, False, unreal.LinearColor(), unreal.LinearColor(), 0.0)
    clearance = decode_hit(raw_clearance)
    if clearance and clearance["Blocking"]:
        raise RuntimeError("Arrival capsule blocked by {}/{}".format(
            actor_label(clearance["Actor"]),
            clearance["Component"].get_name() if clearance["Component"] else ""))
    return center, unreal.Rotator(pitch=0.0, yaw=float(yaw), roll=0.0), hit_rows


def activity_tag_name(activity):
    tag = activity.get_editor_property("activity_tag")
    return str(unreal.GameplayTagLibrary.get_tag_name(tag))


def ordering_item(item_id, text):
    item = unreal.OrderingItem()
    item.set_editor_property("item_id", item_id)
    item.set_editor_property("item_text", text)
    return item


def story_effect_fingerprint(effect):
    """Compare serialized story-effect values, not transient Python struct wrappers."""
    state_tag = effect.get_editor_property("state_tag")
    return (
        str(effect.get_editor_property("effect_type")),
        str(unreal.GameplayTagLibrary.get_tag_name(state_tag)),
        str(effect.get_editor_property("state_id")),
        int(effect.get_editor_property("value")),
    )


def update_ordering_activity():
    level = unreal.EditorAssetLibrary.load_asset(LEVEL_DATA)
    if not level:
        raise RuntimeError("DA_Level_HBCT is missing")
    day = level.get_editor_property("day_data")
    topics = list(day.get_editor_property("topics"))
    found = 0
    for topic_index, topic in enumerate(topics):
        activities = list(topic.get_editor_property("activities"))
        for activity_index, activity in enumerate(activities):
            if activity_tag_name(activity) != ACTIVITY_ID:
                continue
            found += 1
            if activity.get_editor_property("activity_type") != unreal.TextbookActivityType.ORDERING:
                raise RuntimeError("ProgressGraphOrdering is no longer an Ordering activity")
            success_effects_before = [
                story_effect_fingerprint(effect)
                for effect in activity.get_editor_property("success_effects")
            ]
            activity.set_editor_property("activity_title", "Help Uncle Chai Build a Walking Routine")
            activity.set_editor_property(
                "prompt_text",
                "Arrange the milestones from a manageable starting goal to a routine he can maintain.")
            activity.set_editor_property("ordering_items", [
                ordering_item("1", "START SMALL\nBegin with 3 walking days per week."),
                ordering_item("2", "BUILD CONSISTENCY\nIncrease to 4 walking days per week."),
                ordering_item("3", "REACH THE GOAL\nWork up to 5 walking days per week."),
                ordering_item("4", "MAINTAIN THE HABIT\nContinue the 5-day goal consistently."),
            ])
            activity.set_editor_property("correct_order", ["1", "2", "3", "4"])
            activity.set_editor_property(
                "correct_feedback",
                "Great! Small, achievable increases help Uncle Chai build a routine he can keep.")
            retry_text = (
                "Think about starting with a manageable goal, gradually increasing it, "
                "and then maintaining the routine.")
            activity.set_editor_property("incorrect_feedback", retry_text)
            activity.set_editor_property("hints", [retry_text, retry_text])
            success_effects_after = [
                story_effect_fingerprint(effect)
                for effect in activity.get_editor_property("success_effects")
            ]
            if success_effects_after != success_effects_before:
                raise RuntimeError("Ordering activity completion effects changed")
            activities[activity_index] = activity
        topic.set_editor_property("activities", activities)
        topics[topic_index] = topic
    if found != 1:
        raise RuntimeError("Expected one ProgressGraphOrdering activity; found {}".format(found))
    day.set_editor_property("topics", topics)
    level.modify()
    level.set_editor_property("day_data", day)
    return level


def validate_q5_boundaries():
    arc = unreal.EditorAssetLibrary.load_asset(QUEST_ARC)
    if not arc:
        raise RuntimeError("DA_QuestArc_HBCT is missing")
    quests = list(arc.get_editor_property("quests"))
    quest = next((entry for entry in quests
                  if str(entry.get_editor_property("quest_id")) == QUEST_ID), None)
    if not quest:
        raise RuntimeError("Quest 5 is missing")
    objectives = list(quest.get_editor_property("objectives"))
    ids = [str(entry.get_editor_property("objective_id")) for entry in objectives]
    for source, target in (("O02_LearnWhyRecordsMatter", "O03_IdentifyTargetBehavior"),
                           ("O16_SetSelfReminder", "O17_WatchChaiSelfMonitor")):
        if ids.index(target) != ids.index(source) + 1:
            raise RuntimeError("{} is not immediately followed by {}".format(source, target))
    o15 = objectives[ids.index("O15_BuildProgressGraph")]
    if str(o15.get_editor_property("entry_node_id")) != "SMGraph_Activity":
        raise RuntimeError("O15 no longer launches SMGraph_Activity")
    o17 = objectives[ids.index("O17_WatchChaiSelfMonitor")]
    if (o17.get_editor_property("objective_type") != unreal.VHVQuestObjectiveType.WORLD_ACTION
            or o17.get_editor_property("world_action_start_policy")
            != unreal.VHVWorldActionStartPolicy.IMMEDIATE):
        raise RuntimeError("O17 world-action behavior changed unexpectedly")
    return ids


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world_path = world.get_path_name().split(":", 1)[0] if world else ""
    if not world or not world_path.startswith(TARGET_MAP + "."):
        raise RuntimeError("Lvl_Village_Main must be the loaded Editor world")
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors_before = list(actor_subsystem.get_all_level_actors())
    by_label = {}
    for actor in actors_before:
        by_label.setdefault(actor.get_actor_label(), []).append(actor)
    new_labels = [value for config in TRANSITIONS
                  for value in (config["Sequence"], config["Target"])]
    duplicates = [label for label in new_labels if by_label.get(label)]
    if duplicates:
        raise RuntimeError("Refusing duplicate installation: {}".format(duplicates))

    project_dir = unreal.Paths.project_dir()
    main_file = os.path.join(project_dir, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
    blockout_file = os.path.join(project_dir, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Blockout.umap")
    main_hash_before = sha256(main_file)
    blockout_hash_before = sha256(blockout_file)
    transforms_before = {actor.get_path_name(): transform(actor) for actor in actors_before}

    q3_sequences = [actor for actor in actors_before
                    if actor.get_class().get_name() == "VHVTimeSkipSequence"
                    and str(actor.get_editor_property("required_quest_id")) == "Q_HBCT_03_ROLE_MODEL"]
    q3_before = [(actor.get_path_name(), transform(actor),
                  float(actor.get_editor_property("fade_in_duration")),
                  float(actor.get_editor_property("text_hold_duration")),
                  float(actor.get_editor_property("fade_out_duration")),
                  len(list(actor.get_editor_property("stages")))) for actor in q3_sequences]
    daily = exact_actor(by_label, "VHV_Q5_TimeSkip_DailyRecording")
    daily_before = (transform(daily), float(daily.get_editor_property("fade_in_duration")),
                    float(daily.get_editor_property("text_hold_duration")),
                    float(daily.get_editor_property("fade_out_duration")),
                    [str(stage.get_editor_property("stage_id"))
                     for stage in daily.get_editor_property("stages")])
    display_controller = exact_actor(by_label, CONTROLLER_LABEL)
    existing_additional = list(display_controller.get_editor_property("additional_time_skip_sequences") or [])
    if existing_additional:
        raise RuntimeError("Q5 display controller already has additional time-skip sequences")

    objective_ids = validate_q5_boundaries()
    level_data = update_ordering_activity()
    plane = exact_actor(by_label, PLANE_LABEL)
    pawn_class, radius, half_height = resolve_player_capsule()

    plane_rotation = plane.get_actor_rotation()
    plane_scale = plane.get_actor_scale3d()
    yaw_radians = math.radians(float(plane_rotation.yaw))
    local_x = (math.cos(yaw_radians), math.sin(yaw_radians))
    local_y = (-math.sin(yaw_radians), math.cos(yaw_radians))
    axis = local_x if abs(float(plane_scale.x)) >= abs(float(plane_scale.y)) else local_y
    endpoint_direction = (-10346.0 - -2149.0, 18286.0 - 18305.0)
    if axis[0] * endpoint_direction[0] + axis[1] * endpoint_direction[1] < 0.0:
        axis = (-axis[0], -axis[1])

    created = []
    rows = []
    sequences = []
    try:
        for config in TRANSITIONS:
            reference = exact_actor(by_label, config["Reference"])
            reference_location = reference.get_actor_location()
            xy = (reference_location.x - axis[0] * 350.0,
                  reference_location.y - axis[1] * 350.0)
            facing_yaw = math.degrees(math.atan2(
                reference_location.y - xy[1], reference_location.x - xy[0]))
            center, rotation, support_hits = validate_arrival(
                world, plane, xy, facing_yaw, radius, half_height)
            target = actor_subsystem.spawn_actor_from_class(unreal.TargetPoint, center, rotation)
            sequence = actor_subsystem.spawn_actor_from_class(
                unreal.VHVTimeSkipSequence, unreal.Vector(0.0, 0.0, 0.0),
                unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
            if not target or not sequence:
                raise RuntimeError("Failed to create next-day transition actors")
            target.set_actor_label(config["Target"])
            sequence.set_actor_label(config["Sequence"])
            target.set_folder_path("Village_Main/Q5_TimeSkip/NextDay")
            sequence.set_folder_path("Village_Main/Q5_TimeSkip/NextDay")
            created.extend([target, sequence])

            stage = unreal.VHVTimeSkipStage()
            stage.set_editor_property("stage_id", config["StageID"])
            stage.set_editor_property("objective_id", config["TargetObjective"])
            stage.set_editor_property("transition_text", "The next day...")
            stage.set_editor_property("observation_scene", None)
            stage.set_editor_property("stage_actors", [])
            stage.set_editor_property("player_arrival_target", target)
            stage.set_editor_property("player_arrival_support_actor", plane)
            stage.set_editor_property("legacy_trigger", None)
            stage.set_editor_property("auto_start_observation_after_transition", False)
            stage.set_editor_property("post_transition_observation_delay", 0.0)
            stage.set_editor_property("presentation_only", True)

            sequence.set_editor_property("required_quest_id", QUEST_ID)
            sequence.set_editor_property("required_objective_id", config["SourceObjective"])
            sequence.set_editor_property("progress_counter_id", "")
            sequence.set_editor_property("advance_on_objective_completion", False)
            sequence.set_editor_property("transition_on_next_objective_activation", True)
            sequence.set_editor_property("require_navigation_projection", False)
            sequence.set_editor_property("fade_in_duration", 0.45)
            sequence.set_editor_property("text_hold_duration", 1.30)
            sequence.set_editor_property("fade_out_duration", 0.45)
            sequence.set_editor_property("transition_fail_safe_duration", 3.0)
            sequence.set_editor_property("stages", [stage])
            sequences.append(sequence)
            rows.append({
                "SourceObjective": config["SourceObjective"],
                "TargetObjective": config["TargetObjective"],
                "Sequence": config["Sequence"],
                "ArrivalTarget": config["Target"],
                "ArrivalXYZ": transform(target)[:3],
                "TransitionText": "The next day...",
                "SupportHits": support_hits,
            })

        display_controller.set_editor_property("additional_time_skip_sequences", sequences)

        actors_after = list(actor_subsystem.get_all_level_actors())
        changed_transforms = [actor.get_actor_label() for actor in actors_after
                              if actor.get_path_name() in transforms_before
                              and transform(actor) != transforms_before[actor.get_path_name()]]
        if changed_transforms:
            raise RuntimeError("Pre-existing transforms changed: {}".format(changed_transforms))
        if len(actors_after) != len(actors_before) + 4:
            raise RuntimeError("Expected exactly four new actors")
        if list(display_controller.get_editor_property("additional_time_skip_sequences")) != sequences:
            raise RuntimeError("Display controller next-day sequence references failed read-back")

        q3_after = [(actor.get_path_name(), transform(actor),
                     float(actor.get_editor_property("fade_in_duration")),
                     float(actor.get_editor_property("text_hold_duration")),
                     float(actor.get_editor_property("fade_out_duration")),
                     len(list(actor.get_editor_property("stages")))) for actor in q3_sequences]
        daily_after = (transform(daily), float(daily.get_editor_property("fade_in_duration")),
                       float(daily.get_editor_property("text_hold_duration")),
                       float(daily.get_editor_property("fade_out_duration")),
                       [str(stage.get_editor_property("stage_id"))
                        for stage in daily.get_editor_property("stages")])
        if q3_after != q3_before or daily_after != daily_before:
            raise RuntimeError("Existing Q3 or Q5 daily time-skip configuration changed")
        if sha256(blockout_file) != blockout_hash_before:
            raise RuntimeError("Protected blockout changed before save")

        # Save only the Q5 activity asset and the target map after all preflight checks pass.
        if not unreal.EditorAssetLibrary.save_asset(LEVEL_DATA, only_if_is_dirty=False):
            raise RuntimeError("Failed to save DA_Level_HBCT")
        if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
            raise RuntimeError("Failed to save Lvl_Village_Main")
        if sha256(blockout_file) != blockout_hash_before:
            raise RuntimeError("Protected blockout changed after save")

        report = {
            "Status": "SUCCESS",
            "GeneratedAtLocal": datetime.datetime.now().isoformat(),
            "Quest": QUEST_ID,
            "VerifiedObjectiveOrder": objective_ids,
            "Transitions": rows,
            "Timing": {"FadeIn": 0.45, "TextHold": 1.30, "FadeOut": 0.45,
                       "Total": 2.20, "FailSafe": 3.0},
            "Activity": {
                "ID": ACTIVITY_ID,
                "Objective": "O15_BuildProgressGraph",
                "Title": "Help Uncle Chai Build a Walking Routine",
                "Instruction": "Arrange the milestones from a manageable starting goal to a routine he can maintain.",
                "CorrectOrder": ["1", "2", "3", "4"],
                "Cards": [
                    "START SMALL - Begin with 3 walking days per week.",
                    "BUILD CONSISTENCY - Increase to 4 walking days per week.",
                    "REACH THE GOAL - Work up to 5 walking days per week.",
                    "MAINTAIN THE HABIT - Continue the 5-day goal consistently.",
                ],
                "CompletionEffectPreserved": "VHV.Story.Flag.HBCT.SelfMonitoring.SMProgressGraphBuilt",
            },
            "PlayerPawnClass": pawn_class,
            "CapsuleRadius": radius,
            "CapsuleHalfHeight": half_height,
            "ExistingActorTransformsChanged": 0,
            "ActorsCreated": new_labels,
            "Q3SequenceConfigurationChanged": False,
            "Q5DailySequenceConfigurationChanged": False,
            "Q0ToQ4ActorChanges": 0,
            "Q6ActorChanges": 0,
            "MainMapSaved": True,
            "MainMapHashBefore": main_hash_before,
            "MainMapHashAfter": sha256(main_file),
            "BlockoutHashBefore": blockout_hash_before,
            "BlockoutHashAfter": sha256(blockout_file),
            "BlockoutUntouched": sha256(blockout_file) == blockout_hash_before,
            "ManualPIERequired": True,
        }
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        report_path = os.path.join(
            OUTPUT_DIR,
            "q5_next_day_install_{}.json".format(datetime.datetime.now().strftime("%Y%m%d_%H%M%S")))
        with open(report_path, "w", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2)
        unreal.log("Q5_NEXT_DAY_SUCCESS=" + json.dumps({
            "Report": report_path,
            "Transitions": len(rows),
            "ExistingTransformsChanged": 0,
            "MainMapSaved": True,
        }))
    except Exception:
        display_controller.set_editor_property("additional_time_skip_sequences", existing_additional)
        for actor in reversed(created):
            if actor:
                actor_subsystem.destroy_actor(actor)
        raise


try:
    main()
except Exception:
    unreal.log_error("Q5_NEXT_DAY_FAILED:\n" + traceback.format_exc())
    raise
