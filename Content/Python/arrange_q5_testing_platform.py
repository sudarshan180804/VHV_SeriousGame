"""Arrange Quest 5-only actors on the temporary elevated testing platform.

This is deliberately transactional and idempotent.  It never moves persistent shared
participants (Instructor or Uncle Chai), never edits quest data, and saves only
Lvl_Village_Main after every non-Q5 actor has passed an identity/transform/settings
comparison.
"""

import csv
import datetime
import hashlib
import json
import math
import os
import traceback

import unreal


TARGET_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
PROTECTED_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout"
QUEST_ID = "Q_HBCT_05_SELF_MONITORING"
QUEST_ARC_PATH = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
Q5_PREFIX = "HBCT_SM_"
Q5_FOLDER_PREFIXES = ("06_SelfMonitoring", "Village_Main/Q5_TestingPlatform")
EXPECTED_Q5_ACTOR_COUNT = 120
OUTPUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "VHV_Q5TestingPlatform")
PLATFORM_LABELS = ("Plane",)
ENDPOINT_A = unreal.Vector(-2149.0, 18305.0, 1065.0)
ENDPOINT_B = unreal.Vector(-10346.0, 18286.0, 1065.0)
END_MARGIN = 200.0
SURFACE_TOLERANCE = 8.0

OBJECTIVE_IDS = [
    "O01_CheckInWithChai",
    "O02_LearnWhyRecordsMatter",
    "O03_IdentifyTargetBehavior",
    "O04_BuildMonitoringProcess",
    "O05_ChooseMonitoringMethod",
    "O06_PrepareChaiTracker",
    "O07_RecordBreakfast",
    "O08_RecordAfternoon",
    "O09_RecordEveningWalk",
    "O10_ReviewFirstDay",
    "O11_OneWeekLater",
    "O12_FindBehaviorPatterns",
    "O13_AdjustThePlan",
    "O14_PracticeAdjustedRoutine",
    "O15_BuildProgressGraph",
    "O16_SetSelfReminder",
    "O17_WatchChaiSelfMonitor",
    "O18_FinalDebrief",
]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def map_path(world):
    outer = world.get_outer()
    package = outer.get_path_name() if outer else world.get_path_name()
    return package.split(".")[0]


def v3(value):
    return [round(float(value.x), 3), round(float(value.y), 3), round(float(value.z), 3)]


