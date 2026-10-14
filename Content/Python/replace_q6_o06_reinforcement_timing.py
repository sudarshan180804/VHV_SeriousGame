"""Install and validate the focused Q6 O06 reinforcement-timing replacement.

Run once in Unreal Editor 5.7 with Lvl_Village_Main already loaded. The script
never loads another map, never changes an existing actor transform, and saves
only after the complete O06 configuration validates.
"""

import unreal
import author_hbct_milestone_01 as hbct


MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
LEVEL_PATH = "/Game/VHV_Stuff/HBCT/Activities/DA_Level_HBCT"
ARC_PATH = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
AMBIENT_DIR = "/Game/VHV_Stuff/HBCT/Ambient"
QUEST_ID = "Q_HBCT_06_POSITIVE_REINFORCEMENT"
OBJECTIVE_ID = "O06_TestReinforcementTiming"
NEXT_OBJECTIVE_ID = "O07_OrderReinforcementTiming"
ACTIVITY_TAG = "VHV.Activity.HBCT.PositiveReinforcement.TimingDecision"
LOCATION = "VHV.Location.HBCT.PositiveReinforcement."
FLAG = "VHV.Story.Flag.HBCT.PositiveReinforcement."
FOLDER = "Village_Main/Q6_TestingPlane/O06_TestReinforcementTiming"
PREFIX = "HBCT_PR_"
_enum_return_diagnostic_logged = False

STAGE1_ASSET = "DA_Ambient_HBCT_PositiveReinforcement_Timing_BeforeWalk"
STAGE2_ASSET = "DA_Ambient_HBCT_PositiveReinforcement_Timing_ImmediateAfter"
STAGE3_ASSET = "DA_Ambient_HBCT_PositiveReinforcement_Timing_TwoDaysLater"
STAGE2_ACTOR = PREFIX + "Ambient_Timing_ImmediateAfter"
STAGE3_ACTOR = PREFIX + "Ambient_Timing_TwoDaysLater"
SEQUENCE_ACTOR = PREFIX + "TimeSkip_TimingLesson"


def struct(kind, **properties):
    value = kind()
    value.set_editor_properties(properties)
    return value


def tag(name):
    value = unreal.GameplayTag()
    if not value.import_text(name):
        raise RuntimeError("Gameplay tag is not registered: {}".format(name))
    return value


def tag_name(value):
    return str(unreal.GameplayTagLibrary.get_tag_name(value))


def actor_transform(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return (
        float(location.x), float(location.y), float(location.z),
        float(rotation.roll), float(rotation.pitch), float(rotation.yaw),
        float(scale.x), float(scale.y), float(scale.z))


def box_signature(actor):
    if not isinstance(actor, unreal.VHVQuestLocationVolume):
        return None
    component = actor.get_editor_property("box_component")
    extent = component.get_unscaled_box_extent()
    return (
        float(extent.x), float(extent.y), float(extent.z),
        str(component.get_collision_enabled()),
        str(component.get_collision_profile_name()),
        bool(component.get_editor_property("generate_overlap_events")))


def canonical_value(value, field_path):
    """Return stable serialized data for a non-container reflected value."""
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, unreal.GameplayTag):
        return {"gameplay_tag": tag_name(value)}
    if isinstance(value, unreal.Object):
        return {"object_path": value.get_path_name()}

    type_name = type(value).__name__
    if type_name in ("Name", "Text"):
        return str(value)

    # UE Python enums expose their serialized numeric value. Recording the type as
    # well prevents equal-valued members of different enums from comparing equal.
    enum_value = getattr(value, "value", None)
    if isinstance(enum_value, (int, str)):
        return {"enum_type": type_name, "value": enum_value}

    # Soft object/class properties normally resolve to UObject/Class instances.
    # Retain a field-level path fallback for an unloaded UE soft reference without
    # relying on its Python wrapper representation.
    get_path_name = getattr(value, "get_path_name", None)
    if callable(get_path_name):
        return {"object_path": get_path_name()}
    get_asset_path_name = getattr(value, "get_asset_path_name", None)
    if callable(get_asset_path_name):
        return {"object_path": str(get_asset_path_name())}

    raise RuntimeError("Unsupported protected field type at {}: {}".format(field_path, type_name))


def struct_fields(value, field_names, field_path):
    return {
        name: canonical_value(value.get_editor_property(name), "{}.{}".format(field_path, name))
        for name in field_names}


