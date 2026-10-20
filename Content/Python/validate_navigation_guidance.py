"""Read-only integration checks for VHV navigation guidance.

The checks are read-only: NavMesh projection and path queries run directly
against the loaded editor world, and no map is saved.
"""

import traceback
import unreal


LOG = "[VHV Guidance Validation]"
MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
PROLOGUE_MAP_PATH = MAP_PATH
ARC_PATH = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
ACTION_PATH = "/Game/Input/Actions/IA_LocateObjective"
CONTEXT_PATH = "/Game/Input/IMC_Default"
CONTROLLER_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"
GOLD_PATH = "/Game/VHV_Stuff/UI/Tracking/M_VHV_GuidanceGold"
BEAM_PATH = "/Game/VHV_Stuff/UI/Tracking/M_VHV_GuidanceBeam"
CORE_PATH = "/Game/VHV_Stuff/UI/Tracking/M_VHV_GuidanceCore"
PROLOGUE_TRACKING = {
    "O01_MeetInstructor": "VHV.Location.HBCT.Intro.Instructor",
    "O02_ReachMarket": "VHV.Location.HBCT.Market",
    "O03_ObserveMarket": "VHV.Location.HBCT.Market",
    "O04_ReportToInstructor": "VHV.Location.HBCT.Intro.Instructor",
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def path_name(value):
    return value.get_path_name() if value else ""


def validate_assets_and_input():
    gold = unreal.EditorAssetLibrary.load_asset(GOLD_PATH)
    beam = unreal.EditorAssetLibrary.load_asset(BEAM_PATH)
    core = unreal.EditorAssetLibrary.load_asset(CORE_PATH)
    require(isinstance(gold, unreal.Material), "Missing gold guidance material")
    require(isinstance(beam, unreal.Material), "Missing beam guidance material")
    require(isinstance(core, unreal.Material), "Missing masked beam core material")

    gold_vectors = {str(name) for name in
                    unreal.MaterialEditingLibrary.get_vector_parameter_names(gold)}
    gold_scalars = {str(name) for name in
                    unreal.MaterialEditingLibrary.get_scalar_parameter_names(gold)}
    beam_vectors = {str(name) for name in
                    unreal.MaterialEditingLibrary.get_vector_parameter_names(beam)}
    beam_scalars = {str(name) for name in
                    unreal.MaterialEditingLibrary.get_scalar_parameter_names(beam)}
    core_vectors = {str(name) for name in
                    unreal.MaterialEditingLibrary.get_vector_parameter_names(core)}
    core_scalars = {str(name) for name in
                    unreal.MaterialEditingLibrary.get_scalar_parameter_names(core)}
    require("GuidanceColor" in gold_vectors, "Gold material lacks GuidanceColor")
    require("EmissiveStrength" in gold_scalars, "Gold material lacks EmissiveStrength")
    require("GuidanceColor" in beam_vectors, "Beam material lacks GuidanceColor")
    require({"EmissiveStrength", "Opacity"}.issubset(beam_scalars),
            "Beam material lacks runtime scalar parameters")
    require("GuidanceColor" in core_vectors
            and "EmissiveStrength" in core_scalars,
            "Masked beam core lacks runtime parameters")

    action = unreal.EditorAssetLibrary.load_asset(ACTION_PATH)
    context = unreal.EditorAssetLibrary.load_asset(CONTEXT_PATH)
    require(action is not None, "Missing IA_LocateObjective")
    require(context is not None, "Missing IMC_Default")
    mapping_data = context.get_editor_property("default_key_mappings")
    mappings = list(mapping_data.get_editor_property("mappings"))
    x_mappings = []
    for mapping in mappings:
        mapped_action = mapping.get_editor_property("action")
        key = mapping.get_editor_property("key")
        key_name = str(key.get_editor_property("key_name"))
        if path_name(mapped_action) == path_name(action) and key_name == "X":
            x_mappings.append(mapping)
    require(len(x_mappings) == 1,
            "IA_LocateObjective must have exactly one X mapping; found {}".format(
                len(x_mappings)))

    controller_bp = unreal.EditorAssetLibrary.load_asset(CONTROLLER_PATH)
    require(controller_bp and controller_bp.generated_class(),
            "Missing player controller Blueprint")
    controller_cdo = unreal.get_default_object(controller_bp.generated_class())
    configured_action = controller_cdo.get_editor_property("locate_objective_action")
    require(path_name(configured_action) == path_name(action),
            "Player controller LocateObjectiveAction is not IA_LocateObjective")
    unreal.log("{} Controller quest flow enabled={}, arc='{}'.".format(
        LOG,
        controller_cdo.get_editor_property("use_quest_flow"),
        path_name(controller_cdo.get_editor_property("starting_quest_arc"))))

    guidance_cdo = unreal.get_default_object(unreal.VHVNavigationGuidanceActor)
    ui_manager_cdo = unreal.get_default_object(unreal.VHVUIManagerComponent)
    require(guidance_cdo.get_editor_property("max_arrow_count") <= 2,
            "Guidance must never display more than two arrow heads")
    require(guidance_cdo.get_editor_property("first_arrow_distance") >= 100.0,
            "First arrow distance is invalid")
    require(guidance_cdo.get_editor_property("second_arrow_distance")
            > guidance_cdo.get_editor_property("first_arrow_distance"),
            "Second arrow must be farther along the route")
    require(ui_manager_cdo.get_editor_property("guidance_update_interval") >= 0.1,
            "Guidance update interval is invalid")

    unreal.log("{} Materials, X mapping, controller binding, and config defaults passed.".format(LOG))


def find_route_endpoints(actors):
    starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    if not starts:
        starts = [actor for actor in actors
                  if isinstance(actor, unreal.Pawn)
                  or "player" in actor.get_actor_label().lower()]
    destinations = [actor for actor in actors
                    if isinstance(actor, unreal.VHVQuestLocationVolume)]
    require(starts, "No PlayerStart/player actor found in {}".format(MAP_PATH))
    require(destinations, "No quest location volumes found in {}".format(MAP_PATH))

    start_actor = starts[0]
    start = start_actor.get_actor_location()
    reachable_candidates = sorted(
        destinations,
        key=lambda actor: (actor.get_actor_location() - start).length(),
        reverse=True)
    return start_actor, start, reachable_candidates


def require_nav_path(world, start, destination, label):
    extent = unreal.Vector(350.0, 350.0, 600.0)
    projected_start = unreal.NavigationSystemV1.project_point_to_navigation(
        world, start, None, None, extent)
    projected_destination = unreal.NavigationSystemV1.project_point_to_navigation(
        world, destination, None, None, extent)
    require(projected_start is not None,
            "{} start does not project to NavMesh".format(label))
    require(projected_destination is not None,
            "{} destination does not project to NavMesh".format(label))
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(
        world, projected_start, projected_destination, None, None)
    require(path and path.is_valid() and not path.is_partial()
            and len(path.path_points or []) >= 2,
            "{} has no complete NavMesh route".format(label))
    return path


def validate_map_route():
    require(unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH),
            "Could not load {}".format(MAP_PATH))
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    require(any(isinstance(actor, unreal.NavMeshBoundsVolume) for actor in actors),
            "The village map has no NavMeshBoundsVolume")

    start_actor, start, destinations = find_route_endpoints(actors)
    world = unreal.EditorLevelLibrary.get_editor_world()
    selected = None
    selected_path = None
    for destination_actor in destinations:
        destination = destination_actor.get_actor_location()
        if (destination - start).length() < 900.0:
            continue
        try:
            selected_path = require_nav_path(
                world, start, destination, destination_actor.get_actor_label())
            selected = destination_actor
            break
        except RuntimeError:
            continue
    require(selected is not None, "No complete NavMesh route was found")
    unreal.log(
        "{} NavMesh route passed: '{}' -> '{}', {} path points.".format(
            LOG, start_actor.get_actor_label(), selected.get_actor_label(),
            len(selected_path.path_points or [])))


