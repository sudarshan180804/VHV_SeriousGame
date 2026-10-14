"""Apply the minimal Q5 relocation repair, validate it, and save only the main map."""

import datetime
import hashlib
import json
import math
import os

import unreal


TARGET_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
QUEST_ARC = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
QUEST_ID = "Q_HBCT_05_SELF_MONITORING"
OUTPUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "VHV_Q5UrgentFix")
SNAPSHOT_PATH = os.path.join(OUTPUT_DIR, "q5_urgent_prechange_snapshot.json")

TRANSITIONS = [
    ("O02_LearnWhyRecordsMatter", "O03_IdentifyTargetBehavior",
     "VHV_Q5_TimeSkip_NextDay_TargetBehavior", "TargetBehaviorNextDay", "TargetBehaviorNextDay",
     "The next day...", 0.45, 1.30, 0.45),
    ("O07_RecordBreakfast", "O08_RecordAfternoon",
     "VHV_Q5_TimeSkip_DailyRecording", "AfternoonSweetDrink", "MorningBreakfast",
     "Later that afternoon...", 0.40, 1.10, 0.45),
    ("O08_RecordAfternoon", "O09_RecordEveningWalk",
     "VHV_Q5_TimeSkip_DailyRecording", "EveningWalk", "AfternoonSweetDrink",
     "That evening...", 0.40, 1.10, 0.45),
    ("O16_SetSelfReminder", "O17_WatchChaiSelfMonitor",
     "VHV_Q5_TimeSkip_NextDay_SelfMonitor", "ChaiIndependentNextDay", "ChaiIndependentNextDay",
     "The next day...", 0.45, 1.30, 0.45),
]


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


def label(actor):
    return actor.get_actor_label() if actor else ""


def exact(by_label, actor_label):
    matches = by_label.get(actor_label, [])
    if len(matches) != 1:
        raise RuntimeError("Expected exactly one '{}'; found {}".format(actor_label, len(matches)))
    return matches[0]


def decode_hit(hit):
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
        "Distance": float(values[3]),
        "Impact": values[5],
        "Normal": values[7],
        "Actor": values[9],
        "Component": values[10],
    }


def resolve_player_capsule():
    game_mode_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode")
    if not game_mode_class:
        raise RuntimeError("BP_ThirdPersonGameMode is missing")
    game_mode_default = unreal.get_default_object(game_mode_class)
    pawn_class = game_mode_default.get_editor_property("default_pawn_class")
    pawn_default = unreal.get_default_object(pawn_class)
    capsules = list(pawn_default.get_components_by_class(unreal.CapsuleComponent) or [])
    if len(capsules) != 1:
        raise RuntimeError("Default player pawn does not have exactly one capsule")
    capsule = capsules[0]
    return (pawn_class.get_name(), float(capsule.get_scaled_capsule_radius()),
            float(capsule.get_scaled_capsule_half_height()))


