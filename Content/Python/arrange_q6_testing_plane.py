"""Arrange Quest 6-only actors on the saved Lvl_Village_Main testing Plane.

The operation is deterministic, transactional, and placement-only. It derives the
Plane geometry from the loaded level, verifies the live Quest 6 definition and actor
inventory, computes every transform before mutation, then saves only after protected
actors, Q6 trigger extents, footprints, and actor identity all pass read-back.
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
QUEST_ID = "Q_HBCT_06_POSITIVE_REINFORCEMENT"
QUEST_ARC_PATH = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
Q6_PREFIX = "HBCT_PR_"
Q6_SOURCE_FOLDER = "07_PositiveReinforcement"
Q6_TARGET_FOLDER = "Village_Main/Q6_TestingPlane"
OUTPUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "VHV_Q6TestingPlane")
PLANE_LABEL = "Plane"
SURFACE_CLEARANCE = 2.0
SURFACE_TOLERANCE = 6.0
EDGE_MARGIN = 430.0

OBJECTIVE_IDS = [
    "O01_NoticeSuccess",
    "O02_UnderstandReinforcement",
    "O03_ExploreHealthFair",
    "O04_MatchReinforcementTypes",
    "O05_ChooseMeaningfulReinforcement",
    "O06_TestReinforcementTiming",
    "O07_OrderReinforcementTiming",
    "O08_CheckInWithChai",
    "O09_ReinforceChaiProgress",
    "O10_ReinforceExercise",
    "O11_ChooseSelfReward",
    "O12_JoinWalkingChallenge",
    "O13_SeveralWeeksLater",
    "O14_SeeFeedbackReinforcement",
    "O15_FinalDebrief",
]

# Objectives without their own level actor deliberately share the preceding authored
# scene. They remain separate quest stages and are not completed or altered here.
STATION_CANONICAL_X = {
    "O01_NoticeSuccess": -50.0,
    "O02_UnderstandReinforcement": 900.0,
    "O03_ExploreHealthFair": 2050.0,
    "O04_MatchReinforcementTypes": 2350.0,
    "O05_ChooseMeaningfulReinforcement": 3400.0,
    "O06_TestReinforcementTiming": 4600.0,
    "O07_OrderReinforcementTiming": 4800.0,
    "O08_CheckInWithChai": 6000.0,
    "O09_ReinforceChaiProgress": 6100.0,
    "O10_ReinforceExercise": 6200.0,
    "O11_ChooseSelfReward": 6300.0,
    "O12_JoinWalkingChallenge": 7800.0,
    "O13_SeveralWeeksLater": 9000.0,
    "O14_SeeFeedbackReinforcement": 9600.0,
    "O15_FinalDebrief": 10700.0,
}


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


def actor_class(actor):
    return actor.get_class().get_name()


def folder(actor):
    return str(actor.get_folder_path())


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


def angular_difference(left, right):
    return abs((float(left) - float(right) + 180.0) % 360.0 - 180.0)


def actor_bounds(actor):
    try:
        return actor.get_actor_bounds(False, True)
    except TypeError:
        return actor.get_actor_bounds(False)


def component_location(component):
    try:
        return component.get_world_location()
    except Exception:
        return component.get_component_location()


def component_rotation(component):
    try:
        return component.get_world_rotation()
    except Exception:
        return component.get_component_rotation()


def component_scale(component):
    try:
        return component.get_world_scale()
    except Exception:
        transform = component.get_component_transform()
        try:
            return transform.scale3d
        except Exception:
            return transform.get_editor_property("scale3d")


def tag_name(value):
    try:
        return str(unreal.GameplayTagLibrary.get_tag_name(value))
    except Exception:
        return ""


def safe_property(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def primitive_state(actor):
    rows = []
    for component in list(actor.get_components_by_class(unreal.PrimitiveComponent) or []):
        row = {"Name": component.get_name(), "Class": component.get_class().get_name()}
        for name, key in (
                ("visible", "Visible"),
                ("hidden_in_game", "HiddenInGame"),
                ("generate_overlap_events", "GenerateOverlapEvents")):
            value = safe_property(component, name, None)
            if value is not None:
                row[key] = bool(value)
        try:
            row["CollisionEnabled"] = str(component.get_collision_enabled())
        except Exception:
            pass
        relative_location = safe_property(component, "relative_location", None)
        relative_rotation = safe_property(component, "relative_rotation", None)
        relative_scale = safe_property(component, "relative_scale3d", None)
        if relative_location is not None:
            row["RelativeLocation"] = v3(relative_location)
        if relative_rotation is not None:
            row["RelativeRotation"] = [
                round(float(relative_rotation.pitch), 3),
                round(float(relative_rotation.yaw), 3),
                round(float(relative_rotation.roll), 3),
            ]
        if relative_scale is not None:
            row["RelativeScale"] = v3(relative_scale)
        if isinstance(component, unreal.BoxComponent):
            row["BoxExtent"] = v3(component.get_unscaled_box_extent())
        if isinstance(component, unreal.CapsuleComponent):
            row["CapsuleRadius"] = round(float(component.get_unscaled_capsule_radius()), 3)
            row["CapsuleHalfHeight"] = round(float(component.get_unscaled_capsule_half_height()), 3)
        rows.append(row)
    return sorted(rows, key=lambda item: (item["Name"], item["Class"]))


def stable_property_value(value):
    """Serialize trigger bindings/settings without including transient object identity."""
    if value is None:
        return None
    if isinstance(value, (list, tuple)):
        return [stable_property_value(item) for item in value]
    if hasattr(value, "get_path_name"):
        try:
            return value.get_path_name()
        except Exception:
            pass
    return str(value)


def trigger_gameplay_state(actor):
    """Immutable trigger gameplay configuration, deliberately excluding transforms."""
    rows = []
    channel_names = (
        "WORLD_STATIC", "WORLD_DYNAMIC", "PAWN", "VISIBILITY", "CAMERA",
        "PHYSICS_BODY", "VEHICLE", "DESTRUCTIBLE",
        "ENGINE_TRACE_CHANNEL1", "ENGINE_TRACE_CHANNEL2", "ENGINE_TRACE_CHANNEL3",
        "ENGINE_TRACE_CHANNEL4", "ENGINE_TRACE_CHANNEL5", "ENGINE_TRACE_CHANNEL6",
        "GAME_TRACE_CHANNEL1", "GAME_TRACE_CHANNEL2", "GAME_TRACE_CHANNEL3",
        "GAME_TRACE_CHANNEL4", "GAME_TRACE_CHANNEL5", "GAME_TRACE_CHANNEL6",
        "GAME_TRACE_CHANNEL7", "GAME_TRACE_CHANNEL8", "GAME_TRACE_CHANNEL9",
        "GAME_TRACE_CHANNEL10", "GAME_TRACE_CHANNEL11", "GAME_TRACE_CHANNEL12",
        "GAME_TRACE_CHANNEL13", "GAME_TRACE_CHANNEL14", "GAME_TRACE_CHANNEL15",
        "GAME_TRACE_CHANNEL16", "GAME_TRACE_CHANNEL17", "GAME_TRACE_CHANNEL18")
    for component in list(actor.get_components_by_class(unreal.PrimitiveComponent) or []):
        row = {"Name": component.get_name(), "Class": component.get_class().get_name()}
        for name, key in (("visible", "Visible"), ("hidden_in_game", "HiddenInGame"),
                          ("generate_overlap_events", "GenerateOverlapEvents")):
            value = safe_property(component, name, None)
            if value is not None:
                row[key] = bool(value)
        try:
            row["CollisionEnabled"] = str(component.get_collision_enabled())
        except Exception:
            pass
        try:
            row["CollisionProfile"] = str(component.get_collision_profile_name())
        except Exception:
            pass
        try:
            row["ObjectType"] = str(component.get_collision_object_type())
        except Exception:
            pass
        responses = {}
        for channel_name in channel_names:
            channel = getattr(unreal.CollisionChannel, channel_name, None)
            if channel is None:
                continue
            try:
                responses[channel_name] = str(component.get_collision_response_to_channel(channel))
            except Exception:
                pass
        row["CollisionResponses"] = responses
        if isinstance(component, unreal.BoxComponent):
            row["BoxExtent"] = v3(component.get_unscaled_box_extent())
        if isinstance(component, unreal.CapsuleComponent):
            row["CapsuleRadius"] = round(float(component.get_unscaled_capsule_radius()), 3)
            row["CapsuleHalfHeight"] = round(float(component.get_unscaled_capsule_half_height()), 3)
        rows.append(row)

    activation_properties = (
        "enabled", "required_active_quest_id", "required_active_objective_id",
        "activate_current_objective", "activate_current_objective_once",
        "trigger_world_action", "trigger_world_action_once", "track_world_action_as_objective",
        "world_action_receiver_tag", "world_action_tag", "require_player_interaction",
        "interaction_prompt", "trigger_conditions", "location_tag")
    activation = {
        name: stable_property_value(safe_property(actor, name, None))
        for name in activation_properties
    }
    try:
        actor_collision = bool(actor.get_actor_enable_collision())
    except Exception:
        actor_collision = None
    return {
        "ActorCollision": actor_collision,
        "ActorTags": sorted(str(item) for item in (safe_property(actor, "tags", []) or [])),
        "Activation": activation,
        "PrimitiveConfiguration": sorted(rows, key=lambda item: (item["Name"], item["Class"])),
    }


def actor_root_component(actor):
    """Resolve an Actor root with APIs exposed by UE 5.7 Python."""
    root = safe_property(actor, "root_component", None)
    if root:
        return root
    candidates = []
    for component in list(actor.get_components_by_class(unreal.SceneComponent) or []):
        parent = safe_property(component, "attach_parent", None)
        if parent is None and hasattr(component, "get_attach_parent"):
            try:
                parent = component.get_attach_parent()
            except Exception:
                parent = None
        if parent is None:
            candidates.append(component)
    if len(candidates) != 1:
        raise RuntimeError("Could not resolve one UE 5.7 root SceneComponent for %s; candidates=%s" %
                           (actor.get_actor_label(), [item.get_name() for item in candidates]))
    return candidates[0]


def trigger_attachment_state(actor):
    """Preserve child attachment offsets; the root transform is validated in world space."""
    root = actor_root_component(actor)
    root_name = root.get_name() if root else ""
    children = []
    for component in list(actor.get_components_by_class(unreal.SceneComponent) or []):
        if component == root:
            continue
        location = safe_property(component, "relative_location", None)
        rotation = safe_property(component, "relative_rotation", None)
        scale = safe_property(component, "relative_scale3d", None)
        children.append({
            "Name": component.get_name(),
            "Class": component.get_class().get_name(),
            "RelativeLocation": v3(location) if location is not None else None,
            "RelativeRotation": ([round(float(rotation.pitch), 3), round(float(rotation.yaw), 3),
                                  round(float(rotation.roll), 3)] if rotation is not None else None),
            "RelativeScale": v3(scale) if scale is not None else None,
        })
    return {"RootComponent": root_name,
            "Children": sorted(children, key=lambda item: (item["Name"], item["Class"]))}


def protected_state(actor):
    tags = safe_property(actor, "tags", []) or []
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
        "Hidden": bool(actor.is_hidden()) if hasattr(actor, "is_hidden") else None,
        "ActorCollision": collision,
        "Tags": sorted(str(item) for item in tags),
        "PrimitiveState": primitive_state(actor),
    }


def participant_tag(actor):
    if not hasattr(actor, "get_quest_participant_component"):
        return ""
    try:
        component = actor.get_quest_participant_component()
        return tag_name(component.get_editor_property("participant_tag")) if component else ""
    except Exception:
        return ""


def q6_signal_tags(actor):
    signals = []
    participant = participant_tag(actor)
    if participant.startswith("VHV.Participant.PR"):
        signals.append(participant)
    location_tag = safe_property(actor, "location_tag", None)
    location = tag_name(location_tag) if location_tag is not None else ""
    if location.startswith("VHV.Location.HBCT.PositiveReinforcement."):
        signals.append(location)
    receiver_tag = safe_property(actor, "world_action_receiver_tag", None)
    receiver = tag_name(receiver_tag) if receiver_tag is not None else ""
    if receiver.startswith("VHV.WorldReceiver.HBCT.PositiveReinforcement."):
        signals.append(receiver)
    receiver_component = safe_property(actor, "world_action_receiver", None)
    if receiver_component:
        receiver = tag_name(safe_property(receiver_component, "receiver_tag", None))
        if receiver.startswith("VHV.WorldReceiver.HBCT.PositiveReinforcement."):
            signals.append(receiver)
    return sorted(set(signals))


def find_quest_definition():
    arc = unreal.EditorAssetLibrary.load_asset(QUEST_ARC_PATH)
    if not arc:
        raise RuntimeError("Missing quest arc: " + QUEST_ARC_PATH)
    matches = [quest for quest in list(arc.get_editor_property("quests") or [])
               if str(quest.get_editor_property("quest_id")) == QUEST_ID]
    if len(matches) != 1:
        raise RuntimeError("Expected exactly one Quest 6 definition; found %d" % len(matches))
    objectives = list(matches[0].get_editor_property("objectives") or [])
    actual_ids = [str(item.get_editor_property("objective_id")) for item in objectives]
    if actual_ids != OBJECTIVE_IDS:
        raise RuntimeError("Saved Quest 6 objective order differs from authored order: %s" % actual_ids)
    return matches[0], objectives


def canonical_layout():
    """Return every expected Q6 actor's authored local placement and station owner."""
    rows = {}

    def add(label, xyz, objective, yaw=0.0, role="SceneActor"):
        if label in rows:
            raise RuntimeError("Duplicate canonical Q6 label: " + label)
        rows[label] = {
            "XYZ": [float(xyz[0]), float(xyz[1]), float(xyz[2])],
            "Yaw": float(yaw),
            "ObjectiveId": objective,
            "Role": role,
        }

    npc_specs = [
        ("SuccessWalker", (100, -220, 50), "O01_NoticeSuccess"),
        ("SuccessWalkerFriend", (100, -70, 50), "O01_NoticeSuccess"),
        ("WaterChild", (100, 180, 50), "O01_NoticeSuccess"),
        ("WaterParent", (100, 330, 50), "O01_NoticeSuccess"),
        ("HealthFairExerciseVillager", (1750, -420, 50), "O03_ExploreHealthFair"),
        ("HealthFairVHV", (1900, -300, 50), "O03_ExploreHealthFair"),
        ("MaterialVillager", (2050, -420, 50), "O03_ExploreHealthFair"),
        ("ActivityVillager", (2350, -20, 50), "O03_ExploreHealthFair"),
        ("TokenVillager", (2050, 380, 50), "O03_ExploreHealthFair"),
        ("FeedbackVillager", (1750, 380, 50), "O03_ExploreHealthFair"),
        ("PreferencePraise", (3300, -250, 50), "O05_ChooseMeaningfulReinforcement"),
        ("PreferenceActivity", (3400, 0, 50), "O05_ChooseMeaningfulReinforcement"),
        ("PreferenceToken", (3500, 250, 50), "O05_ChooseMeaningfulReinforcement"),
        ("DemoVillager", (4600, 150, 50), "O06_TestReinforcementTiming"),
        # Head and Nurse are left at their later payoff scenes, matching the authored
        # runtime setup; they remain bound to earlier ambient conversations by reference.
        ("VillageHead", (8450, 180, 50), "O12_JoinWalkingChallenge"),
        ("HealthNurse", (9600, -20, 50), "O14_SeeFeedbackReinforcement"),
    ]
    for suffix, xyz, objective in npc_specs:
        add(Q6_PREFIX + "NPC_" + suffix, xyz, objective, 180.0, "NPC")

    anchors = [
        ("PRSuccessWalker", (100, -220, 50), "O01_NoticeSuccess"),
        ("PRSuccessChild", (100, 180, 50), "O01_NoticeSuccess"),
        ("PRSuccessChai", (250, -20, 50), "O01_NoticeSuccess"),
        ("PRSuccessChaiExit", (500, -20, 50), "O01_NoticeSuccess"),
        ("PRInstructorIntro", (-200, -20, 50), "O01_NoticeSuccess"),
        ("PRInstructorLesson", (900, -20, 50), "O02_UnderstandReinforcement"),
        ("PRHealthFairCenter", (2000, -20, 50), "O03_ExploreHealthFair"),
        ("PRPreferencePraise", (3300, -250, 50), "O05_ChooseMeaningfulReinforcement"),
        ("PRPreferenceActivity", (3400, 0, 50), "O05_ChooseMeaningfulReinforcement"),
        ("PRPreferenceToken", (3500, 250, 50), "O05_ChooseMeaningfulReinforcement"),
        ("PRTimingTooEarly", (4400, -250, 50), "O06_TestReinforcementTiming"),
        ("PRTimingTooLate", (4600, 0, 50), "O06_TestReinforcementTiming"),
        ("PRTimingImmediate", (4800, 250, 50), "O06_TestReinforcementTiming"),
        ("PRChaiHome", (6000, -20, 50), "O08_CheckInWithChai"),
        ("PRChallengeWalkStart", (7100, -20, 50), "O12_JoinWalkingChallenge"),
        ("PRChallengeWalkMid", (7700, 100, 50), "O12_JoinWalkingChallenge"),
        ("PRChallengeWalkEnd", (8300, -20, 50), "O12_JoinWalkingChallenge"),
        ("PRHealthPostChai", (9800, -20, 50), "O14_SeeFeedbackReinforcement"),
        ("PRFinalDebrief", (10700, -20, 50), "O15_FinalDebrief"),
    ]
    for suffix, xyz, objective in anchors:
        add(Q6_PREFIX + "Location_" + suffix, xyz, objective, 0.0, "SemanticLocation")

    trigger_specs = [
        ("PreferenceMatching", (3400, -20, 50), "O05_ChooseMeaningfulReinforcement"),
        ("HealthParkIntro", (-50, -20, 50), "O01_NoticeSuccess"),
        ("HealthFairSocial", (1750, -420, 50), "O03_ExploreHealthFair"),
        ("HealthFairMaterial", (2050, -420, 50), "O03_ExploreHealthFair"),
        ("HealthFairActivity", (2350, -20, 50), "O03_ExploreHealthFair"),
        ("HealthFairToken", (2050, 380, 50), "O03_ExploreHealthFair"),
        ("HealthFairFeedback", (1750, 380, 50), "O03_ExploreHealthFair"),
        ("TimingArea", (4600, -20, 50), "O06_TestReinforcementTiming"),
        ("WalkingChallenge", (8450, -20, 50), "O12_JoinWalkingChallenge"),
        ("HealthPostFeedback", (9600, -20, 50), "O14_SeeFeedbackReinforcement"),
    ]
    for suffix, xyz, objective in trigger_specs:
        add(Q6_PREFIX + "Trigger_" + suffix, xyz, objective, 0.0, "Trigger")

    ambient_specs = [
        ("NoticeSuccess", (-50, -20, 50), "O01_NoticeSuccess"),
        ("HealthFairSocial", (1750, -420, 50), "O03_ExploreHealthFair"),
        ("HealthFairMaterial", (2050, -420, 50), "O03_ExploreHealthFair"),
        ("HealthFairActivity", (2350, -20, 50), "O03_ExploreHealthFair"),
        ("HealthFairToken", (2050, 380, 50), "O03_ExploreHealthFair"),
        ("HealthFairFeedback", (1750, 380, 50), "O03_ExploreHealthFair"),
        ("Timing", (4600, -20, 50), "O06_TestReinforcementTiming"),
        ("WalkingChallenge", (8450, -20, 50), "O12_JoinWalkingChallenge"),
        ("FeedbackResult", (9600, -20, 50), "O14_SeeFeedbackReinforcement"),
    ]
    for suffix, xyz, objective in ambient_specs:
        add(Q6_PREFIX + "Ambient_" + suffix, xyz, objective, 0.0, "ConversationController")

    for suffix, xyz in (
            ("HealthFairSocial", (1750, -420, 200)),
            ("HealthFairMaterial", (2050, -420, 200)),
            ("HealthFairActivity", (2350, -20, 200)),
            ("HealthFairToken", (2050, 380, 200)),
            ("HealthFairFeedback", (1750, 380, 200)),
            ("TokenBoard", (2200, 365, 235))):
        add(Q6_PREFIX + "Text_" + suffix, xyz, "O03_ExploreHealthFair", 180.0, "WorldText")
    for suffix, xyz in (
            ("TimingEarly", (4400, -250, 180)),
            ("TimingLate", (4600, 0, 180)),
            ("TimingImmediate", (4800, 250, 180))):
        add(Q6_PREFIX + "Text_" + suffix, xyz, "O06_TestReinforcementTiming", 180.0, "WorldText")
    add(Q6_PREFIX + "Text_WalkingChallengeBoard", (8450, -35, 290),
        "O12_JoinWalkingChallenge", 180.0, "WorldText")
    add(Q6_PREFIX + "Text_HealthResult", (9450, 145, 225),
        "O14_SeeFeedbackReinforcement", 180.0, "WorldText")
    add(Q6_PREFIX + "Text_MonitoringCalendar", (9650, 145, 225),
        "O14_SeeFeedbackReinforcement", 180.0, "WorldText")

    for suffix, xyz, objective in (
            ("MaterialFruit", (2120, -420, 75), "O03_ExploreHealthFair"),
            ("TokenBoard", (2200, 380, 120), "O03_ExploreHealthFair"),
            ("WalkingChallengeBoard", (8450, -20, 130), "O12_JoinWalkingChallenge"),
            ("HealthResult", (9450, 160, 120), "O14_SeeFeedbackReinforcement"),
            ("MonitoringCalendar", (9650, 160, 120), "O14_SeeFeedbackReinforcement")):
        add(Q6_PREFIX + "Prop_" + suffix, xyz, objective, 0.0, "Prop")

    if len(rows) != 71:
        raise RuntimeError("Internal canonical Q6 inventory should contain 71 actors, got %d" % len(rows))
    return rows


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        raise RuntimeError("No Editor world is loaded")
    current_map = map_path(world)
    if current_map == PROTECTED_MAP or "Lvl_Village_Blockout" in current_map:
        raise RuntimeError("Protected Blockout is loaded; refusing work")
    # PythonScriptCommandlet starts in /Temp/Untitled_0 even when a map is supplied
    # on its command line. Only that empty commandlet world may be redirected, and
    # the loaded package is verified again immediately afterward.
    if current_map == "/Temp/Untitled_0":
        loaded_world = unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP)
        if not loaded_world:
            raise RuntimeError("Could not load the authorized target map: " + TARGET_MAP)
        world = unreal.EditorLevelLibrary.get_editor_world()
        current_map = map_path(world)
    if current_map != TARGET_MAP:
        raise RuntimeError("Lvl_Village_Main must be loaded; current map is " + current_map)

    content_dir = unreal.Paths.project_content_dir()
    main_file = os.path.join(content_dir, "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
    blockout_file = os.path.join(content_dir, "VHV_Stuff", "Maps", "Lvl_Village_Blockout.umap")
    quest_asset_file = os.path.join(content_dir, "VHV_Stuff", "HBCT", "Quests", "DA_QuestArc_HBCT.uasset")
    main_hash_before = sha256(main_file)
    blockout_hash_before = sha256(blockout_file)
    quest_hash_before = sha256(quest_asset_file)
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = list(actor_subsystem.get_all_level_actors())
    paths_before = {actor.get_path_name() for actor in all_actors}
    if len(paths_before) != len(all_actors):
        raise RuntimeError("Duplicate actor paths detected")

    canonical = canonical_layout()
    q6_actors = [actor for actor in all_actors if actor.get_actor_label().startswith(Q6_PREFIX)]
    q6_by_label = {actor.get_actor_label(): actor for actor in q6_actors}
    if len(q6_by_label) != len(q6_actors):
        raise RuntimeError("Quest 6 actor labels are not unique")
    missing = sorted(set(canonical) - set(q6_by_label))
    unexpected = sorted(set(q6_by_label) - set(canonical))
    if missing or unexpected:
        raise RuntimeError("Current Q6 inventory differs from the 71 authored actors; missing=%s unexpected=%s" %
                           (missing, unexpected))

    # Folder names are not accepted as ownership proof. Any non-prefixed actor with a
    # Q6 folder/tag is ambiguous unless it is a verified persistent shared participant.
    shared_tags = {"VHV.Participant.Instructor", "VHV.Participant.UncleChai"}
    ambiguous = []
    shared = []
    for actor in all_actors:
        participant = participant_tag(actor)
        signals = q6_signal_tags(actor)
        q6_folder = (folder(actor).startswith(Q6_SOURCE_FOLDER)
                     or folder(actor).startswith(Q6_TARGET_FOLDER))
        if participant in shared_tags:
            shared.append(actor)
            continue
        if not actor.get_actor_label().startswith(Q6_PREFIX) and (signals or q6_folder):
            ambiguous.append({
                "ActorLabel": actor.get_actor_label(),
                "Folder": folder(actor),
                "Signals": signals,
            })
    if ambiguous:
        raise RuntimeError("Ambiguous non-prefixed Quest 6 ownership; no placement applied: %s" % ambiguous)
    for required_tag in sorted(shared_tags):
        matches = [actor for actor in shared if participant_tag(actor) == required_tag]
        if len(matches) != 1:
            raise RuntimeError("Shared participant %s resolves %d actors" % (required_tag, len(matches)))

    quest, objectives = find_quest_definition()
    objective_by_id = {str(item.get_editor_property("objective_id")): item for item in objectives}

    plane_matches = [actor for actor in all_actors if actor.get_actor_label() == PLANE_LABEL]
    if len(plane_matches) != 1:
        raise RuntimeError("Expected exactly one Plane actor; found %d" % len(plane_matches))
    plane = plane_matches[0]
    plane_origin, plane_extent = actor_bounds(plane)
    plane_rotation = plane.get_actor_rotation()
    plane_scale = plane.get_actor_scale3d()
    if abs(float(plane_rotation.pitch)) > 1.0 or abs(float(plane_rotation.roll)) > 1.0:
        raise RuntimeError("Testing Plane is not horizontal: Pitch=%.3f Roll=%.3f" %
                           (float(plane_rotation.pitch), float(plane_rotation.roll)))

    yaw_radians = math.radians(float(plane_rotation.yaw))
    local_x = (math.cos(yaw_radians), math.sin(yaw_radians))
    local_y = (-math.sin(yaw_radians), math.cos(yaw_radians))
    scale_x = abs(float(plane_scale.x))
    scale_y = abs(float(plane_scale.y))
    if min(scale_x, scale_y) < 0.0001:
        raise RuntimeError("Testing Plane has a zero horizontal scale component")
    world_half_x = float(plane_extent.x)
    world_half_y = float(plane_extent.y)
    denom_x = abs(local_x[0]) * scale_x + abs(local_y[0]) * scale_y
    denom_y = abs(local_x[1]) * scale_x + abs(local_y[1]) * scale_y
    samples = []
    if denom_x > 0.0001:
        samples.append(world_half_x / denom_x)
    if denom_y > 0.0001:
        samples.append(world_half_y / denom_y)
    if not samples:
        raise RuntimeError("Could not derive Plane local dimensions")
    base_half = sum(samples) / float(len(samples))
    half_x = base_half * scale_x
    half_y = base_half * scale_y
    if half_x >= half_y:
        axis = local_x
        side = local_y
        plane_half_length = half_x
        plane_half_width = half_y
    else:
        axis = local_y
        side = local_x
        plane_half_length = half_y
        plane_half_width = half_x
    # Start at the negative end of the saved Plane's long local axis.
    plane_center_x = float(plane_origin.x)
    plane_center_y = float(plane_origin.y)
    plane_top_z = float(plane_origin.z + plane_extent.z)
    plane_length = plane_half_length * 2.0
    plane_width = plane_half_width * 2.0
    if plane_half_length <= EDGE_MARGIN + 1000.0:
        raise RuntimeError("Plane is too short after the required edge margin")
    if plane_half_width < 460.0:
        raise RuntimeError("Plane width %.3f cm cannot hold authored Q6 lateral groups" % plane_width)

    def local_coordinates(x, y):
        dx = float(x) - plane_center_x
        dy = float(y) - plane_center_y
        return dx * axis[0] + dy * axis[1], dx * side[0] + dy * side[1]

    canonical_min_x = min(row["XYZ"][0] for row in canonical.values())
    canonical_max_x = max(row["XYZ"][0] for row in canonical.values())
    canonical_max_abs_y = max(abs(row["XYZ"][1]) for row in canonical.values())
    usable_min = -plane_half_length + EDGE_MARGIN
    usable_max = plane_half_length - EDGE_MARGIN
    long_scale = (usable_max - usable_min) / (canonical_max_x - canonical_min_x)
    across_limit = plane_half_width - 65.0
    side_scale = min(1.0, across_limit / canonical_max_abs_y)
    if long_scale < 0.75:
        raise RuntimeError("Plane is too short for the authored Q6 station sequence; scale would be %.3f" % long_scale)
    if side_scale < 0.80:
        raise RuntimeError("Plane is too narrow for the authored Q6 station lanes; scale would be %.3f" % side_scale)

    def world_xy(canonical_x, canonical_y):
        along = usable_min + (float(canonical_x) - canonical_min_x) * long_scale
        across = float(canonical_y) * side_scale
        return (plane_center_x + axis[0] * along + side[0] * across,
                plane_center_y + axis[1] * along + side[1] * across,
                along, across)

    protected_actors = [actor for actor in all_actors if actor not in q6_actors]
    protected_before = {actor.get_path_name(): protected_state(actor) for actor in protected_actors}
    q6_before = {
        actor.get_path_name(): {
            "Transform": actor.get_actor_transform(),
            "TransformValues": transform_values(actor),
            "Folder": folder(actor),
            "PrimitiveState": primitive_state(actor),
        }
        for actor in q6_actors
    }
    q6_trigger_state_before = {
        actor.get_actor_label(): {
            "Gameplay": trigger_gameplay_state(actor),
            "Attachments": trigger_attachment_state(actor),
        }
        for actor in q6_actors if actor.get_actor_label().startswith(Q6_PREFIX + "Trigger_")
    }

    plan = []
    axis_yaw = math.degrees(math.atan2(axis[1], axis[0]))
    for label in sorted(canonical):
        actor = q6_by_label[label]
        spec = canonical[label]
        x, y, along, across = world_xy(spec["XYZ"][0], spec["XYZ"][1])
        desired_z = plane_top_z + spec["XYZ"][2]
        if spec["Role"] == "NPC":
            capsules = list(actor.get_components_by_class(unreal.CapsuleComponent) or [])
            if not capsules:
                raise RuntimeError("Q6 NPC has no CapsuleComponent: " + label)
            capsule = capsules[0]
            center = component_location(capsule)
            try:
                half_height = float(capsule.get_scaled_capsule_half_height())
            except Exception:
                half_height = float(capsule.get_unscaled_capsule_half_height()) * abs(float(component_scale(capsule).z))
            actor_location = actor.get_actor_location()
            capsule_center_offset_z = float(center.z - actor_location.z)
            desired_z = plane_top_z + SURFACE_CLEARANCE + half_height - capsule_center_offset_z
        old_rotation = actor.get_actor_rotation()
        desired_yaw = axis_yaw + spec["Yaw"]
        plan.append({
            "Actor": actor,
            "ActorPath": actor.get_path_name(),
            "ActorLabel": label,
            "ActorClass": actor_class(actor),
            "ObjectiveId": spec["ObjectiveId"],
            "Role": spec["Role"],
            "CanonicalXYZ": spec["XYZ"],
            "OldTransform": transform_values(actor),
            "NewLocation": [x, y, desired_z],
            "NewRotation": [float(old_rotation.pitch), desired_yaw, float(old_rotation.roll)],
            "PlaneLocal": [along, across],
        })

    changed = []
    try:
        # All transforms are already computed; mutation begins only here.
        for row in plan:
            actor = row["Actor"]
            actor.modify()
            if not actor.set_actor_location(unreal.Vector(*row["NewLocation"]), False, True):
                raise RuntimeError("Unreal rejected location update for " + row["ActorLabel"])
            rotation = unreal.Rotator(
                pitch=row["NewRotation"][0], yaw=row["NewRotation"][1], roll=row["NewRotation"][2])
            if not actor.set_actor_rotation(rotation, False):
                raise RuntimeError("Unreal rejected rotation update for " + row["ActorLabel"])
            actor.set_folder_path(Q6_TARGET_FOLDER + "/" + row["ObjectiveId"])
            changed.append(actor)

        readback_failures = []
        footprint_failures = []
        npc_grounding_failures = []
        for row in plan:
            actor = row["Actor"]
            actual = transform_values(actor)
            expected = row["NewLocation"] + row["NewRotation"] + row["OldTransform"][6:9]
            mismatch = any(abs(actual[index] - expected[index]) > 0.03 for index in range(3))
            mismatch = mismatch or any(
                angular_difference(actual[index], expected[index]) > 0.03 for index in range(3, 6))
            mismatch = mismatch or any(
                abs(actual[index] - expected[index]) > 0.0001 for index in range(6, 9))
            if mismatch:
                readback_failures.append({"Actor": row["ActorLabel"], "Expected": expected, "Actual": actual})

            along, across = local_coordinates(actual[0], actual[1])
            half_along = 0.0
            half_across = 0.0
            if row["Role"] == "Trigger":
                component = safe_property(actor, "box_component", None)
                if not component:
                    boxes = list(actor.get_components_by_class(unreal.BoxComponent) or [])
                    component = boxes[0] if boxes else None
                if not component:
                    raise RuntimeError("Q6 trigger has no BoxComponent: " + row["ActorLabel"])
                extent = component.get_unscaled_box_extent()
                scale = component_scale(component)
                rotation = component_rotation(component)
                ex = abs(float(extent.x) * float(scale.x))
                ey = abs(float(extent.y) * float(scale.y))
                component_yaw = math.radians(float(rotation.yaw))
                bx = (math.cos(component_yaw), math.sin(component_yaw))
                by = (-math.sin(component_yaw), math.cos(component_yaw))
                half_along = abs(axis[0] * bx[0] + axis[1] * bx[1]) * ex + abs(axis[0] * by[0] + axis[1] * by[1]) * ey
                half_across = abs(side[0] * bx[0] + side[1] * bx[1]) * ex + abs(side[0] * by[0] + side[1] * by[1]) * ey
                center = component_location(component)
                along, across = local_coordinates(center.x, center.y)
            elif row["Role"] == "NPC":
                capsule = list(actor.get_components_by_class(unreal.CapsuleComponent) or [None])[0]
                radius = float(capsule.get_scaled_capsule_radius())
                half_along = radius
                half_across = radius
                center = component_location(capsule)
                along, across = local_coordinates(center.x, center.y)
                bottom = float(center.z) - float(capsule.get_scaled_capsule_half_height())
                if abs(bottom - (plane_top_z + SURFACE_CLEARANCE)) > SURFACE_TOLERANCE:
                    npc_grounding_failures.append({
                        "Actor": row["ActorLabel"], "CapsuleBottomZ": bottom,
                        "ExpectedZ": plane_top_z + SURFACE_CLEARANCE})
            elif row["Role"] in ("WorldText", "Prop"):
                origin, extent = actor_bounds(actor)
                along, across = local_coordinates(origin.x, origin.y)
                half_along = abs(axis[0]) * float(extent.x) + abs(axis[1]) * float(extent.y)
                half_across = abs(side[0]) * float(extent.x) + abs(side[1]) * float(extent.y)
            # Semantic locations and ambient controllers use their gameplay position;
            # their Editor icons/billboards are intentionally excluded.
            if abs(along) + half_along + 10.0 > plane_half_length + 1.0:
                footprint_failures.append({"Actor": row["ActorLabel"], "Axis": "Length", "Along": along,
                                           "HalfExtent": half_along})
            if abs(across) + half_across + 10.0 > plane_half_width + 1.0:
                footprint_failures.append({"Actor": row["ActorLabel"], "Axis": "Width", "Across": across,
                                           "HalfExtent": half_across})
        if readback_failures:
            raise RuntimeError("Q6 transform read-back failures: %s" % readback_failures[:5])
        if npc_grounding_failures:
            raise RuntimeError("Q6 NPC grounding failures: %s" % npc_grounding_failures)
        if footprint_failures:
            raise RuntimeError("Q6 gameplay footprint failures: %s" % footprint_failures)

        # Reject only overlap between trigger groups owned by different objectives.
        trigger_shapes = []
        for row in plan:
            if row["Role"] != "Trigger":
                continue
            actor = row["Actor"]
            component = safe_property(actor, "box_component", None)
            if not component:
                component = list(actor.get_components_by_class(unreal.BoxComponent) or [None])[0]
            center = component_location(component)
            extent = component.get_unscaled_box_extent()
            scale = component_scale(component)
            rotation = component_rotation(component)
            ex = abs(float(extent.x) * float(scale.x))
            ey = abs(float(extent.y) * float(scale.y))
            ez = abs(float(extent.z) * float(scale.z))
            cyaw = math.radians(float(rotation.yaw))
            bx = (math.cos(cyaw), math.sin(cyaw))
            by = (-math.sin(cyaw), math.cos(cyaw))
            along, across = local_coordinates(center.x, center.y)
            trigger_shapes.append({
                "Label": row["ActorLabel"], "ObjectiveId": row["ObjectiveId"],
                "Along": along, "Across": across,
                "HalfAlong": abs(axis[0] * bx[0] + axis[1] * bx[1]) * ex + abs(axis[0] * by[0] + axis[1] * by[1]) * ey,
                "HalfAcross": abs(side[0] * bx[0] + side[1] * bx[1]) * ex + abs(side[0] * by[0] + side[1] * by[1]) * ey,
                "MinZ": float(center.z) - ez, "MaxZ": float(center.z) + ez,
            })
        overlaps = []
        for index, left in enumerate(trigger_shapes):
            for right in trigger_shapes[index + 1:]:
                if left["ObjectiveId"] == right["ObjectiveId"]:
                    continue
                if (abs(left["Along"] - right["Along"]) < left["HalfAlong"] + right["HalfAlong"] - 1.0
                        and abs(left["Across"] - right["Across"]) < left["HalfAcross"] + right["HalfAcross"] - 1.0
                        and left["MinZ"] < right["MaxZ"] - 1.0
                        and right["MinZ"] < left["MaxZ"] - 1.0):
                    overlaps.append([left["Label"], right["Label"]])
        if overlaps:
            raise RuntimeError("Unintended cross-objective Q6 trigger overlaps: %s" % overlaps)

        q6_trigger_state_after = {
            actor.get_actor_label(): {
                "Gameplay": trigger_gameplay_state(actor),
                "Attachments": trigger_attachment_state(actor),
            }
            for actor in q6_actors if actor.get_actor_label().startswith(Q6_PREFIX + "Trigger_")
        }
        trigger_configuration_changes = []
        for label, before in q6_trigger_state_before.items():
            after = q6_trigger_state_after.get(label)
            if after != before:
                trigger_configuration_changes.append({"Actor": label, "Before": before, "After": after})
        if trigger_configuration_changes:
            raise RuntimeError("A Q6 trigger gameplay setting, extent, or child attachment changed; no save permitted: %s" %
                               trigger_configuration_changes[:2])

        # Actor transform read-back above authorizes the root change. Independently
        # prove that each root component's final world transform is exactly that actor
        # transform, so unrelated root offsets/scales are still rejected.
        root_transform_failures = []
        for row in plan:
            if row["Role"] != "Trigger":
                continue
            actor = row["Actor"]
            root = actor_root_component(actor)
            if not root:
                root_transform_failures.append({"Actor": row["ActorLabel"], "Reason": "Missing root component"})
                continue
            actor_values = transform_values(actor)
            root_location = component_location(root)
            root_rotation = component_rotation(root)
            root_scale = component_scale(root)
            root_values = [float(root_location.x), float(root_location.y), float(root_location.z),
                           float(root_rotation.pitch), float(root_rotation.yaw), float(root_rotation.roll),
                           float(root_scale.x), float(root_scale.y), float(root_scale.z)]
            mismatch = any(abs(root_values[index] - actor_values[index]) > 0.03 for index in range(3))
            mismatch = mismatch or any(
                angular_difference(root_values[index], actor_values[index]) > 0.03 for index in range(3, 6))
            mismatch = mismatch or any(
                abs(root_values[index] - actor_values[index]) > 0.0001 for index in range(6, 9))
            if mismatch:
                root_transform_failures.append({
                    "Actor": row["ActorLabel"], "ActorTransform": actor_values,
                    "RootWorldTransform": root_values})
        if root_transform_failures:
            raise RuntimeError("Q6 trigger root changes are not attributable to planned actor transforms: %s" %
                               root_transform_failures)

        actors_after = list(actor_subsystem.get_all_level_actors())
        paths_after = {actor.get_path_name() for actor in actors_after}
        if paths_after != paths_before:
            raise RuntimeError("Actor identity set changed; no save permitted")
        protected_after = {actor.get_path_name(): protected_state(actor)
                           for actor in actors_after if actor.get_path_name() in protected_before}
        protected_changes = []
        for path, before in protected_before.items():
            if protected_after.get(path) != before:
                protected_changes.append({"ActorPath": path, "Before": before,
                                          "After": protected_after.get(path)})
        if protected_changes:
            raise RuntimeError("Protected actors changed; no save permitted: %s" % protected_changes[:3])
        if sha256(blockout_file) != blockout_hash_before:
            raise RuntimeError("Protected Blockout hash changed before save")
        if sha256(quest_asset_file) != quest_hash_before:
            raise RuntimeError("Quest data asset changed during placement; no save permitted")

        station_reports = []
        for index, objective_id in enumerate(OBJECTIVE_IDS):
            objective = objective_by_id[objective_id]
            center_x, center_y, center_along, _ = world_xy(STATION_CANONICAL_X[objective_id], 0.0)
            actor_rows = [row for row in plan if row["ObjectiveId"] == objective_id]
            conversation = objective.get_editor_property("conversation")
            station_reports.append({
                "Station": index + 1,
                "ObjectiveId": objective_id,
                "ObjectiveName": str(objective.get_editor_property("objective_text")),
                "CenterSurfaceXYZ": [round(center_x, 3), round(center_y, 3), round(plane_top_z, 3)],
                "PlaneLocalAlongCm": round(center_along, 3),
                "Conversation": conversation.get_path_name() if conversation else "",
                "NPCs": [row["ActorLabel"] for row in actor_rows if row["Role"] == "NPC"],
                "ConversationActors": [row["ActorLabel"] for row in actor_rows
                                       if row["Role"] == "ConversationController"],
                "Triggers": [row["ActorLabel"] for row in actor_rows if row["Role"] == "Trigger"],
                "SemanticLocations": [row["ActorLabel"] for row in actor_rows
                                      if row["Role"] == "SemanticLocation"],
                "Displays": [row["ActorLabel"] for row in actor_rows if row["Role"] == "WorldText"],
                "RewardChallengeActors": [row["ActorLabel"] for row in actor_rows
                                          if row["Role"] == "Prop"],
                "PhysicalActorCount": len(actor_rows),
                "SharedSceneNote": (
                    "No dedicated Q6 actor; authored gameplay continues in the adjacent existing scene."
                    if not actor_rows else ""),
            })

        shared_report = []
        for actor in shared:
            shared_report.append({
                "ActorLabel": actor.get_actor_label(),
                "ParticipantTag": participant_tag(actor),
                "Transform": transform_values(actor),
                "ReasonExcluded": "Persistent shared NPC; Q6 uses moved Q6 semantic staging anchors",
            })

        report = {
            "Status": "SUCCESS",
            "GeneratedAtLocal": datetime.datetime.now().isoformat(),
            "TargetMap": current_map,
            "QuestId": str(quest.get_editor_property("quest_id")),
            "QuestTitle": str(quest.get_editor_property("quest_title")),
            "Quest6ActorsIdentified": len(q6_actors),
            "Quest6ExclusiveActors": len(q6_actors),
            "Quest6ActorsMoved": len(plan),
            "SharedActorsExcluded": len(shared_report),
            "AmbiguousActors": [],
            "Plane": {
                "ActorLabel": plane.get_actor_label(),
                "ActorClass": actor_class(plane),
                "WorldTransform": transform_values(plane),
                "BoundsOrigin": v3(plane_origin),
                "BoundsExtent": v3(plane_extent),
                "MeasuredLengthCm": round(plane_length, 3),
                "MeasuredWidthCm": round(plane_width, 3),
                "TopSurfaceZ": round(plane_top_z, 3),
                "UsableLongRangeCm": [round(usable_min, 3), round(usable_max, 3)],
                "UsableAcrossRangeCm": [round(-across_limit, 3), round(across_limit, 3)],
                "LongAxis": [round(axis[0], 6), round(axis[1], 6), 0.0],
                "SideAxis": [round(side[0], 6), round(side[1], 6), 0.0],
            },
            "CanonicalLongScale": round(long_scale, 6),
            "CanonicalSideScale": round(side_scale, 6),
            "Stations": station_reports,
            "SharedActorsLeftInPlace": shared_report,
            "TriggerExtentsModified": [],
            "TriggerOverlapConflicts": 0,
            "GenuineGameplayFootprintViolations": 0,
            "EditorIconBoundsExcluded": True,
            "ProtectedActorCountVerified": len(protected_before),
            "ProtectedActorChanges": 0,
            "ActorsCreated": 0,
            "ActorsDeleted": 0,
            "ActorsDuplicated": 0,
            "GameplayCppChanged": 0,
            "QuestDataAssetsChanged": 0,
            "NavigationChanged": 0,
            "NavigationNote": "Q6 walking anchors are ordered; NavMesh/pathfinding was not changed or rebuilt.",
            "ManualPIERequired": True,
            "MainMapSaved": False,
            "MainMapHashBefore": main_hash_before,
            "BlockoutHashBefore": blockout_hash_before,
            "BlockoutHashAfter": blockout_hash_before,
            "BlockoutUntouched": True,
        }

        stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        json_path = os.path.join(OUTPUT_DIR, "q6_testing_plane_%s.json" % stamp)
        csv_path = os.path.join(OUTPUT_DIR, "q6_testing_plane_%s.csv" % stamp)
        with open(json_path, "w", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2)
        with open(csv_path, "w", newline="", encoding="utf-8-sig") as stream:
            writer = csv.writer(stream)
            writer.writerow(["Station", "ObjectiveId", "ObjectiveName", "StationX", "StationY",
                             "SurfaceZ", "ActorLabel", "ActorClass", "Role", "ActorX", "ActorY", "ActorZ"])
            for station in station_reports:
                matching = [row for row in plan if row["ObjectiveId"] == station["ObjectiveId"]]
                if not matching:
                    writer.writerow([station["Station"], station["ObjectiveId"], station["ObjectiveName"],
                                     *station["CenterSurfaceXYZ"], "", "", "", "", "", ""])
                for row in matching:
                    writer.writerow([station["Station"], station["ObjectiveId"], station["ObjectiveName"],
                                     *station["CenterSurfaceXYZ"], row["ActorLabel"], row["ActorClass"],
                                     row["Role"], *[round(value, 3) for value in row["NewLocation"]]])

        level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not level_editor.save_current_level():
            raise RuntimeError("Failed to save Lvl_Village_Main")
        if sha256(blockout_file) != blockout_hash_before:
            raise RuntimeError("Protected Blockout hash changed after saving Lvl_Village_Main")
        if sha256(quest_asset_file) != quest_hash_before:
            raise RuntimeError("Quest data asset changed after saving Lvl_Village_Main")
        report["MainMapSaved"] = True
        report["MainMapHashAfter"] = sha256(main_file)
        report["BlockoutHashAfter"] = sha256(blockout_file)
        report["ReportJson"] = json_path
        report["ReportCsv"] = csv_path
        with open(json_path, "w", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2)
        unreal.log("Q6_TESTING_PLANE_SUCCESS=" + json.dumps({
            "Actors": len(plan), "Stations": len(station_reports),
            "Report": json_path, "MapSaved": True}))
        return report
    except Exception:
        for actor in changed:
            before = q6_before.get(actor.get_path_name())
            if before:
                actor.set_actor_transform(before["Transform"], False, True)
                actor.set_folder_path(before["Folder"])
        raise


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        failure_path = os.path.join(
            OUTPUT_DIR, "q6_testing_plane_failed_%s.json" % datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))
        failure = {
            "Status": "FAILED",
            "Error": str(exc),
            "Traceback": traceback.format_exc(),
            "MainMapSaved": False,
        }
        with open(failure_path, "w", encoding="utf-8") as stream:
            json.dump(failure, stream, indent=2)
        unreal.log_error("Q6_TESTING_PLANE_FAILED=" + json.dumps(failure))
        raise