def enum_property_snapshot(value, native_property_name, field_path):
    global _enum_return_diagnostic_logged
    result = unreal.VHVAuthoringReferenceLibrary.read_serialized_enum_property(
        value.static_struct().get_path_name(), value.export_text(), native_property_name)
    if result is None:
        raise RuntimeError("Native enum reader failed at {}".format(field_path))
    if not isinstance(result, tuple) or len(result) != 3:
        raise RuntimeError(
            "Unexpected native enum reader result at {}: type={}, tuple={}, count={}".format(
                field_path, type(result).__name__, isinstance(result, tuple),
                len(result) if hasattr(result, "__len__") else "unavailable"))
    declared_type, numeric_value, error = result
    if not _enum_return_diagnostic_logged:
        unreal.log(
            "[Q6 O06 Enum Adapter] field={} | type={} | tuple=True | count=3 | "
            "outputs=(enum_type={}, numeric_value={}, error_length={})".format(
                field_path, type(result).__name__, str(declared_type),
                int(numeric_value), len(str(error))))
        _enum_return_diagnostic_logged = True
    if str(error):
        raise RuntimeError("Native enum reader returned an error at {}: {}".format(field_path, error))
    return {"enum_type": str(declared_type), "numeric_value": int(numeric_value)}


def story_condition_snapshot(condition, field_path):
    result = struct_fields(condition, (
        "state_tag", "state_id", "compare_value"), field_path)
    result["condition_type"] = enum_property_snapshot(
        condition, "ConditionType", field_path + ".condition_type")
    return result


def story_condition_set_snapshot(condition_set, field_path):
    result = {}
    result["match_mode"] = enum_property_snapshot(
        condition_set, "MatchMode", field_path + ".match_mode")
    result["conditions"] = [
        story_condition_snapshot(item, "{}.conditions[{}]".format(field_path, index))
        for index, item in enumerate(condition_set.get_editor_property("conditions"))]
    return result


def story_effect_snapshot(effect, field_path):
    result = struct_fields(effect, ("state_tag", "state_id", "value"), field_path)
    result["effect_type"] = enum_property_snapshot(
        effect, "EffectType", field_path + ".effect_type")
    return result


def stinger_snapshot(stinger, field_path):
    return struct_fields(stinger, (
        "label", "title", "subtitle", "intro_sound", "hold_duration"), field_path)


def move_request_snapshot(move, field_path):
    return struct_fields(move, (
        "participant_tag", "destination_location_tag",
        "face_destination_rotation", "move_speed_override"), field_path)


def move_stage_snapshot(stage, field_path):
    result = struct_fields(stage, ("ambient_conversation_receiver_tag",), field_path)
    result["moves"] = [
        move_request_snapshot(item, "{}.moves[{}]".format(field_path, index))
        for index, item in enumerate(stage.get_editor_property("moves"))]
    return result


def contextual_conversation_snapshot(conversation, field_path):
    result = struct_fields(conversation, (
        "participant_tag", "conversation", "entry_node_id"), field_path)
    result["availability_conditions"] = story_condition_set_snapshot(
        conversation.get_editor_property("availability_conditions"),
        field_path + ".availability_conditions")
    return result


def tracking_destination_snapshot(destination, field_path):
    result = struct_fields(destination, ("destination_location_tag",), field_path)
    result["completion_conditions"] = story_condition_set_snapshot(
        destination.get_editor_property("completion_conditions"),
        field_path + ".completion_conditions")
    return result