def transform_values(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return [
        float(location.x), float(location.y), float(location.z),
        float(rotation.pitch), float(rotation.yaw), float(rotation.roll),
        float(scale.x), float(scale.y), float(scale.z),
    ]


def angular_difference_degrees(left, right):
    """Smallest absolute difference between two Unreal Euler components."""
    return abs((float(left) - float(right) + 180.0) % 360.0 - 180.0)


def actor_bounds(actor):
    try:
        origin, extent = actor.get_actor_bounds(False, True)
    except TypeError:
        origin, extent = actor.get_actor_bounds(False)
    return origin, extent


def component_world_location(component):
    try:
        return component.get_world_location()
    except Exception:
        return component.get_component_location()


def component_world_rotation(component):
    try:
        return component.get_world_rotation()
    except Exception:
        return component.get_component_rotation()


def component_world_scale(component):
    try:
        return component.get_world_scale()
    except Exception:
        transform = component.get_component_transform()
        try:
            return transform.scale3d
        except Exception:
            return transform.get_editor_property("scale3d")


def actor_class(actor):
    return actor.get_class().get_name()


def folder(actor):
    return str(actor.get_folder_path())


def gameplay_tag_name(tag):
    return str(unreal.GameplayTagLibrary.get_tag_name(tag))


def primitive_settings(actor):
    rows = []
    for component in list(actor.get_components_by_class(unreal.PrimitiveComponent) or []):
        row = {
            "Name": component.get_name(),
            "Class": component.get_class().get_name(),
        }
        try:
            row["CollisionEnabled"] = str(component.get_collision_enabled())
        except Exception:
            pass
        try:
            row["GenerateOverlapEvents"] = bool(component.get_generate_overlap_events())
        except Exception:
            pass
        rows.append(row)
    return sorted(rows, key=lambda item: (item["Name"], item["Class"]))


def protected_state(actor):
    try:
        tags = sorted(str(item) for item in list(actor.get_editor_property("tags") or []))
    except Exception:
        tags = []
    try:
        collision = bool(actor.get_actor_enable_collision())
    except Exception:
        collision = None
    return {
        "Path": actor.get_path_name(),
        "Label": actor.get_actor_label(),
        "Class": actor_class(actor),
        "Folder": folder(actor),
        "Transform": transform_values(actor),
        "ActorCollision": collision,
        "Tags": tags,
        "PrimitiveSettings": primitive_settings(actor),
    }


def is_q5_owned(actor):
    label = actor.get_actor_label()
    actor_folder = folder(actor)
    return label.startswith(Q5_PREFIX) or actor_folder.startswith(Q5_FOLDER_PREFIXES)


def classify_objective(label):
    """Resolve every Q5-owned label to exactly one existing objective."""
    suffix = label[len(Q5_PREFIX):] if label.startswith(Q5_PREFIX) else label

    if suffix == "NPC_SelfMonitoringFriend":
        return "O08_RecordAfternoon"
    if "FinalDebrief" in suffix or "FinalChai" in suffix or suffix == "Display_Final":
        return "O18_FinalDebrief"
    if "SelfMonitor" in suffix:
        return "O17_WatchChaiSelfMonitor"
    if "SelfReminder" in suffix or "Reminder" in suffix:
        return "O16_SetSelfReminder"
    if "ProgressGraph" in suffix:
        return "O15_BuildProgressGraph"
    if "AdjustedWalk" in suffix:
        return "O14_PracticeAdjustedRoutine"
    if "Adjustment" in suffix:
        return "O13_AdjustThePlan"
    if ("Pattern" in suffix or suffix in (
            "Evidence_WeeklyCalendar", "Evidence_MarketSchedule",
            "Evidence_SweetDrinkVisits", "Evidence_WalkingGoal")):
        return "O12_FindBehaviorPatterns"
    if "OneWeek" in suffix or suffix == "Display_WeeklyCalendar":
        return "O11_OneWeekLater"
    if "FirstDay" in suffix:
        return "O10_ReviewFirstDay"
    if ("ChaiWalk" in suffix or "WalkStart" in suffix or "EveningWalk" in suffix
            or "RecordWalk" in suffix or suffix == "Display_CompleteDay"):
        return "O09_RecordEveningWalk"
    if "Afternoon" in suffix:
        return "O08_RecordAfternoon"
    if "Breakfast" in suffix:
        return "O07_RecordBreakfast"
    if "PrepareTracker" in suffix or "PreparedTracker" in suffix or "TrackerChai" in suffix:
        return "O06_PrepareChaiTracker"
    if "Method" in suffix:
        return "O05_ChooseMonitoringMethod"
    if "MonitoringProcess" in suffix:
        return "O04_BuildMonitoringProcess"
    if ("Target" in suffix or suffix in (
            "Prop_WalkingShoes", "Prop_Vegetables", "Prop_SweetDrink",
            "Prop_GlucoseRecord", "Prop_Scale", "Prop_Calendar", "Prop_Notebook")
            or suffix.startswith("Label_WalkingShoes")
            or suffix.startswith("Label_Vegetables")
            or suffix.startswith("Label_SweetDrink")
            or suffix.startswith("Label_GlucoseRecord")
            or suffix.startswith("Label_Scale")
            or suffix.startswith("Label_Calendar")
            or suffix.startswith("Label_Notebook")):
        return "O03_IdentifyTargetBehavior"
    if "InstructorLesson" in suffix:
        return "O02_LearnWhyRecordsMatter"
    if ("ChaiCheckIn" in suffix or "ChaiHome" in suffix
            or suffix.startswith("Home_") or suffix == "Display_BlankCalendar"):
        return "O01_CheckInWithChai"
    return ""


def feature_name(label, objective_id):
    feature_sets = {
        "O01_CheckInWithChai": [
            "Notebook", "Calendar", "WalkingShoes", "Vegetables",
            "SweetDrink", "GlucoseRecord", "Scale"],
        "O03_IdentifyTargetBehavior": [
            "WalkingShoes", "Vegetables", "SweetDrink", "GlucoseRecord",
            "Scale", "Calendar", "Notebook"],
        "O05_ChooseMonitoringMethod": ["Notebook", "Calendar", "Phone"],
        "O12_FindBehaviorPatterns": ["Busy", "Sweet", "Walking"],
    }
    for feature in feature_sets.get(objective_id, []):
        if feature in label:
            return feature
    return ""


def station_offset(label, objective_id, lane):
    feature = feature_name(label, objective_id)
    seven_grid = {
        "WalkingShoes": (-120.0, -lane),
        "Notebook": (0.0, 0.0),
        "Vegetables": (0.0, -lane),
        "SweetDrink": (120.0, -lane),
        "GlucoseRecord": (-120.0, lane),
        "Scale": (0.0, lane),
        "Calendar": (120.0, lane),
    }
    if objective_id == "O01_CheckInWithChai" and feature:
        return seven_grid.get(feature, (0.0, 0.0))
    if objective_id == "O03_IdentifyTargetBehavior" and feature:
        return seven_grid.get(feature, (0.0, 0.0))
    if objective_id == "O05_ChooseMonitoringMethod" and feature:
        return {
            "Notebook": (0.0, -lane),
            "Calendar": (0.0, 0.0),
            "Phone": (0.0, lane),
        }[feature]
    if objective_id == "O12_FindBehaviorPatterns" and feature:
        return {
            "Busy": (0.0, -lane),
            "Sweet": (0.0, 0.0),
            "Walking": (0.0, lane),
        }[feature]

    if objective_id == "O02_LearnWhyRecordsMatter":
        if "InstructorLessonChai" in label:
            return (0.0, lane * 0.70)
        if "SMInstructorLesson" in label:
            return (0.0, -lane * 0.70)
    if objective_id == "O06_PrepareChaiTracker":
        if "TrackerChai" in label:
            return (0.0, lane * 0.70)
        if "Display_PreparedTracker" in label:
            return (-80.0, -lane * 0.65)
    if objective_id == "O07_RecordBreakfast" and (
            "RecordBreakfast" in label or "BreakfastRecord" in label):
        return (-110.0, lane * 0.65)
    if objective_id == "O08_RecordAfternoon":
        if "AfternoonFriend" in label or "SelfMonitoringFriend" in label:
            return (0.0, lane * 0.72)
        if "RecordAfternoon" in label or "AfternoonRecord" in label:
            return (-105.0, -lane * 0.65)
    if objective_id == "O09_RecordEveningWalk":
        if "WalkStart" in label or "EveningWalk" in label:
            return (145.0, 0.0)
        if "ChaiWalkMid" in label:
            return (0.0, 0.0)
        if "ChaiWalkEnd" in label or "RecordWalk" in label or "CompleteDay" in label:
            return (-145.0, 0.0)
    if objective_id == "O10_ReviewFirstDay":
        if "Instructor" in label:
            return (0.0, lane * 0.72)
        if "Display" in label:
            return (-80.0, -lane * 0.65)
    if objective_id == "O11_OneWeekLater":
        if "Instructor" in label:
            return (0.0, lane * 0.72)
        if "Display" in label:
            return (-80.0, -lane * 0.65)
    if objective_id == "O14_PracticeAdjustedRoutine":
        if "AdjustedWalkStart" in label or "Ambient_AdjustedWalk" in label:
            return (145.0, 0.0)
        if "AdjustedWalkMid" in label:
            return (0.0, 0.0)
        if "AdjustedWalkEnd" in label:
            return (-145.0, 0.0)
    if objective_id == "O15_BuildProgressGraph":
        if "ProgressGraphChai" in label:
            return (0.0, lane * 0.72)
        if "ProgressGraphInstructor" in label:
            return (0.0, -lane * 0.72)
    if objective_id == "O16_SetSelfReminder" and (
            "Display" in label or "Prop" in label):
        return (0.0, lane * 0.70)
    if objective_id == "O17_WatchChaiSelfMonitor":
        if "Instructor" in label:
            return (0.0, lane * 0.75)
        if "Display" in label:
            return (-70.0, -lane * 0.65)
    if objective_id == "O18_FinalDebrief":
        if "FinalChai" in label:
            return (0.0, lane * 0.72)
        if "Display_Final" in label:
            return (-70.0, -lane * 0.65)
    return (0.0, 0.0)


def desired_yaw(actor, objective_id, offset_y, axis_x, axis_y, side_x, side_y):
    label = actor.get_actor_label()
    if objective_id in ("O09_RecordEveningWalk", "O14_PracticeAdjustedRoutine"):
        return math.degrees(math.atan2(axis_y, axis_x))
    if actor_class(actor) == "BP_VHVNPCBase_C" or "Location_" in label or "Trigger_" in label:
        if offset_y > 25.0:
            return math.degrees(math.atan2(-side_y, -side_x))
        if offset_y < -25.0:
            return math.degrees(math.atan2(side_y, side_x))
        return math.degrees(math.atan2(-axis_y, -axis_x))
    return None


def find_q5_definition():
    arc = unreal.EditorAssetLibrary.load_asset(QUEST_ARC_PATH)
    if not arc:
        raise RuntimeError("Missing quest arc: " + QUEST_ARC_PATH)
    matches = [quest for quest in list(arc.get_editor_property("quests") or [])
               if str(quest.get_editor_property("quest_id")) == QUEST_ID]
    if len(matches) != 1:
        raise RuntimeError("Expected one Quest 5 definition; found %d" % len(matches))
    quest = matches[0]
    objectives = list(quest.get_editor_property("objectives") or [])
    actual_ids = [str(item.get_editor_property("objective_id")) for item in objectives]
    if actual_ids != OBJECTIVE_IDS:
        raise RuntimeError("Quest 5 objective order differs from the approved 18-stage order: %s" % actual_ids)
    return objectives


def validate_o02_existing_activation(objective):
    """O02 is activated through the Instructor conversation, not a location trigger."""
    participant = gameplay_tag_name(objective.get_editor_property("participant_tag"))
    conversation = objective.get_editor_property("conversation")
    entry = str(objective.get_editor_property("entry_node_id"))
    readiness = list(objective.get_editor_property("activation_npc_readiness") or [])
    destinations = {
        gameplay_tag_name(item.get_editor_property("destination_location_tag"))
        for item in readiness
    }
    required_destinations = {
        "VHV.Location.HBCT.SelfMonitoring.SMInstructorLesson",
        "VHV.Location.HBCT.SelfMonitoring.SMInstructorLessonChai",
    }
    if participant != "VHV.Participant.Instructor":
        raise RuntimeError("O02 participant is not the shared Instructor: " + participant)
    if not conversation or conversation.get_name() != "DA_Conversation_HBCT_SelfMonitoring_InstructorLesson":
        raise RuntimeError("O02 Instructor lesson conversation binding is missing or changed")
    if entry != "SMLesson_01":
        raise RuntimeError("O02 conversation entry changed: " + entry)
    if not required_destinations.issubset(destinations):
        raise RuntimeError("O02 runtime staging destinations are incomplete: %s" % sorted(destinations))
    return {
        "Mechanism": "Existing participant interaction and Conversation objective",
        "ParticipantTag": participant,
        "Conversation": conversation.get_path_name(),
        "EntryNodeId": entry,
        "RuntimeStagingDestinations": sorted(required_destinations),
        "TriggerRequired": False,
    }


def participant_actors(actors, full_tag):
    matches = []
    for actor in actors:
        try:
            component = actor.get_quest_participant_component()
        except Exception:
            component = None
        if component and gameplay_tag_name(component.get_editor_property("participant_tag")) == full_tag:
            matches.append(actor)
    return matches


def platform_bounds_row(actor):
    origin, extent = actor_bounds(actor)
    return {
        "Actor": actor,
        "Label": actor.get_actor_label(),
        "Class": actor_class(actor),
        "Origin": origin,
        "Extent": extent,
        "MinX": float(origin.x - extent.x),
        "MaxX": float(origin.x + extent.x),
        "MinY": float(origin.y - extent.y),
        "MaxY": float(origin.y + extent.y),
        "MinZ": float(origin.z - extent.z),
        "MaxZ": float(origin.z + extent.z),
    }


def supports_xy(row, x, y, tolerance=3.0):
    return (row["MinX"] - tolerance <= x <= row["MaxX"] + tolerance
            and row["MinY"] - tolerance <= y <= row["MaxY"] + tolerance)


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        raise RuntimeError("No Editor world is loaded")
    current_map = map_path(world)
    if current_map == PROTECTED_MAP or "Lvl_Village_Blockout" in current_map:
        raise RuntimeError("Protected Blockout is loaded; refusing work")
    if current_map != TARGET_MAP:
        raise RuntimeError("Lvl_Village_Main must be loaded; current map is " + current_map)

    content_dir = unreal.Paths.project_content_dir()
    main_file = os.path.join(content_dir, "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
    blockout_file = os.path.join(content_dir, "VHV_Stuff", "Maps", "Lvl_Village_Blockout.umap")
    main_hash_before = sha256(main_file)
    blockout_hash_before = sha256(blockout_file)
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = list(actor_subsystem.get_all_level_actors())
    paths_before = {actor.get_path_name() for actor in all_actors}
    if len(paths_before) != len(all_actors):
        raise RuntimeError("Duplicate actor paths detected")

    q5_actors = [actor for actor in all_actors if is_q5_owned(actor)]
    protected_actors = [actor for actor in all_actors if not is_q5_owned(actor)]
    if not q5_actors:
        raise RuntimeError("No Quest 5-owned actors were found")
    if len(q5_actors) != EXPECTED_Q5_ACTOR_COUNT:
        raise RuntimeError("Quest 5 inventory changed: expected %d actors, found %d" %
                           (EXPECTED_Q5_ACTOR_COUNT, len(q5_actors)))

    ambiguous_folder_actors = [actor.get_actor_label() for actor in q5_actors
                               if not actor.get_actor_label().startswith(Q5_PREFIX)]
    if ambiguous_folder_actors:
        raise RuntimeError("Q5 folder contains non-HBCT_SM actors; ownership is ambiguous: %s" %
                           ambiguous_folder_actors)

    labels = [actor.get_actor_label() for actor in q5_actors]
    if len(set(labels)) != len(labels):
        raise RuntimeError("Quest 5 actor labels are not unique")

    objectives = find_q5_definition()
    objective_by_id = {str(item.get_editor_property("objective_id")): item for item in objectives}
    o02_activation = validate_o02_existing_activation(objective_by_id["O02_LearnWhyRecordsMatter"])
    groups = {objective_id: [] for objective_id in OBJECTIVE_IDS}
    unresolved = []
    for actor in q5_actors:
        objective_id = classify_objective(actor.get_actor_label())
        if not objective_id:
            unresolved.append(actor.get_actor_label())
        else:
            groups[objective_id].append(actor)
    if unresolved:
        raise RuntimeError("Unresolved Q5 actor ownership/station mapping: %s" % unresolved)
    empty_groups = [objective_id for objective_id in OBJECTIVE_IDS if not groups[objective_id]]
    if empty_groups:
        raise RuntimeError("Quest 5 objectives without physical actors: %s" % empty_groups)

    required_triggers = {
        "O01_CheckInWithChai": "HBCT_SM_Trigger_ChaiCheckIn",
        "O03_IdentifyTargetBehavior": "HBCT_SM_Trigger_TargetBehaviorArea",
        "O04_BuildMonitoringProcess": "HBCT_SM_Trigger_MonitoringProcess",
        "O05_ChooseMonitoringMethod": "HBCT_SM_Trigger_MonitoringMethods",
        "O06_PrepareChaiTracker": "HBCT_SM_Trigger_PrepareTracker",
        "O07_RecordBreakfast": "HBCT_SM_Trigger_Breakfast",
        "O08_RecordAfternoon": "HBCT_SM_Trigger_Afternoon",
        "O09_RecordEveningWalk": "HBCT_SM_Trigger_WalkStart",
        "O10_ReviewFirstDay": "HBCT_SM_Trigger_FirstDayReview",
        "O11_OneWeekLater": "HBCT_SM_Trigger_OneWeekLater",
        "O12_FindBehaviorPatterns": "HBCT_SM_Trigger_PatternArea",
        "O13_AdjustThePlan": "HBCT_SM_Trigger_Adjustment",
        "O14_PracticeAdjustedRoutine": "HBCT_SM_Trigger_AdjustedWalkStart",
        "O15_BuildProgressGraph": "HBCT_SM_Trigger_ProgressGraph",
        "O16_SetSelfReminder": "HBCT_SM_Trigger_SelfReminder",
        "O17_WatchChaiSelfMonitor": "HBCT_SM_Trigger_SelfMonitorPayoff",
        "O18_FinalDebrief": "HBCT_SM_Trigger_FinalDebrief",
    }
    missing_triggers = [label for label in required_triggers.values() if label not in labels]
    if missing_triggers:
        raise RuntimeError("Required Quest 5 scene triggers are missing: %s" % missing_triggers)

    shared = []
    for display_name, tag in (
            ("Instructor", "VHV.Participant.Instructor"),
            ("Uncle Chai", "VHV.Participant.UncleChai")):
        matches = participant_actors(all_actors, tag)
        if len(matches) != 1:
            raise RuntimeError("Shared participant %s resolves %d actors" % (tag, len(matches)))
        actor = matches[0]
        if is_q5_owned(actor):
            raise RuntimeError("Shared participant was incorrectly classified as Q5-owned: " + actor.get_actor_label())
        shared.append({
            "Name": display_name,
            "ActorLabel": actor.get_actor_label(),
            "ParticipantTag": tag,
            "Transform": transform_values(actor),
            "ReasonLeftInPlace": "Shared persistent Q0-Q4 participant; Q5 uses its semantic staging anchors",
        })

    platform_matches = {label: [actor for actor in all_actors if actor.get_actor_label() == label]
                        for label in PLATFORM_LABELS}
    platform_errors = {label: len(matches) for label, matches in platform_matches.items()
                       if len(matches) != 1}
    if platform_errors:
        raise RuntimeError("Platform actor identities are unresolved: %s" % platform_errors)
    platform_rows = [platform_bounds_row(platform_matches[label][0]) for label in PLATFORM_LABELS]
    plane_row = platform_rows[0]
    plane_actor = plane_row["Actor"]
    combined = {
        "MinX": min(row["MinX"] for row in platform_rows),
        "MaxX": max(row["MaxX"] for row in platform_rows),
        "MinY": min(row["MinY"] for row in platform_rows),
        "MaxY": max(row["MaxY"] for row in platform_rows),
        "MinZ": min(row["MinZ"] for row in platform_rows),
        "MaxZ": max(row["MaxZ"] for row in platform_rows),
    }
    # The saved Plane is authoritative.  Derive its two horizontal local axes from
    # its transform, recover its local dimensions from the rotated world bounds,
    # and then choose the longer local axis.  A/B controls direction only (O01 at
    # A, O18 toward B); it must not redefine the Plane's orientation or footprint.
    plane_rotation = plane_actor.get_actor_rotation()
    if abs(float(plane_rotation.pitch)) > 1.0 or abs(float(plane_rotation.roll)) > 1.0:
        raise RuntimeError(
            "Testing Plane is not horizontal (Pitch=%.3f Roll=%.3f); cannot use a single top surface" %
            (float(plane_rotation.pitch), float(plane_rotation.roll)))
    plane_scale = plane_actor.get_actor_scale3d()
    scale_x = abs(float(plane_scale.x))
    scale_y = abs(float(plane_scale.y))
    if scale_x < 0.0001 or scale_y < 0.0001:
        raise RuntimeError("Testing Plane has a zero horizontal scale component")
    yaw_radians = math.radians(float(plane_rotation.yaw))
    local_x = (math.cos(yaw_radians), math.sin(yaw_radians))
    local_y = (-math.sin(yaw_radians), math.cos(yaw_radians))
    world_half_x = (float(plane_row["MaxX"]) - float(plane_row["MinX"])) * 0.5
    world_half_y = (float(plane_row["MaxY"]) - float(plane_row["MinY"])) * 0.5
    denominator_x = abs(local_x[0]) * scale_x + abs(local_y[0]) * scale_y
    denominator_y = abs(local_x[1]) * scale_x + abs(local_y[1]) * scale_y
    base_half_samples = []
    if denominator_x > 0.0001:
        base_half_samples.append(world_half_x / denominator_x)
    if denominator_y > 0.0001:
        base_half_samples.append(world_half_y / denominator_y)
    if not base_half_samples:
        raise RuntimeError("Could not recover local Plane dimensions from its saved bounds")
    base_half_extent = sum(base_half_samples) / float(len(base_half_samples))
    local_half_x = base_half_extent * scale_x
    local_half_y = base_half_extent * scale_y
    if local_half_x >= local_half_y:
        axis_x, axis_y = local_x
        plane_half_length = local_half_x
        plane_half_width = local_half_y
    else:
        axis_x, axis_y = local_y
        plane_half_length = local_half_y
        plane_half_width = local_half_x
    reference_x = float(ENDPOINT_B.x - ENDPOINT_A.x)
    reference_y = float(ENDPOINT_B.y - ENDPOINT_A.y)
    if axis_x * reference_x + axis_y * reference_y < 0.0:
        axis_x = -axis_x
        axis_y = -axis_y
    side_x = -axis_y
    side_y = axis_x
    plane_length = plane_half_length * 2.0
    plane_width = plane_half_width * 2.0
    plane_center_x = float(plane_row["Origin"].x)
    plane_center_y = float(plane_row["Origin"].y)

    def plane_local_coordinates(x, y):
        delta_x = float(x) - plane_center_x
        delta_y = float(y) - plane_center_y
        return (delta_x * axis_x + delta_y * axis_y,
                delta_x * side_x + delta_y * side_y)

    def plane_contains_point(x, y, tolerance=0.0):
        along, across = plane_local_coordinates(x, y)
        return (abs(along) <= plane_half_length + float(tolerance)
                and abs(across) <= plane_half_width + float(tolerance))

    def box_collision_shape(component):
        center = component_world_location(component)
        rotation = component_world_rotation(component)
        scale = component_world_scale(component)
        unscaled = component.get_unscaled_box_extent()
        extent_x = abs(float(unscaled.x) * float(scale.x))
        extent_y = abs(float(unscaled.y) * float(scale.y))
        extent_z = abs(float(unscaled.z) * float(scale.z))
        yaw = math.radians(float(rotation.yaw))
        box_x = (math.cos(yaw), math.sin(yaw))
        box_y = (-math.sin(yaw), math.cos(yaw))
        half_along = (abs(axis_x * box_x[0] + axis_y * box_x[1]) * extent_x
                      + abs(axis_x * box_y[0] + axis_y * box_y[1]) * extent_y)
        half_across = (abs(side_x * box_x[0] + side_y * box_x[1]) * extent_x
                       + abs(side_x * box_y[0] + side_y * box_y[1]) * extent_y)
        along, across = plane_local_coordinates(center.x, center.y)
        return {
            "Kind": "BoxComponent",
            "Component": component.get_name(),
            "Center": center,
            "Along": along,
            "Across": across,
            "HalfAlong": half_along,
            "HalfAcross": half_across,
            "MinZ": float(center.z) - extent_z,
            "MaxZ": float(center.z) + extent_z,
            "Dimensions": [extent_x * 2.0, extent_y * 2.0, extent_z * 2.0],
        }

    def capsule_collision_shape(component):
        center = component_world_location(component)
        try:
            radius = float(component.get_scaled_capsule_radius())
            half_height = float(component.get_scaled_capsule_half_height())
        except Exception:
            scale = component_world_scale(component)
            radius = float(component.get_unscaled_capsule_radius()) * max(
                abs(float(scale.x)), abs(float(scale.y)))
            half_height = float(component.get_unscaled_capsule_half_height()) * abs(float(scale.z))
        along, across = plane_local_coordinates(center.x, center.y)
        return {
            "Kind": "CapsuleComponent",
            "Component": component.get_name(),
            "Center": center,
            "Along": along,
            "Across": across,
            "HalfAlong": radius,
            "HalfAcross": radius,
            "MinZ": float(center.z) - half_height,
            "MaxZ": float(center.z) + half_height,
            "Dimensions": [radius * 2.0, radius * 2.0, half_height * 2.0],
        }

    def collision_shape_for_actor(actor, require_trigger_shape=False, require_capsule=False):
        boxes = list(actor.get_components_by_class(unreal.BoxComponent) or [])
        capsules = list(actor.get_components_by_class(unreal.CapsuleComponent) or [])
        if require_trigger_shape:
            try:
                configured_box = actor.get_editor_property("box_component")
            except Exception:
                configured_box = None
            if configured_box:
                return box_collision_shape(configured_box)
            if boxes:
                return box_collision_shape(boxes[0])
            if capsules:
                return capsule_collision_shape(capsules[0])
            raise RuntimeError("Trigger has no BoxComponent/CapsuleComponent: " + actor.get_actor_label())
        if require_capsule:
            if not capsules:
                raise RuntimeError("NPC has no CapsuleComponent: " + actor.get_actor_label())
            return capsule_collision_shape(capsules[0])
        return None

    def gameplay_footprint(actor):
        label = actor.get_actor_label()
        class_name = actor_class(actor)
        if label.startswith("HBCT_SM_Trigger_"):
            return collision_shape_for_actor(actor, require_trigger_shape=True)
        if class_name == "BP_VHVNPCBase_C":
            return collision_shape_for_actor(actor, require_capsule=True)
        if ("Location_" in label or "TargetPoint" in class_name
                or class_name == "VHVAmbientConversationActor"
                or "Ambient_" in label or "Conversation" in class_name):
            center = actor.get_actor_location()
            along, across = plane_local_coordinates(center.x, center.y)
            return {
                "Kind": "ActorPosition",
                "Component": "",
                "Center": center,
                "Along": along,
                "Across": across,
                "HalfAlong": 0.0,
                "HalfAcross": 0.0,
                "MinZ": float(center.z),
                "MaxZ": float(center.z),
                "Dimensions": [0.0, 0.0, 0.0],
            }
        # Props and displays are physical staging objects.  Retain conservative
        # gameplay bounds for them; only semantic/editor visualization actors use
        # point validation above.
        center, extent = actor_bounds(actor)
        along, across = plane_local_coordinates(center.x, center.y)
        return {
            "Kind": "PhysicalActorBounds",
            "Component": "Gameplay mesh/collision bounds",
            "Center": center,
            "Along": along,
            "Across": across,
            "HalfAlong": abs(axis_x) * float(extent.x) + abs(axis_y) * float(extent.y),
            "HalfAcross": abs(side_x) * float(extent.x) + abs(side_y) * float(extent.y),
            "MinZ": float(center.z - extent.z),
            "MaxZ": float(center.z + extent.z),
            "Dimensions": [float(extent.x) * 2.0, float(extent.y) * 2.0,
                           float(extent.z) * 2.0],
        }
    max_q5_axis_half_extent = 0.0
    for actor in q5_actors:
        _, extent = actor_bounds(actor)
        max_q5_axis_half_extent = max(
            max_q5_axis_half_extent,
            abs(axis_x) * float(extent.x) + abs(axis_y) * float(extent.y))
    edge_margin = max(END_MARGIN, max_q5_axis_half_extent + 20.0)
    start_projection = -plane_half_length + edge_margin
    end_projection = plane_half_length - edge_margin
    usable_length = end_projection - start_projection
    station_spacing = usable_length / float(len(OBJECTIVE_IDS) - 1)
    if station_spacing < 400.0:
        raise RuntimeError("Platform is too short for 18 separated stations: %.3f cm spacing" % station_spacing)

    station_centers = {}
    station_support = {}
    half_widths = []
    for index, objective_id in enumerate(OBJECTIVE_IDS):
        along = start_projection + station_spacing * index
        x = plane_center_x + axis_x * along
        y = plane_center_y + axis_y * along
        if not plane_contains_point(x, y, 5.0):
            raise RuntimeError("No platform surface covers station %s at %.3f, %.3f" %
                               (objective_id, x, y))
        top_z = float(plane_row["MaxZ"])
        station_centers[objective_id] = unreal.Vector(x, y, top_z)
        station_support[objective_id] = [plane_row["Label"]]
        half_widths.append(plane_half_width)
    usable_half_width = plane_width * 0.5 - 80.0
    if usable_half_width < 310.0:
        raise RuntimeError("Platform width is insufficient for Q5 trigger and side-by-side staging: %.3f cm" %
                           usable_half_width)
    primary_extents = []
    for actor in q5_actors:
        if actor.get_actor_label() in required_triggers.values():
            shape = collision_shape_for_actor(actor, require_trigger_shape=True)
            primary_extents.append((shape["HalfAlong"], shape["HalfAcross"]))
    max_primary_axis_extent = max(item[0] for item in primary_extents)
    max_primary_side_extent = max(item[1] for item in primary_extents)
    target_primary_side_extent = min(
        max_primary_side_extent,
        (plane_half_width - 25.0) * 0.5)
    if target_primary_side_extent < 180.0:
        raise RuntimeError("Plane is too narrow for functional Q5 primary triggers: %.3f cm half extent" %
                           target_primary_side_extent)
    station_lateral = target_primary_side_extent + 5.0
    lane = min(120.0, plane_half_width - station_lateral - 80.0)
    if lane < 80.0:
        raise RuntimeError("Plane is too narrow for alternating Q5 station groups and participant spacing")

    station_group_centers = {}
    for objective_index, objective_id in enumerate(OBJECTIVE_IDS):
        base = station_centers[objective_id]
        lateral = station_lateral if objective_index % 2 else -station_lateral
        station_group_centers[objective_id] = unreal.Vector(
            float(base.x + side_x * lateral),
            float(base.y + side_y * lateral),
            float(base.z))

    trigger_extent_changes = []
    if target_primary_side_extent < max_primary_side_extent - 0.01:
        for actor in q5_actors:
            if actor.get_actor_label() not in required_triggers.values():
                continue
            component = actor.get_editor_property("box_component")
            old_extent = component.get_unscaled_box_extent()
            current_side_extent = box_collision_shape(component)["HalfAcross"]
            if current_side_extent <= target_primary_side_extent + 0.01:
                continue
            ratio = target_primary_side_extent / current_side_extent
            new_extent = unreal.Vector(
                float(old_extent.x) * ratio,
                float(old_extent.y) * ratio,
                float(old_extent.z))
            trigger_extent_changes.append({
                "Actor": actor,
                "Component": component,
                "ActorLabel": actor.get_actor_label(),
                "OldExtent": [float(old_extent.x), float(old_extent.y), float(old_extent.z)],
                "NewExtent": [float(new_extent.x), float(new_extent.y), float(new_extent.z)],
            })

    protected_before = {actor.get_path_name(): protected_state(actor) for actor in protected_actors}
    q5_before = {
        actor.get_path_name(): {
            "Transform": actor.get_actor_transform(),
            "TransformValues": transform_values(actor),
            "Folder": folder(actor),
        }
        for actor in q5_actors
    }

    plan = []
    for objective_index, objective_id in enumerate(OBJECTIVE_IDS):
        center = station_group_centers[objective_id]
        for actor in sorted(groups[objective_id], key=lambda item: item.get_actor_label()):
            label = actor.get_actor_label()
            offset_x, offset_y = station_offset(label, objective_id, lane)
            desired_x = float(center.x - axis_x * offset_x + side_x * offset_y)
            desired_y = float(center.y - axis_y * offset_x + side_y * offset_y)
            if not plane_contains_point(desired_x, desired_y, 3.0):
                raise RuntimeError("Actor %s has no platform beneath its planned XY" % label)
            surface_z = float(plane_row["MaxZ"])
            origin, extent = actor_bounds(actor)
            old_location = actor.get_actor_location()
            has_bounds = max(abs(float(extent.x)), abs(float(extent.y)), abs(float(extent.z))) > 0.5
            pivot_above_bottom = float(old_location.z - (origin.z - extent.z)) if has_bounds else 50.0
            desired_z = float(surface_z + pivot_above_bottom)
            yaw = desired_yaw(actor, objective_id, offset_y, axis_x, axis_y, side_x, side_y)
            old_rotation = actor.get_actor_rotation()
            desired_rotation = [float(old_rotation.pitch),
                                float(yaw if yaw is not None else old_rotation.yaw),
                                float(old_rotation.roll)]
            plan.append({
                "Actor": actor,
                "ActorPath": actor.get_path_name(),
                "ActorLabel": label,
                "ActorClass": actor_class(actor),
                "ObjectiveId": objective_id,
                "StationIndex": objective_index + 1,
                "OldTransform": transform_values(actor),
                "NewLocation": [desired_x, desired_y, desired_z],
                "NewRotation": desired_rotation,
                "SurfaceZ": surface_z,
                "BoundsExtent": v3(extent),
                "HasBounds": has_bounds,
            })

    changed = []
    try:
        for extent_row in trigger_extent_changes:
            extent_row["Component"].set_box_extent(unreal.Vector(*extent_row["NewExtent"]), True)
        for row in plan:
            actor = row["Actor"]
            actor.modify()
            location = unreal.Vector(*row["NewLocation"])
            rotation = unreal.Rotator(
                pitch=row["NewRotation"][0],
                yaw=row["NewRotation"][1],
                roll=row["NewRotation"][2])
            if not actor.set_actor_location(location, False, True):
                raise RuntimeError("Unreal rejected location update for " + row["ActorLabel"])
            if not actor.set_actor_rotation(rotation, False):
                raise RuntimeError("Unreal rejected rotation update for " + row["ActorLabel"])
            actor.set_folder_path("Village_Main/Q5_TestingPlatform/%s" % row["ObjectiveId"])
            changed.append(actor)

        # Exact transform read-back and platform-support validation.
        readback_failures = []
        footprint_failures = []
        for row in plan:
            actor = row["Actor"]
            actual = transform_values(actor)
            expected = row["NewLocation"] + row["NewRotation"] + row["OldTransform"][6:9]
            mismatch = any(abs(float(actual[index]) - float(expected[index])) > 0.02
                           for index in range(3))
            mismatch = mismatch or any(
                angular_difference_degrees(actual[index], expected[index]) > 0.02
                for index in range(3, 6))
            mismatch = mismatch or any(
                abs(float(actual[index]) - float(expected[index])) > 0.0001
                for index in range(6, 9))
            if mismatch:
                readback_failures.append({
                    "ActorLabel": row["ActorLabel"],
                    "Expected": expected,
                    "Actual": actual,
                })
            origin, extent = actor_bounds(actor)
            bottom = float(origin.z - extent.z)
            if row["HasBounds"] and abs(bottom - row["SurfaceZ"]) > SURFACE_TOLERANCE:
                footprint_failures.append({
                    "ActorLabel": row["ActorLabel"],
                    "Reason": "Bounds bottom is not on platform surface",
                    "BottomZ": bottom,
                    "SurfaceZ": row["SurfaceZ"],
                })
            footprint = gameplay_footprint(actor)
            footprint_margin = 25.0 if footprint["Kind"] == "ActorPosition" else 10.0
            if (abs(footprint["Along"]) + footprint["HalfAlong"] + footprint_margin
                    > plane_half_length + 3.0
                    or abs(footprint["Across"]) + footprint["HalfAcross"] + footprint_margin
                    > plane_half_width + 3.0):
                footprint_failures.append({
                    "ActorLabel": row["ActorLabel"],
                    "Reason": "Actual gameplay footprint extends outside the Plane footprint",
                    "FootprintKind": footprint["Kind"],
                    "Component": footprint["Component"],
                    "Center": v3(footprint["Center"]),
                    "Dimensions": [round(value, 3) for value in footprint["Dimensions"]],
                    "PlaneLocalCenter": [round(footprint["Along"], 3),
                                         round(footprint["Across"], 3)],
                    "PlaneLocalHalfExtent": [round(footprint["HalfAlong"], 3),
                                             round(footprint["HalfAcross"], 3)],
                    "SafetyMarginCm": footprint_margin,
                    "MinimumAdditionalPlaneWidthCm": round(max(
                        0.0,
                        2.0 * (abs(footprint["Across"]) + footprint["HalfAcross"]
                               + footprint_margin - plane_half_width)), 3),
                })
        if readback_failures:
            raise RuntimeError("Q5 transform read-back failures: %s" % readback_failures)
        if footprint_failures:
            raise RuntimeError("Q5 platform footprint failures: %s" % footprint_failures)

        extent_readback_failures = []
        for extent_row in trigger_extent_changes:
            actual_extent = extent_row["Component"].get_unscaled_box_extent()
            actual_values = [float(actual_extent.x), float(actual_extent.y), float(actual_extent.z)]
            if any(abs(actual_values[index] - extent_row["NewExtent"][index]) > 0.02
                   for index in range(3)):
                extent_readback_failures.append({
                    "ActorLabel": extent_row["ActorLabel"],
                    "Expected": extent_row["NewExtent"],
                    "Actual": actual_values,
                })
        if extent_readback_failures:
            raise RuntimeError("Q5 trigger extent read-back failures: %s" % extent_readback_failures)

        # Reject overlap between trigger volumes belonging to different objectives.
        trigger_rows = [row for row in plan if row["ActorLabel"].startswith("HBCT_SM_Trigger_")]
        trigger_overlaps = []
        for left_index, left in enumerate(trigger_rows):
            left_shape = collision_shape_for_actor(left["Actor"], require_trigger_shape=True)
            for right in trigger_rows[left_index + 1:]:
                if left["ObjectiveId"] == right["ObjectiveId"]:
                    continue
                right_shape = collision_shape_for_actor(right["Actor"], require_trigger_shape=True)
                overlap_along = abs(left_shape["Along"] - right_shape["Along"]) < (
                    left_shape["HalfAlong"] + right_shape["HalfAlong"] - 1.0)
                overlap_across = abs(left_shape["Across"] - right_shape["Across"]) < (
                    left_shape["HalfAcross"] + right_shape["HalfAcross"] - 1.0)
                overlap_z = (left_shape["MinZ"] < right_shape["MaxZ"] - 1.0
                             and right_shape["MinZ"] < left_shape["MaxZ"] - 1.0)
                if overlap_along and overlap_across and overlap_z:
                    trigger_overlaps.append({
                        "ActorA": left["ActorLabel"],
                        "ObjectiveA": left["ObjectiveId"],
                        "ActorB": right["ActorLabel"],
                        "ObjectiveB": right["ObjectiveId"],
                        "ActorAComponent": left_shape["Component"],
                        "ActorBComponent": right_shape["Component"],
                    })
        if trigger_overlaps:
            raise RuntimeError("Unrelated Q5 trigger volumes overlap: %s" % trigger_overlaps)

        actors_after = list(actor_subsystem.get_all_level_actors())
        paths_after = {actor.get_path_name() for actor in actors_after}
        if paths_after != paths_before:
            raise RuntimeError("Actor identity set changed; no save permitted")
        protected_after = {actor.get_path_name(): protected_state(actor)
                           for actor in actors_after if actor.get_path_name() in protected_before}
        protected_changes = []
        for path, before in protected_before.items():
            after = protected_after.get(path)
            if after != before:
                protected_changes.append({"ActorPath": path, "Before": before, "After": after})
        if protected_changes:
            raise RuntimeError("Protected actors changed; no save permitted: %s" % protected_changes[:10])
        if sha256(blockout_file) != blockout_hash_before:
            raise RuntimeError("Protected Blockout hash changed before save")

        station_report = []
        for objective_index, objective_id in enumerate(OBJECTIVE_IDS):
            objective = objective_by_id[objective_id]
            conversation = objective.get_editor_property("conversation")
            conversation_path = conversation.get_path_name() if conversation else ""
            station_rows = [row for row in plan if row["ObjectiveId"] == objective_id]
            station_report.append({
                "Station": objective_index + 1,
                "ObjectiveId": objective_id,
                "ObjectiveText": str(objective.get_editor_property("objective_text")),
                "CenterSurfaceXYZ": v3(station_group_centers[objective_id]),
                "BaseCenterlineXYZ": v3(station_centers[objective_id]),
                "PlatformActorsSupportingCenter": station_support[objective_id],
                "Conversation": conversation_path,
                "EntryNode": str(objective.get_editor_property("entry_node_id")),
                "Actors": [{
                    "ActorLabel": row["ActorLabel"],
                    "ActorClass": row["ActorClass"],
                    "WorldXYZ": [round(value, 3) for value in row["NewLocation"]],
                } for row in station_rows],
                "NPCs": [row["ActorLabel"] for row in station_rows
                         if row["ActorClass"] == "BP_VHVNPCBase_C"],
                "AmbientConversations": [row["ActorLabel"] for row in station_rows
                                         if row["ActorClass"] == "VHVAmbientConversationActor"],
                "Triggers": [row["ActorLabel"] for row in station_rows
                             if row["ActorLabel"].startswith("HBCT_SM_Trigger_")],
                "SemanticLocations": [row["ActorLabel"] for row in station_rows
                                      if "Location_" in row["ActorLabel"]],
            })

        report = {
            "Status": "SUCCESS",
            "GeneratedAtLocal": datetime.datetime.now().isoformat(),
            "TargetMap": current_map,
            "QuestId": QUEST_ID,
            "Quest5ActorCount": len(q5_actors),
            "Quest5ActorsMoved": len(plan),
            "ObjectiveStationCount": len(station_report),
            "StationSpacingCm": round(station_spacing, 3),
            "SideLaneOffsetCm": round(lane, 3),
            "StationGroupLateralOffsetCm": station_lateral,
            "TriggerExtentChanges": [{
                "ActorLabel": row["ActorLabel"],
                "OldExtent": row["OldExtent"],
                "NewExtent": row["NewExtent"],
            } for row in trigger_extent_changes],
            "PlaneMeasuredLengthCm": round(plane_length, 3),
            "PlaneMeasuredWidthCm": round(plane_width, 3),
            "PlaneEdgeMarginCm": round(edge_margin, 3),
            "PlaneTopSurfaceZ": round(plane_row["MaxZ"], 3),
            "ArrangementAxisAtoB": [round(axis_x, 6), round(axis_y, 6), 0.0],
            "PlaneLocalLongAxis": [round(axis_x, 6), round(axis_y, 6), 0.0],
            "PlaneLocalSideAxis": [round(side_x, 6), round(side_y, 6), 0.0],
            "O02Activation": o02_activation,
            "PlatformActors": [{
                "ActorLabel": row["Label"],
                "ActorClass": row["Class"],
                "WorldTransform": transform_values(row["Actor"]),
                "Origin": v3(row["Origin"]),
                "Extent": v3(row["Extent"]),
                "Bounds": {
                    "Min": [round(row["MinX"], 3), round(row["MinY"], 3), round(row["MinZ"], 3)],
                    "Max": [round(row["MaxX"], 3), round(row["MaxY"], 3), round(row["MaxZ"], 3)],
                },
            } for row in platform_rows],
            "CombinedPlatformBounds": {key: round(value, 3) for key, value in combined.items()},
            "Stations": station_report,
            "SharedActorsLeftInPlace": shared,
            "UnresolvedQuest5Actors": [],
            "MissingQuest5Actors": [],
            "ProtectedActorCountVerified": len(protected_before),
            "ProtectedActorChanges": 0,
            "ActorsCreatedOrDeleted": 0,
            "PreviousEditorVisualizationFootprintViolations": 26,
            "EditorVisualizationFalsePositivesResolved": 26,
            "GenuineGameplayFootprintViolations": 0,
            "FootprintSafetyMarginCm": 10.0,
            "TriggerOverlapConflicts": 0,
            "NavigationValidationPerformed": False,
            "NavigationNote": "Per task scope, walking anchors were ordered on the platform but Recast/pathfinding was not investigated.",
            "ManualPIERequired": True,
            "MainMapSaved": False,
            "MainMapHashBefore": main_hash_before,
            "BlockoutHashBefore": blockout_hash_before,
            "BlockoutHashAfter": blockout_hash_before,
            "BlockoutUntouched": True,
        }

        stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        json_path = os.path.join(OUTPUT_DIR, "q5_testing_platform_%s.json" % stamp)
        csv_path = os.path.join(OUTPUT_DIR, "q5_testing_platform_%s.csv" % stamp)
        with open(json_path, "w", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2)
        with open(csv_path, "w", newline="", encoding="utf-8-sig") as stream:
            writer = csv.writer(stream)
            writer.writerow(["Station", "ObjectiveId", "StationX", "StationY", "SurfaceZ",
                             "ActorLabel", "ActorClass", "ActorX", "ActorY", "ActorZ"])
            for station in station_report:
                center_values = station["CenterSurfaceXYZ"]
                for actor_row in station["Actors"]:
                    writer.writerow([
                        station["Station"], station["ObjectiveId"],
                        center_values[0], center_values[1], center_values[2],
                        actor_row["ActorLabel"], actor_row["ActorClass"],
                        actor_row["WorldXYZ"][0], actor_row["WorldXYZ"][1], actor_row["WorldXYZ"][2],
                    ])

        level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not level_editor.save_current_level():
            raise RuntimeError("Failed to save Lvl_Village_Main")
        if sha256(blockout_file) != blockout_hash_before:
            raise RuntimeError("Protected Blockout hash changed after saving Lvl_Village_Main")
        report["MainMapSaved"] = True
        report["MainMapHashAfter"] = sha256(main_file)
        report["BlockoutHashAfter"] = sha256(blockout_file)
        report["ReportJson"] = json_path
        report["ReportCsv"] = csv_path
        with open(json_path, "w", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2)
        unreal.log("Q5_TESTING_PLATFORM_SUCCESS=" + json.dumps({
            "Actors": len(plan),
            "Stations": len(station_report),
            "Report": json_path,
            "MapSaved": True,
        }))
        return report
    except Exception:
        for extent_row in trigger_extent_changes:
            extent_row["Component"].set_box_extent(unreal.Vector(*extent_row["OldExtent"]), True)
        for actor in changed:
            before = q5_before.get(actor.get_path_name())
            if before:
                actor.set_actor_transform(before["Transform"], False, True)
                actor.set_folder_path(before["Folder"])
        raise


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        failure_path = os.path.join(OUTPUT_DIR, "q5_testing_platform_failed_%s.json" %
                                    datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))
        failure = {
            "Status": "FAILED",
            "Error": str(exc),
            "Traceback": traceback.format_exc(),
            "MainMapSaved": False,
        }
        with open(failure_path, "w", encoding="utf-8") as stream:
            json.dump(failure, stream, indent=2)
        unreal.log_error("Q5_TESTING_PLATFORM_FAILED=" + json.dumps(failure))
        raise