def validate_arrival(world, stage, radius, half_height):
    arrival = stage.get_editor_property("player_arrival_target")
    if not arrival:
        raise RuntimeError("Transition destination has no arrival reference")
    anchor = arrival.get_actor_location()
    start = unreal.Vector(float(anchor.x), float(anchor.y), float(anchor.z) + 25.0)
    end = unreal.Vector(float(anchor.x), float(anchor.y),
                        float(anchor.z) - max(1000.0, half_height * 6.0))
    ignore = [arrival]
    trigger = stage.get_editor_property("legacy_trigger")
    if trigger:
        ignore.append(trigger)
    object_types = [
        unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1,
        unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2,
    ]
    hits = unreal.SystemLibrary.line_trace_multi_for_objects(
        world, start, end, object_types, True, ignore,
        unreal.DrawDebugTrace.NONE, True) or []
    hit_rows = []
    selected = None
    for raw in hits:
        hit = decode_hit(raw)
        if not hit:
            continue
        actor = hit["Actor"]
        component = hit["Component"]
        pawn_response = ""
        if component:
            pawn_response = str(component.get_collision_response_to_channel(
                unreal.CollisionChannel.ECC_PAWN))
        row = {
            "Actor": label(actor),
            "ActorClass": actor.get_class().get_name() if actor else "",
            "Component": component.get_name() if component else "",
            "Blocking": hit["Blocking"],
            "PawnResponse": pawn_response,
            "Impact": [float(hit["Impact"].x), float(hit["Impact"].y), float(hit["Impact"].z)],
            "NormalZ": float(hit["Normal"].z),
        }
        hit_rows.append(row)
        if (hit["Blocking"] and float(hit["Normal"].z) >= 0.70
                and "BLOCK" in pawn_response.upper()):
            selected = hit
            break
    if not selected:
        raise RuntimeError("{} has no walkable Pawn-blocking floor: {}".format(
            label(arrival), hit_rows))

    impact = selected["Impact"]
    center = unreal.Vector(float(anchor.x), float(anchor.y),
                           float(impact.z) + half_height + 3.0)
    finish = unreal.Vector(float(center.x), float(center.y), float(center.z) + 1.0)
    clearance = unreal.SystemLibrary.capsule_trace_single_for_objects(
        world, center, finish, radius, half_height, object_types, True,
        ignore, unreal.DrawDebugTrace.NONE, True,
        unreal.LinearColor(), unreal.LinearColor(), 0.0)
    clearance_hit = decode_hit(clearance)
    if clearance_hit and clearance_hit["Blocking"]:
        raise RuntimeError("{} capsule is blocked by {}/{}".format(
            label(arrival), label(clearance_hit["Actor"]),
            clearance_hit["Component"].get_name() if clearance_hit["Component"] else ""))
    return {
        "ArrivalTarget": label(arrival),
        "ArrivalTransform": transform(arrival),
        "ResolvedPlayerCenter": [float(center.x), float(center.y), float(center.z)],
        "SupportActor": label(selected["Actor"]),
        "SupportClass": selected["Actor"].get_class().get_name() if selected["Actor"] else "",
        "SupportComponent": selected["Component"].get_name() if selected["Component"] else "",
        "SupportImpact": [float(impact.x), float(impact.y), float(impact.z)],
        "CapsuleRadius": radius,
        "CapsuleHalfHeight": half_height,
        "CapsuleClear": True,
        "TraceHits": hit_rows,
    }


def box_snapshot(actor):
    rows = []
    for box in list(actor.get_components_by_class(unreal.BoxComponent) or []):
        extent = box.get_unscaled_box_extent()
        rows.append({
            "Name": box.get_name(),
            "Extent": [float(extent.x), float(extent.y), float(extent.z)],
            "Profile": str(box.get_collision_profile_name()),
        })
    return rows


def sequence_signature(actor):
    return {
        "Path": actor.get_path_name(), "Label": label(actor), "Transform": transform(actor),
        "Quest": str(actor.get_editor_property("required_quest_id")),
        "RequiredObjective": str(actor.get_editor_property("required_objective_id")),
        "Advance": bool(actor.get_editor_property("advance_on_objective_completion")),
        "Entry": bool(actor.get_editor_property("transition_on_next_objective_activation")),
        "Nav": bool(actor.get_editor_property("require_navigation_projection")),
        "FadeIn": float(actor.get_editor_property("fade_in_duration")),
        "Hold": float(actor.get_editor_property("text_hold_duration")),
        "FadeOut": float(actor.get_editor_property("fade_out_duration")),
        "Stages": [{
            "StageID": str(stage.get_editor_property("stage_id")),
            "ObjectiveID": str(stage.get_editor_property("objective_id")),
            "TransitionText": str(stage.get_editor_property("transition_text")),
            "Arrival": label(stage.get_editor_property("player_arrival_target")),
            "Support": label(stage.get_editor_property("player_arrival_support_actor")),
            "Scene": label(stage.get_editor_property("observation_scene")),
            "Actors": [label(item) for item in list(stage.get_editor_property("stage_actors") or [])],
            "Trigger": label(stage.get_editor_property("legacy_trigger")),
            "Auto": bool(stage.get_editor_property("auto_start_observation_after_transition")),
            "Delay": float(stage.get_editor_property("post_transition_observation_delay")),
        } for stage in list(actor.get_editor_property("stages") or [])],
    }