def objective_snapshot(objective, field_path):
    result = struct_fields(objective, (
        "objective_id", "objective_text", "participant_tag", "location_tag",
        "ensure_participant_present_at_location", "prepare_activation_npc_readiness_before_exposure",
        "required_participant_spawn_class", "enable_location_tracking", "tracking_location_tag",
        "custom_event_tag", "target_id", "activity_tag", "activity_id", "conversation", "entry_node_id",
        "auto_start", "allow_participant_activation", "npc_participant_tag", "npc_participant_id",
        "npc_behavior_target_tag", "npc_target_id", "npc_wait_duration",
        "npc_action_tag", "npc_action_id", "world_action_receiver_tag", "world_action_receiver_id",
        "world_action_tag", "world_action_id", "complete_after_npc_move_stages"), field_path)
    for python_name, native_name in (
            ("objective_type", "ObjectiveType"),
            ("npc_command_type", "NPCCommandType"),
            ("world_action_start_policy", "WorldActionStartPolicy"),
            ("npc_travel_mode", "NPCTravelMode")):
        result[python_name] = enum_property_snapshot(
            objective, native_name, "{}.{}".format(field_path, python_name))
    result["tracking_destinations"] = [
        tracking_destination_snapshot(item, "{}.tracking_destinations[{}]".format(field_path, index))
        for index, item in enumerate(objective.get_editor_property("tracking_destinations"))]
    result["contextual_conversations"] = [
        contextual_conversation_snapshot(item, "{}.contextual_conversations[{}]".format(field_path, index))
        for index, item in enumerate(objective.get_editor_property("contextual_conversations"))]
    result["activation_conditions"] = story_condition_set_snapshot(
        objective.get_editor_property("activation_conditions"), field_path + ".activation_conditions")
    result["completion_conditions"] = story_condition_set_snapshot(
        objective.get_editor_property("completion_conditions"), field_path + ".completion_conditions")
    result["completion_effects"] = [
        story_effect_snapshot(item, "{}.completion_effects[{}]".format(field_path, index))
        for index, item in enumerate(objective.get_editor_property("completion_effects"))]
    result["activation_stinger"] = stinger_snapshot(
        objective.get_editor_property("activation_stinger"), field_path + ".activation_stinger")
    result["completion_npc_moves"] = [
        move_request_snapshot(item, "{}.completion_npc_moves[{}]".format(field_path, index))
        for index, item in enumerate(objective.get_editor_property("completion_npc_moves"))]
    result["npc_move_stages"] = [
        move_stage_snapshot(item, "{}.npc_move_stages[{}]".format(field_path, index))
        for index, item in enumerate(objective.get_editor_property("npc_move_stages"))]
    result["activation_npc_readiness"] = [
        move_request_snapshot(item, "{}.activation_npc_readiness[{}]".format(field_path, index))
        for index, item in enumerate(objective.get_editor_property("activation_npc_readiness"))]
    result["npc_move_stage_activation_conditions"] = story_condition_set_snapshot(
        objective.get_editor_property("npc_move_stage_activation_conditions"),
        field_path + ".npc_move_stage_activation_conditions")
    return result


def quest_metadata_snapshot(quest, field_path):
    result = struct_fields(quest, (
        "quest_id", "quest_title", "quest_description",
        "auto_track", "auto_start_next_quest"), field_path)
    result["category"] = enum_property_snapshot(quest, "Category", field_path + ".category")
    result["start_stinger"] = stinger_snapshot(
        quest.get_editor_property("start_stinger"), field_path + ".start_stinger")
    result["completion_stinger"] = stinger_snapshot(
        quest.get_editor_property("completion_stinger"), field_path + ".completion_stinger")
    return result


def quest_snapshot(quest, field_path):
    result = quest_metadata_snapshot(quest, field_path)
    result["objectives"] = [
        objective_snapshot(item, "{}.objectives[{}]".format(field_path, index))
        for index, item in enumerate(quest.get_editor_property("objectives"))]
    return result


def first_difference(before, after, field_path):
    if type(before) is not type(after):
        return "{} type changed from {} to {}".format(
            field_path, type(before).__name__, type(after).__name__)
    if isinstance(before, dict):
        if list(before.keys()) != list(after.keys()):
            return "{} field set/order changed".format(field_path)
        for key in before:
            difference = first_difference(before[key], after[key], "{}.{}".format(field_path, key))
            if difference:
                return difference
        return None
    if isinstance(before, list):
        if len(before) != len(after):
            return "{} length changed from {} to {}".format(field_path, len(before), len(after))
        for index, (before_item, after_item) in enumerate(zip(before, after)):
            difference = first_difference(
                before_item, after_item, "{}[{}]".format(field_path, index))
            if difference:
                return difference
        return None
    if before != after:
        return "{} changed from {} to {}".format(field_path, before, after)
    return None


def require_one_by_label(labels, label):
    matches = [actor for actor in labels.values() if actor.get_actor_label() == label]
    if len(matches) != 1:
        raise RuntimeError("Expected exactly one actor '{}'; found {}".format(label, len(matches)))
    return matches[0]


def participant_id(actor):
    if not hasattr(actor, "get_quest_participant_component"):
        return ""
    component = actor.get_quest_participant_component()
    return str(component.get_effective_participant_id()) if component else ""


def require_participant(actors, identity):
    matches = [actor for actor in actors if participant_id(actor) == identity]
    if len(matches) != 1:
        raise RuntimeError("Expected one participant '{}'; found {}".format(identity, len(matches)))
    return matches[0]