def validate_prologue_tracking():
    arc = unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    require(arc is not None, "Missing production HBCT quest arc")
    prologue = next((quest for quest in arc.get_editor_property("quests")
                     if str(quest.get_editor_property("quest_id")) == "Q_HBCT_00_INTRO"), None)
    require(prologue is not None, "Production arc has no prologue quest")
    objectives = {str(item.get_editor_property("objective_id")): item
                  for item in prologue.get_editor_property("objectives")}
    for objective_id, expected_tag in PROLOGUE_TRACKING.items():
        objective = objectives.get(objective_id)
        require(objective is not None, "Missing prologue objective {}".format(objective_id))
        require(objective.get_editor_property("enable_location_tracking"),
                "Prologue objective {} is not tracker-enabled".format(objective_id))
        actual_tag = str(unreal.GameplayTagLibrary.get_tag_name(
            objective.get_editor_property("tracking_location_tag")))
        require(actual_tag == expected_tag,
                "Prologue objective {} tracks '{}', expected '{}'".format(
                    objective_id, actual_tag, expected_tag))

    require(unreal.EditorLoadingAndSavingUtils.load_map(PROLOGUE_MAP_PATH),
            "Could not load {}".format(PROLOGUE_MAP_PATH))
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    locations = {}
    for actor in actors:
        if isinstance(actor, unreal.VHVQuestLocationVolume):
            locations.setdefault(str(actor.get_effective_location_id()), []).append(actor)
    instructor = next((actor for actor in actors
                       if actor.get_actor_label() == "HBCT_Intro_Instructor"), None)
    require(instructor is not None, "Main-map prologue Instructor is missing")
    instructor_target = next((actor for actor in actors
                              if actor.get_actor_label()
                              == "HBCT_Location_IntroInstructor"), None)
    # Runtime intentionally resolves these two objectives directly to the sole
    # persistent Instructor when the optional semantic volume is absent.
    instructor_target = instructor_target or instructor
    require(unreal.MathLibrary.vector_distance(
                instructor_target.get_actor_location(),
                instructor.get_actor_location()) <= 1.0,
            "Prologue Instructor tracking anchor must match the authored Instructor position")
    for objective_id in ("O01_MeetInstructor", "O04_ReportToInstructor"):
        require(str(unreal.GameplayTagLibrary.get_tag_name(
                    objectives[objective_id].get_editor_property("tracking_location_tag")))
                == "VHV.Location.HBCT.Intro.Instructor",
                "{} must resolve the Main-map Instructor target".format(objective_id))

    starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    require(starts, "No PlayerStart found in prologue map")
    start = starts[0].get_actor_location()
    require(len(locations.get("Market", [])) == 1,
            "Prologue market location must resolve exactly once")
    market = locations["Market"][0]
    require_nav_path(
        unreal.EditorLevelLibrary.get_editor_world(),
        start, market.get_actor_location(), "Prologue market")
    unreal.log("{} Both Instructor objectives resolve the authored Main-map Instructor target; metadata, anchors, and NavMesh route passed.".format(LOG))