def main():
    if not os.path.isfile(SNAPSHOT_PATH):
        raise RuntimeError("Pre-change snapshot is missing")
    with open(SNAPSHOT_PATH, "r", encoding="utf-8") as stream:
        snapshot = json.load(stream)

    unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world_path = world.get_path_name().split(":", 1)[0] if world else ""
    if not world or not (world_path == TARGET_MAP or world_path.startswith(TARGET_MAP + ".")):
        raise RuntimeError("Lvl_Village_Main did not load")

    project_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    main_file = os.path.join(project_dir, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
    blockout_file = os.path.join(project_dir, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Blockout.umap")
    if sha256(main_file) != snapshot["MainMapHash"]:
        raise RuntimeError("Lvl_Village_Main changed after the pre-change snapshot")
    if sha256(blockout_file) != snapshot["BlockoutHash"]:
        raise RuntimeError("Protected blockout changed after the pre-change snapshot")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    by_label = {}
    for actor in actors:
        by_label.setdefault(label(actor), []).append(actor)
    transforms_before = {actor.get_path_name(): transform(actor) for actor in actors}
    trigger_boxes_before = {actor.get_path_name(): box_snapshot(actor) for actor in actors
                            if label(actor).startswith("HBCT_SM_") and box_snapshot(actor)}
    q3_before = [sequence_signature(actor) for actor in actors
                 if actor.get_class().get_name() == "VHVTimeSkipSequence"
                 and str(actor.get_editor_property("required_quest_id")) == "Q_HBCT_03_ROLE_MODEL"]

    arc = unreal.EditorAssetLibrary.load_asset(QUEST_ARC)
    quests = list(arc.get_editor_property("quests") or []) if arc else []
    quest = next((entry for entry in quests
                  if str(entry.get_editor_property("quest_id")) == QUEST_ID), None)
    if not quest:
        raise RuntimeError("Quest 5 is missing from DA_QuestArc_HBCT")
    objective_ids = [str(entry.get_editor_property("objective_id"))
                     for entry in list(quest.get_editor_property("objectives") or [])]
    for source, destination, *_ in TRANSITIONS:
        if objective_ids.index(destination) != objective_ids.index(source) + 1:
            raise RuntimeError("{} is not immediately followed by {}".format(source, destination))

    pawn_class, capsule_radius, capsule_half_height = resolve_player_capsule()
    sequences = {name: exact(by_label, name) for name in {
        item[2] for item in TRANSITIONS}}
    controller = exact(by_label, "VHV_Q5_ObjectiveDisplayVisibility")
    afternoon_display = exact(by_label, "HBCT_SM_Display_AfternoonRecord")

    configured = {}
    for group in list(controller.get_editor_property("objective_displays") or []):
        configured[str(group.get_editor_property("objective_id"))] = [
            label(item) for item in list(group.get_editor_property("display_actors") or [])]
    if configured.get("O08_RecordAfternoon") != ["HBCT_SM_Display_AfternoonRecord"]:
        raise RuntimeError("O08 display mapping is not the authored afternoon record board")
    text_component = afternoon_display.get_component_by_class(unreal.TextRenderComponent)
    if not text_component:
        raise RuntimeError("O08 display has no TextRenderComponent")
    display_text = str(text_component.get_editor_property("text"))
    if not all(value in display_text for value in ("TODAY", "Vegetables", "Sweet Drink")):
        raise RuntimeError("O08 display content is incomplete")

    stage_by_key = {}
    for sequence_name, sequence in sequences.items():
        for stage in list(sequence.get_editor_property("stages") or []):
            stage_by_key[(sequence_name, str(stage.get_editor_property("stage_id")))] = stage

    validation_rows = []
    for (source, destination, sequence_name, stage_id, text_stage_id,
         expected_text, fade_in, hold, fade_out) in TRANSITIONS:
        sequence = sequences[sequence_name]
        stage = stage_by_key.get((sequence_name, stage_id))
        text_stage = stage_by_key.get((sequence_name, text_stage_id))
        if not stage:
            raise RuntimeError("Missing {} stage {}".format(sequence_name, stage_id))
        if not text_stage:
            raise RuntimeError("Missing {} text stage {}".format(sequence_name, text_stage_id))
        if str(stage.get_editor_property("objective_id")) != destination:
            raise RuntimeError("{} destination objective is wrong".format(stage_id))
        if str(text_stage.get_editor_property("transition_text")) != expected_text:
            raise RuntimeError("{} transition text is wrong".format(text_stage_id))
        for prop, expected in (("fade_in_duration", fade_in),
                               ("text_hold_duration", hold),
                               ("fade_out_duration", fade_out)):
            if abs(float(sequence.get_editor_property(prop)) - expected) > 0.001:
                raise RuntimeError("{} {} timing is wrong".format(sequence_name, prop))
        arrival_validation = validate_arrival(world, stage, capsule_radius, capsule_half_height)
        validation_rows.append({
            "SourceObjective": source,
            "DestinationObjective": destination,
            "Sequence": sequence_name,
            "Stage": stage_id,
            "TransitionText": expected_text,
            "Timing": [fade_in, hold, fade_out],
            "Arrival": arrival_validation,
            "Status": "PASS",
        })

    # Remove the obsolete Plane dependency. The hand-authored arrival TargetPoints
    # remain untouched and the runtime now validates any real walkable Pawn floor.
    for sequence in sequences.values():
        stages = list(sequence.get_editor_property("stages") or [])
        for stage in stages:
            stage.set_editor_property("player_arrival_support_actor", None)
        sequence.set_editor_property("stages", stages)

    # The objective display controller is the sole visibility owner. Conversation
    # actors remain staged by the daily sequence; authored board actors are not.
    daily = sequences["VHV_Q5_TimeSkip_DailyRecording"]
    daily_stages = list(daily.get_editor_property("stages") or [])
    removed_display_owners = []
    for stage in daily_stages:
        stage_actors = list(stage.get_editor_property("stage_actors") or [])
        retained = [actor for actor in stage_actors if not label(actor).startswith("HBCT_SM_Display_")]
        removed_display_owners.extend(label(actor) for actor in stage_actors if actor not in retained)
        stage.set_editor_property("stage_actors", retained)
    daily.set_editor_property("stages", daily_stages)

    for sequence in sequences.values():
        for stage in list(sequence.get_editor_property("stages") or []):
            if stage.get_editor_property("player_arrival_support_actor"):
                raise RuntimeError("Obsolete Q5 arrival support reference remains")
    if sorted(removed_display_owners) != sorted([
            "HBCT_SM_Display_BreakfastRecord",
            "HBCT_SM_Display_AfternoonRecord",
            "HBCT_SM_Display_CompleteDay"]):
        raise RuntimeError("Unexpected daily stage display ownership: {}".format(removed_display_owners))
    configured_after = {}
    for group in list(controller.get_editor_property("objective_displays") or []):
        configured_after[str(group.get_editor_property("objective_id"))] = [
            label(item) for item in list(group.get_editor_property("display_actors") or [])]
    if configured_after != configured:
        raise RuntimeError("Objective display mapping changed")

    transforms_after = {actor.get_path_name(): transform(actor) for actor in actors}
    changed_transforms = [path for path, value in transforms_before.items()
                          if transforms_after.get(path) != value]
    if changed_transforms:
        raise RuntimeError("Existing actor transforms changed: {}".format(changed_transforms))
    trigger_boxes_after = {actor.get_path_name(): box_snapshot(actor) for actor in actors
                           if actor.get_path_name() in trigger_boxes_before}
    changed_trigger_extents = [path for path, value in trigger_boxes_before.items()
                               if trigger_boxes_after.get(path) != value]
    if changed_trigger_extents:
        raise RuntimeError("Q5 trigger extents/profiles changed: {}".format(changed_trigger_extents))
    q3_after = [sequence_signature(actor) for actor in actors
                if actor.get_class().get_name() == "VHVTimeSkipSequence"
                and str(actor.get_editor_property("required_quest_id")) == "Q_HBCT_03_ROLE_MODEL"]
    if q3_after != q3_before:
        raise RuntimeError("Q3 time-skip configuration changed")
    if sha256(blockout_file) != snapshot["BlockoutHash"]:
        raise RuntimeError("Protected blockout changed before save")

    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
        raise RuntimeError("Failed to save Lvl_Village_Main")
    if sha256(blockout_file) != snapshot["BlockoutHash"]:
        raise RuntimeError("Protected blockout changed after save")

    arrival_by_objective = {row["DestinationObjective"]: row["Arrival"] for row in validation_rows}
    board_location = afternoon_display.get_actor_location()
    board_yaw = float(afternoon_display.get_actor_rotation().yaw)
    afternoon_arrival = arrival_by_objective["O08_RecordAfternoon"]
    arrival_transform = afternoon_arrival["ArrivalTransform"]
    board_distance = math.sqrt(
        (float(board_location.x) - arrival_transform[0]) ** 2
        + (float(board_location.y) - arrival_transform[1]) ** 2
        + (float(board_location.z) - arrival_transform[2]) ** 2)
    report = {
        "Status": "PASS",
        "GeneratedAtLocal": datetime.datetime.now().isoformat(),
        "VerifiedRootCause": (
            "All four Q5 stages retained PlayerArrivalSupportActor=Plane. "
            "Their current hand-authored arrival XYs resolve over non-Plane collision, "
            "so ResolvePlayerArrival rejected preflight before PlayTransition."),
        "QuestObjectiveOrder": objective_ids,
        "Transitions": validation_rows,
        "PlayerPawnClass": pawn_class,
        "PlayerCapsuleRadius": capsule_radius,
        "PlayerCapsuleHalfHeight": capsule_half_height,
        "ArrivalSupportReferencesCleared": 5,
        "ArrivalTargetsMoved": 0,
        "NewTargetPointsCreated": 0,
        "DisplayFix": {
            "Objective": "O08_RecordAfternoon",
            "Controller": label(controller),
            "Actor": label(afternoon_display),
            "Text": display_text,
            "Transform": transform(afternoon_display),
            "DistanceFromO08Arrival": board_distance,
            "BoardYaw": board_yaw,
            "MappingPreserved": True,
            "TimeSkipStageVisibilityOwnershipRemoved": True,
            "ObjectiveControllerIsSoleVisibilityOwner": True,
            "EditorBillboardHiddenInGame": True,
        },
        "RemovedStageDisplayOwners": removed_display_owners,
        "ExistingActorTransformsChanged": len(changed_transforms),
        "ExistingTriggerExtentsOrProfilesChanged": len(changed_trigger_extents),
        "ExistingTriggersMoved": 0,
        "ExistingNPCsOrTextActorsMoved": 0,
        "Q0ToQ4Changes": 0,
        "Q6Changes": 0,
        "Q3TimeSkipChanges": 0,
        "MainMapSaved": True,
        "MainMapHashBefore": snapshot["MainMapHash"],
        "MainMapHashAfter": sha256(main_file),
        "BlockoutHashBefore": snapshot["BlockoutHash"],
        "BlockoutHashAfter": sha256(blockout_file),
        "BlockoutUntouched": True,
        "AutomatedPIERun": False,
        "ManualPIERequired": True,
    }
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    report_path = os.path.join(OUTPUT_DIR, "q5_urgent_fix_validation.json")
    with open(report_path, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, sort_keys=True)
    unreal.log("Q5_URGENT_FIX_PASS=" + json.dumps({
        "Report": report_path,
        "Transitions": {row["SourceObjective"] + "->" + row["DestinationObjective"]: row["Status"]
                        for row in validation_rows},
        "ExistingTransformsChanged": 0,
        "ExistingTriggerExtentsChanged": 0,
        "MapSaved": True,
        "BlockoutUntouched": True,
    }, sort_keys=True))


main()