def ambient_asset(name, conversation_id, lines, completion_flag):
    asset = hbct.load_or_create(AMBIENT_DIR, name, unreal.VHVAmbientConversationData)
    asset.set_editor_properties({
        "conversation_id": conversation_id,
        "participants": [
            hbct.ambient_participant("Instructor", "Instructor"),
            hbct.ambient_participant("Demo", "Villager")],
        "lines": [hbct.ambient_line(slot, text, duration) for slot, text, duration in lines],
        "show_speaker_name": False,
        "completion_effects": [hbct.effect(completion_flag)]})
    return asset


def make_binding(slot, npc):
    return struct(unreal.VHVAmbientParticipantBinding, slot_id=slot, npc=npc)


def configure_ambient_actor(actor, data, instructor, villager, explicit_trigger):
    actor.set_editor_properties({
        "conversation_data": data,
        "participants": [make_binding("Instructor", instructor), make_binding("Demo", villager)],
        "auto_start_on_begin_play": False,
        "requires_explicit_trigger": explicit_trigger,
        "play_once": True,
        "preserve_participant_behavior": True,
        "player_leave_policy": unreal.VHVAmbientConversationLeavePolicy.CONTINUE,
        "observation_radius": 2400.0})


def spawn_at_existing_transform(actor_subsystem, kind, label, reference):
    actor = actor_subsystem.spawn_actor_from_class(
        kind, reference.get_actor_location(), reference.get_actor_rotation())
    if not actor:
        raise RuntimeError("Could not spawn {}".format(label))
    actor.set_actor_label(label)
    actor.set_folder_path(FOLDER)
    actor.set_actor_scale3d(reference.get_actor_scale3d())
    return actor


def update_level_activity(level):
    day = level.get_editor_property("day_data")
    topics = list(day.get_editor_property("topics"))
    topic_index = next((index for index, topic in enumerate(topics)
                        if str(topic.get_editor_property("topic_id")) == "HBCT_M01_POSITIVE_REINFORCEMENT"), None)
    if topic_index is None:
        raise RuntimeError("Positive Reinforcement textbook topic is missing")

    options = [
        struct(unreal.QuestionOption,
               option_text="Before he starts, congratulating him for finishing.",
               is_correct=False,
               response_feedback="Encouragement beforehand can help him start, but it cannot recognize an achievement he hasn't completed."),
        struct(unreal.QuestionOption,
               option_text="Immediately afterward, acknowledging his completed walk.",
               is_correct=True,
               response_feedback="Exactly. Timely, specific encouragement helps connect the positive response to the completed behavior."),
        struct(unreal.QuestionOption,
               option_text="Two days later, without referring to the specific achievement.",
               is_correct=False,
               response_feedback="Delayed encouragement can still help, but giving specific feedback sooner makes the connection clearer.")]
    question = struct(
        unreal.QuestionData,
        question_id="HBCT_PR_TIMING_DECISION",
        question_text="Uncle Chai has just completed his 30-minute walk. When would your encouragement best reinforce that achievement?",
        options=options)
    attempt_policy = struct(
        unreal.TextbookAttemptPolicy,
        enabled=True,
        max_attempts=2,
        retry_until_correct=True,
        first_incorrect_hint="Choose the response that most clearly follows and names the completed behavior.",
        final_incorrect_explanation="Prompt, specific recognition makes the behavior-consequence link clearest.")
    activity = hbct.activity(
        ACTIVITY_TAG,
        "When Does Encouragement Work Best?",
        unreal.TextbookActivityType.SINGLE_CHOICE,
        question.get_editor_property("question_text"),
        unreal.TeachingContent(),
        question=question,
        correct_feedback=options[1].get_editor_property("response_feedback"),
        incorrect_feedback="Review when the walk was completed and how specifically the response refers to it.",
        partial_feedback="",
        require_correct_answer_to_advance=True,
        attempt_policy=attempt_policy,
        success_effects=[])

    activities = list(topics[topic_index].get_editor_property("activities"))
    activities = [item for item in activities if tag_name(item.get_editor_property("activity_tag")) != ACTIVITY_TAG]
    ordering_index = next((index for index, item in enumerate(activities)
                           if tag_name(item.get_editor_property("activity_tag")) ==
                           "VHV.Activity.HBCT.PositiveReinforcement.TimingOrdering"), None)
    if ordering_index is None:
        raise RuntimeError("Existing O07 TimingOrdering activity is missing")
    activities.insert(ordering_index, activity)
    topics[topic_index].set_editor_property("activities", activities)
    day.set_editor_property("topics", topics)
    level.set_editor_property("day_data", day)
    return [tag_name(item.get_editor_property("activity_tag")) for item in activities]