def validate_q2_goal_board():
    arc = unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    quest = next((item for item in arc.get_editor_property("quests")
                  if str(item.get_editor_property("quest_id"))
                  == "Q_HBCT_02_GOAL_SETTING"), None)
    require(quest is not None, "Production arc has no Technique 2 quest")
    objectives = {str(item.get_editor_property("objective_id")): item
                  for item in quest.get_editor_property("objectives")}
    objective = objectives.get("O09_EvaluateGoals")
    require(objective is not None, "Technique 2 goal-board objective is missing")
    require(objective.get_editor_property("objective_type")
            == unreal.VHVQuestObjectiveType.CONVERSATION,
            "Goal-board objective is not an interaction-gated conversation")
    require(not objective.get_editor_property("auto_start"),
            "Goal-board conversation must wait for player interaction")
    participant = str(unreal.GameplayTagLibrary.get_tag_name(
        objective.get_editor_property("participant_tag")))
    require(participant == "VHV.Participant.Instructor",
            "Goal-board conversation does not target the Instructor")
    conversation = objective.get_editor_property("conversation")
    require(conversation is not None
            and conversation.get_name() == "DA_Conversation_HBCT_GoalSetting_GoodGoals",
            "Goal-board objective references the wrong conversation")
    entry_node_id = str(objective.get_editor_property("entry_node_id"))
    nodes = list(conversation.get_editor_property("conversation").get_editor_property("nodes"))
    entry_node = next((node for node in nodes
                       if str(node.get_editor_property("node_id")) == entry_node_id), None)
    require(entry_node_id == "GSGood_01" and entry_node is not None,
            "Goal-board conversation entry node is missing")
    entry_speaker = str(unreal.GameplayTagLibrary.get_tag_name(
        entry_node.get_editor_property("speaker_tag")))
    require(entry_speaker == "VHV.Participant.Instructor",
            "Goal-board conversation entry speaker is not the Instructor")
    expected = "VHV.Location.HBCT.GoalSetting.FinalInstructor"
    location = str(unreal.GameplayTagLibrary.get_tag_name(
        objective.get_editor_property("location_tag")))
    tracking = str(unreal.GameplayTagLibrary.get_tag_name(
        objective.get_editor_property("tracking_location_tag")))
    require(objective.get_editor_property("ensure_participant_present_at_location"),
            "Goal-board objective does not enforce Instructor placement")
    require(location == expected and tracking == expected,
            "Goal-board placement and navigation do not share the interaction anchor")

    require(unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH),
            "Could not load {}".format(MAP_PATH))
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    locations = [actor for actor in actors
                 if isinstance(actor, unreal.VHVQuestLocationVolume)
                 and str(actor.get_effective_location_id()) == "FinalInstructor"]
    require(len(locations) == 1,
            "Goal-board Instructor interaction anchor must resolve exactly once")
    interaction_anchor = locations[0]
    board_anchor = next((actor for actor in actors
                         if actor.get_actor_label()
                         == "HBCT_Location_GoalSetting_GoodGoalBoard"), None)
    require(board_anchor is not None, "Goal-board visual anchor is missing")
    require(unreal.MathLibrary.vector_distance2d(
                interaction_anchor.get_actor_location(),
                board_anchor.get_actor_location()) <= 1.0,
            "Goal-board visual and Instructor interaction anchors disagree in XY")

    instructors = []
    for actor in actors:
        try:
            participant = actor.get_quest_participant_component()
        except Exception:
            participant = None
        if participant and str(participant.get_effective_participant_id()) == "Instructor":
            instructors.append(actor)
    require(len(instructors) == 1,
            "Village must contain exactly one persistent Instructor participant")
    instructor = instructors[0]
    capsule = instructor.get_component_by_class(unreal.CapsuleComponent)
    require(capsule and capsule.get_collision_enabled()
            == unreal.CollisionEnabled.QUERY_AND_PHYSICS,
            "Instructor capsule collision is not enabled")
    interaction = instructor.get_npc_interaction_component()
    require(interaction and interaction.get_editor_property("interaction_enabled"),
            "Instructor interaction is disabled")

    starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    require(starts, "No PlayerStart found for goal-board NavMesh test")
    require_nav_path(
        unreal.EditorLevelLibrary.get_editor_world(),
        starts[0].get_actor_location(), interaction_anchor.get_actor_location(),
        "Technique 2 goal-board interaction")
    unreal.log("{} Technique 2 goal-board placement, collision, interaction, and NavMesh route passed.".format(LOG))


def run():
    validate_assets_and_input()
    validate_map_route()
    validate_prologue_tracking()
    validate_q2_goal_board()
    unreal.log("{} PASS".format(LOG))


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} FAIL:\n{}".format(LOG, traceback.format_exc()))
        raise