def update_o06_objective(arc):
    quests = list(arc.get_editor_property("quests"))
    quest_index = next((index for index, quest in enumerate(quests)
                        if str(quest.get_editor_property("quest_id")) == QUEST_ID), None)
    if quest_index is None:
        raise RuntimeError("Quest 6 definition is missing")
    objective_ids = [str(item.get_editor_property("objective_id"))
                     for item in quests[quest_index].get_editor_property("objectives")]
    expected_ids = [
        "O01_NoticeSuccess", "O02_UnderstandReinforcement", "O03_ExploreHealthFair",
        "O04_MatchReinforcementTypes", "O05_ChooseMeaningfulReinforcement",
        OBJECTIVE_ID, NEXT_OBJECTIVE_ID, "O08_CheckInWithChai", "O09_ReinforceChaiProgress",
        "O10_ReinforceExercise", "O11_ChooseSelfReward", "O12_JoinWalkingChallenge",
        "O13_SeveralWeeksLater", "O14_SeeFeedbackReinforcement", "O15_FinalDebrief"]
    if objective_ids != expected_ids:
        raise RuntimeError("Quest 6 objective order changed; refusing O06 replacement")

    objectives = list(quests[quest_index].get_editor_property("objectives"))
    index = objective_ids.index(OBJECTIVE_ID)
    o06 = objectives[index]
    o06.set_editor_properties({
        "objective_text": "Experience three timings, then choose when encouragement best reinforces a completed walk.",
        "objective_type": unreal.VHVQuestObjectiveType.LEARNING_ACTIVITY,
        "activity_tag": tag(ACTIVITY_TAG),
        "participant_tag": tag("VHV.Participant.Instructor"),
        "auto_start": False,
        "allow_participant_activation": False,
        "prepare_activation_npc_readiness_before_exposure": True,
        "activation_npc_readiness": [
            struct(unreal.VHVQuestNPCMoveRequest,
                   participant_tag=tag("VHV.Participant.Instructor"),
                   destination_location_tag=tag(LOCATION + "PRTimingTooLate"),
                   face_destination_rotation=True,
                   move_speed_override=0.0),
            struct(unreal.VHVQuestNPCMoveRequest,
                   participant_tag=tag("VHV.Participant.PRDemoVillager"),
                   destination_location_tag=tag(LOCATION + "PRTimingImmediate"),
                   face_destination_rotation=True,
                   move_speed_override=0.0)],
        "world_action_receiver_tag": unreal.GameplayTag(),
        "world_action_tag": unreal.GameplayTag()})
    objectives[index] = o06
    quests[quest_index].set_editor_property("objectives", objectives)
    arc.set_editor_property("quests", quests)
    return quests, objective_ids


def run():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world and world.get_path_name().split('.')[0].startswith("/Temp/Untitled"):
        if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
            raise RuntimeError("Could not load the authorized main map: {}".format(MAP_PATH))
        world = unreal.EditorLevelLibrary.get_editor_world()
    if not world or world.get_path_name().split('.')[0] != MAP_PATH:
        raise RuntimeError("Loaded map must be exactly {}; got {}".format(
            MAP_PATH, world.get_path_name() if world else "None"))

    existing_actors = list(actor_subsystem.get_all_level_actors())
    existing_by_path = {actor.get_path_name(): actor for actor in existing_actors}
    labels = {actor.get_path_name(): actor for actor in existing_actors}
    q6_count = len([actor for actor in existing_actors if actor.get_actor_label().startswith(PREFIX)])
    if q6_count != 71:
        raise RuntimeError("Expected validated 71-actor Q6 layout before replacement; found {}".format(q6_count))
    for reserved in (STAGE2_ACTOR, STAGE3_ACTOR, SEQUENCE_ACTOR):
        if any(actor.get_actor_label() == reserved for actor in existing_actors):
            raise RuntimeError("Replacement actor already exists: {}".format(reserved))

    transform_snapshot = {path: actor_transform(actor) for path, actor in existing_by_path.items()}
    box_snapshot = {path: box_signature(actor) for path, actor in existing_by_path.items()
                    if box_signature(actor) is not None}

    level = unreal.load_asset(LEVEL_PATH)
    arc = unreal.load_asset(ARC_PATH)
    if not level or not arc:
        raise RuntimeError("Required quest/textbook assets are missing")
    arc_quests_snapshot = list(arc.get_editor_property("quests"))
    level_day_snapshot = level.get_editor_property("day_data")
    quest_order_snapshot = [str(quest.get_editor_property("quest_id")) for quest in arc_quests_snapshot]
    protected_quest_snapshots = {
        str(quest.get_editor_property("quest_id")): quest_snapshot(
            quest, "quest[{}]".format(str(quest.get_editor_property("quest_id"))))
        for quest in arc_quests_snapshot
        if str(quest.get_editor_property("quest_id")) != QUEST_ID}
    q6_before = next(quest for quest in arc_quests_snapshot
                     if str(quest.get_editor_property("quest_id")) == QUEST_ID)
    q6_metadata_before = quest_metadata_snapshot(q6_before, "quest[{}]".format(QUEST_ID))
    q6_objective_snapshots = {
        str(objective.get_editor_property("objective_id")): objective_snapshot(
            objective, "quest[{}].objective[{}]".format(
                QUEST_ID, str(objective.get_editor_property("objective_id"))))
        for objective in q6_before.get_editor_property("objectives")
        if str(objective.get_editor_property("objective_id")) != OBJECTIVE_ID}

    timing_actor = require_one_by_label(labels, PREFIX + "Ambient_Timing")
    timing_trigger = require_one_by_label(labels, PREFIX + "Trigger_TimingArea")
    early_text = require_one_by_label(labels, PREFIX + "Text_TimingEarly")
    immediate_text = require_one_by_label(labels, PREFIX + "Text_TimingImmediate")
    late_text = require_one_by_label(labels, PREFIX + "Text_TimingLate")
    villager = require_participant(existing_actors, "PRDemoVillager")
    instructor = require_participant(existing_actors, "Instructor")

    old_timing_properties = {
        "conversation_data": timing_actor.get_editor_property("conversation_data"),
        "participants": list(timing_actor.get_editor_property("participants")),
        "requires_explicit_trigger": timing_actor.get_editor_property("requires_explicit_trigger")}
    old_trigger_properties = {
        "track_world_action_as_objective": timing_trigger.get_editor_property("track_world_action_as_objective"),
        "require_player_interaction": timing_trigger.get_editor_property("require_player_interaction"),
        "interaction_prompt": timing_trigger.get_editor_property("interaction_prompt")}
    old_text = {}
    for actor in (early_text, immediate_text, late_text):
        component = actor.get_component_by_class(unreal.TextRenderComponent)
        old_text[actor.get_path_name()] = component.get_editor_property("text")

    new_actors = []
    new_assets = []
    try:
        before = ambient_asset(STAGE1_ASSET, "HBCT_PR_Timing_BeforeWalk", [
            ("Instructor", "Let's see how the timing of encouragement can affect a walking habit.", 2.8),
            ("Instructor", "Watch what happens when praise comes before the action, right after it, or much later.", 3.1),
            ("Instructor", "Great job completing your 30-minute walk!", 2.2),
            ("Demo", "But I haven't started walking yet.", 2.0),
            ("Instructor", "That may encourage someone to begin, but it doesn't recognize a walk they've completed.", 3.2)],
            FLAG + "PRTooEarlySeen")
        immediate = ambient_asset(STAGE2_ASSET, "HBCT_PR_Timing_ImmediateAfter", [
            ("Demo", "I did it! I finished my 30-minute walk.", 2.2),
            ("Instructor", "Excellent work! You completed all 30 minutes today.", 2.7),
            ("Demo", "Thanks! That makes me want to keep going.", 2.3),
            ("Instructor", "The feedback follows the achievement, so the connection is clear.", 3.0)],
            FLAG + "PRImmediateSeen")
        delayed = ambient_asset(STAGE3_ASSET, "HBCT_PR_Timing_TwoDaysLater", [
            ("Instructor", "Good job on that walk you completed two days ago.", 2.6),
            ("Demo", "Thanks! It's a little hard to remember which walk you mean.", 2.8),
            ("Instructor", "The praise is still kind, but the delay makes the connection to that specific achievement less clear.", 3.4)],
            FLAG + "PRTooLateSeen")
        new_assets.extend([before, immediate, delayed])

        configure_ambient_actor(timing_actor, before, instructor, villager, True)
        timing_trigger.set_editor_properties({
            "track_world_action_as_objective": False,
            "require_player_interaction": True,
            "interaction_prompt": "Begin Timing Demonstration"})

        stage2_actor = spawn_at_existing_transform(
            actor_subsystem, unreal.VHVAmbientConversationActor, STAGE2_ACTOR, timing_actor)
        stage3_actor = spawn_at_existing_transform(
            actor_subsystem, unreal.VHVAmbientConversationActor, STAGE3_ACTOR, timing_actor)
        new_actors.extend([stage2_actor, stage3_actor])
        configure_ambient_actor(stage2_actor, immediate, instructor, villager, False)
        configure_ambient_actor(stage3_actor, delayed, instructor, villager, False)

        early_text.get_component_by_class(unreal.TextRenderComponent).set_editor_property("text", "BEFORE THE WALK")
        immediate_text.get_component_by_class(unreal.TextRenderComponent).set_editor_property("text", "IMMEDIATELY AFTER THE WALK")
        late_text.get_component_by_class(unreal.TextRenderComponent).set_editor_property("text", "TWO DAYS LATER")

        sequence = spawn_at_existing_transform(
            actor_subsystem, unreal.VHVTimeSkipSequence, SEQUENCE_ACTOR, timing_actor)
        new_actors.append(sequence)
        sequence.set_editor_properties({
            "required_quest_id": QUEST_ID,
            "required_objective_id": OBJECTIVE_ID,
            "advance_on_objective_completion": False,
            "transition_on_next_objective_activation": False,
            "require_navigation_projection": False,
            "fade_in_duration": 0.40,
            "text_hold_duration": 1.10,
            "fade_out_duration": 0.45,
            "transition_fail_safe_duration": 3.0,
            "request_objective_activation_on_sequence_completed": True,
            "keep_final_stage_after_objective": False,
            "stop_observations_when_deactivated": True,
            "sequence_actors": [villager],
            "stages": [
                struct(unreal.VHVTimeSkipStage,
                       stage_id="BeforeWalk", transition_text="After the walk...",
                       observation_scene=timing_actor, stage_actors=[early_text],
                       legacy_trigger=timing_trigger,
                       auto_start_observation_after_transition=True,
                       post_transition_observation_delay=0.0),
                struct(unreal.VHVTimeSkipStage,
                       stage_id="ImmediateAfter", transition_text="Two days later...",
                       observation_scene=stage2_actor, stage_actors=[immediate_text],
                       auto_start_observation_after_transition=True,
                       post_transition_observation_delay=1.75),
                struct(unreal.VHVTimeSkipStage,
                       stage_id="TwoDaysLater", transition_text="",
                       observation_scene=stage3_actor, stage_actors=[late_text],
                       auto_start_observation_after_transition=True,
                       post_transition_observation_delay=1.75)]})

        activity_ids = update_level_activity(level)
        quests, objective_ids = update_o06_objective(arc)

        if activity_ids.count(ACTIVITY_TAG) != 1:
            raise RuntimeError("Timing decision activity was not authored exactly once")
        if objective_ids[5] != OBJECTIVE_ID or objective_ids[6] != NEXT_OBJECTIVE_ID:
            raise RuntimeError("O06 -> O07 objective ordering changed")
        q6 = next(quest for quest in quests if str(quest.get_editor_property("quest_id")) == QUEST_ID)
        objectives = list(q6.get_editor_property("objectives"))
        o06 = objectives[5]
        arc_quests_after = list(arc.get_editor_property("quests"))
        quest_order_after = [str(q.get_editor_property("quest_id")) for q in arc_quests_after]
        if quest_order_after != quest_order_snapshot:
            raise RuntimeError("Protected quest arc membership/order changed")
        if (o06.get_editor_property("objective_type") != unreal.VHVQuestObjectiveType.LEARNING_ACTIVITY
                or tag_name(o06.get_editor_property("activity_tag")) != ACTIVITY_TAG
                or o06.get_editor_property("auto_start")
                or o06.get_editor_property("allow_participant_activation")
                or not o06.get_editor_property("prepare_activation_npc_readiness_before_exposure")):
            raise RuntimeError("O06 did not become the gated TimingDecision learning objective")
        q6_metadata_after = quest_metadata_snapshot(q6, "quest[{}]".format(QUEST_ID))
        difference = first_difference(
            q6_metadata_before, q6_metadata_after, "quest[{}]".format(QUEST_ID))
        if difference:
            raise RuntimeError("Protected Q6 quest metadata changed: {}".format(difference))
        for protected_quest in arc_quests_after:
            protected_id = str(protected_quest.get_editor_property("quest_id"))
            if protected_id in protected_quest_snapshots:
                protected_after = quest_snapshot(
                    protected_quest, "quest[{}]".format(protected_id))
                difference = first_difference(
                    protected_quest_snapshots[protected_id], protected_after,
                    "quest[{}]".format(protected_id))
                if difference:
                    raise RuntimeError("Protected quest changed: {} ({})".format(protected_id, difference))
        for objective in objectives:
            protected_id = str(objective.get_editor_property("objective_id"))
            if protected_id in q6_objective_snapshots:
                protected_after = objective_snapshot(
                    objective, "quest[{}].objective[{}]".format(QUEST_ID, protected_id))
                difference = first_difference(
                    q6_objective_snapshots[protected_id], protected_after,
                    "quest[{}].objective[{}]".format(QUEST_ID, protected_id))
                if difference:
                    raise RuntimeError("Protected Q6 objective changed: {} ({})".format(
                        protected_id, difference))
        if str(objectives[6].get_editor_property("objective_id")) != NEXT_OBJECTIVE_ID:
            raise RuntimeError("O07 changed during O06 replacement")

        current_actors = list(actor_subsystem.get_all_level_actors())
        current_by_path = {actor.get_path_name(): actor for actor in current_actors}
        for path, expected in transform_snapshot.items():
            if path not in current_by_path or actor_transform(current_by_path[path]) != expected:
                raise RuntimeError("Existing actor transform changed: {}".format(path))
        for path, expected in box_snapshot.items():
            if path not in current_by_path or box_signature(current_by_path[path]) != expected:
                raise RuntimeError("Existing trigger geometry/collision changed: {}".format(path))
        if len(current_actors) != len(existing_actors) + 3:
            raise RuntimeError("Unexpected actor create/delete count")

        # Static clearance check for the runtime staging pair. The existing Q6 layout
        # validation already proved the station footprint rests on the Plane.
        too_late = require_one_by_label(labels, PREFIX + "Location_PRTimingTooLate")
        immediate_anchor = require_one_by_label(labels, PREFIX + "Location_PRTimingImmediate")
        delta = too_late.get_actor_location() - immediate_anchor.get_actor_location()
        separation = (float(delta.x) ** 2 + float(delta.y) ** 2) ** 0.5
        if separation < 120.0 or separation > 450.0:
            raise RuntimeError("O06 staging anchors are not a safe conversational pair: {:.1f} cm".format(separation))

        for asset in new_assets + [level, arc]:
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError("Could not save asset {}".format(asset.get_path_name()))
        if not level_subsystem.save_current_level():
            raise RuntimeError("Could not save Lvl_Village_Main")

        unreal.log("[Q6 O06 Replacement] PASS | ExistingTransformsChanged=0 | TriggerExtentsChanged=0 | "
                   "Stages=3 | Choice={} | O07={} | NewActors=3 | MainMapSaved=Yes".format(
                       ACTIVITY_TAG, NEXT_OBJECTIVE_ID))
        unreal.log("[Q6 O06 Replacement] Runtime staging anchors: Instructor=PRTimingTooLate, "
                   "Demo=PRTimingImmediate, separation={:.1f}cm; player arrival=current position.".format(separation))
    except Exception:
        # Restore all modified existing-object properties in memory; no save occurs on failure.
        timing_actor.set_editor_properties(old_timing_properties)
        timing_trigger.set_editor_properties(old_trigger_properties)
        for actor in (early_text, immediate_text, late_text):
            actor.get_component_by_class(unreal.TextRenderComponent).set_editor_property(
                "text", old_text[actor.get_path_name()])
        for actor in reversed(new_actors):
            if actor:
                actor_subsystem.destroy_actor(actor)
        arc.set_editor_property("quests", arc_quests_snapshot)
        level.set_editor_property("day_data", level_day_snapshot)
        unreal.log_error("[Q6 O06 Replacement] FAILED. Existing map actors restored in memory; map not saved.")
        unreal.log_error("[Q6 O06 Replacement] Deterministic field snapshots retained: protected_quests={}, "
                         "protected_q6_objectives={}".format(
                             len(protected_quest_snapshots), len(q6_objective_snapshots)))
        raise


if __name__ == "__main__":
    run()
