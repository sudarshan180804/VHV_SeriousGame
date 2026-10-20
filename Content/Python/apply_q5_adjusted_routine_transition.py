"""Apply and validate the scoped Q5 O13 -> O14 cinematic/conversation correction."""

import json
import os
import unreal

import author_hbct_milestone_01 as hbct
import author_hbct_quest05_self_monitoring as q5_author


MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
ARC = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
LEVEL = "/Game/VHV_Stuff/HBCT/Activities/DA_Level_HBCT"
DIALOGUE_DIR = "/Game/VHV_Stuff/HBCT/Dialogue"
CONVERSATION_NAME = "DA_Conversation_HBCT_SelfMonitoring_AdjustedWalk"
QUEST_ID = "Q_HBCT_05_SELF_MONITORING"
SOURCE_OBJECTIVE = "O13_AdjustThePlan"
TARGET_OBJECTIVE = "O14_PracticeAdjustedRoutine"
COMPLETION_FLAG = "VHV.Story.Flag.HBCT.SelfMonitoring.SMAdjustedWalkConversationComplete"
ROUTE_COMPLETE_FLAG = "VHV.Story.Flag.HBCT.SelfMonitoring.SMAdjustedWalkRouteComplete"
RECORDED_FLAG = "VHV.Story.Flag.HBCT.SelfMonitoring.SMAdjustedWalkRecordedAccurately"
REFLECTION_COMPLETE_FLAG = "VHV.Story.Flag.HBCT.SelfMonitoring.SMAdjustedRoutineReflectionComplete"
SEQUENCE_LABEL = "VHV_Q5_TimeSkip_ThenLater_AdjustedRoutine"
DISPLAY_CONTROLLER_LABEL = "VHV_Q5_ObjectiveDisplayVisibility"


def actor_transform(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return (
        float(location.x), float(location.y), float(location.z),
        float(rotation.pitch), float(rotation.yaw), float(rotation.roll),
        float(scale.x), float(scale.y), float(scale.z),
    )


def gameplay_tag(name):
    return hbct.tag(name)


def flag_condition(name, is_set):
    value = unreal.VHVStoryCondition()
    value.set_editor_property(
        "condition_type",
        unreal.VHVStoryConditionType.FLAG_SET if is_set else unreal.VHVStoryConditionType.FLAG_NOT_SET)
    value.set_editor_property("state_tag", gameplay_tag(name))
    return value


def condition_set(name, is_set=True):
    value = unreal.VHVStoryConditionSet()
    value.set_editor_property("match_mode", unreal.VHVStoryConditionMatch.ALL)
    value.set_editor_property("conditions", [flag_condition(name, is_set)])
    return value


def conditions(*items):
    value = unreal.VHVStoryConditionSet()
    value.set_editor_property("match_mode", unreal.VHVStoryConditionMatch.ALL)
    value.set_editor_property("conditions", [
        flag_condition(name, is_set) for name, is_set in items])
    return value


def npc_move(participant, destination):
    value = unreal.VHVQuestNPCMoveRequest()
    value.set_editor_property("participant_tag", gameplay_tag(participant))
    value.set_editor_property("destination_location_tag", gameplay_tag(destination))
    value.set_editor_property("face_destination_rotation", True)
    value.set_editor_property("move_speed_override", 0.0)
    return value


def exact_by_label(actors, label):
    matches = [actor for actor in actors if actor.get_actor_label() == label]
    if len(matches) != 1:
        raise RuntimeError("Expected one actor labelled '{}'; found {}".format(label, len(matches)))
    return matches[0]


def participant_id(actor):
    if not hasattr(actor, "get_quest_participant_component"):
        return ""
    component = actor.get_quest_participant_component()
    return str(component.get_effective_participant_id()) if component else ""


def exact_participant(actors, participant):
    matches = [actor for actor in actors if participant_id(actor) == participant]
    if len(matches) != 1:
        raise RuntimeError("Expected one participant '{}'; found {}".format(
            participant, [actor.get_actor_label() for actor in matches]))
    return matches[0]


def story_effect_fingerprint(effect):
    return (
        str(effect.get_editor_property("effect_type")),
        str(unreal.GameplayTagLibrary.get_tag_name(effect.get_editor_property("state_tag"))),
        str(effect.get_editor_property("state_id")),
        int(effect.get_editor_property("value")),
    )


def build_conversation():
    asset = hbct.load_or_create(DIALOGUE_DIR, CONVERSATION_NAME, unreal.VHVConversationDataAsset)
    learn = hbct.learning_node
    nodes = [
        hbct.text_node("SMAdjustedWalk_01", "Uncle Chai",
            "I planned to walk for 30 minutes today, but work at the market took longer than expected.",
            "SMAdjustedWalk_02", "VHV.Participant.UncleChai"),
        hbct.text_node("SMAdjustedWalk_02", "Uncle Chai",
            "I only have about 10 minutes free now. Normally, I'd just skip walking altogether.",
            "SMAdjustedWalk_03", "VHV.Participant.UncleChai"),
        hbct.text_node("SMAdjustedWalk_03", "Player",
            "Let's look at your walking record. We noticed this happens often on busy market days.",
            "SMAdjustedWalk_04"),
        hbct.text_node("SMAdjustedWalk_04", "Uncle Chai",
            "You're right. I keep missing the same kind of day. But what should I do when I don't have enough time?",
            "SMAdjustedWalk_Decision", "VHV.Participant.UncleChai"),
        learn("SMAdjustedWalk_Decision",
            "VHV.Activity.HBCT.SelfMonitoring.AdjustedRoutineDecision",
            "SMAdjustedWalk_05", q5_author.TOPIC_ID),
        hbct.text_node("SMAdjustedWalk_05", "Player",
            "Let's use the time you have. A shorter walk is better than abandoning the routine, and we'll record exactly what you complete.",
            "SMAdjustedWalk_06"),
        hbct.text_node("SMAdjustedWalk_06", "Uncle Chai",
            "That sounds manageable. I can walk for 10 minutes now, then plan my next full session.",
            speaker_tag="VHV.Participant.UncleChai",
            completion_effects=[hbct.effect(COMPLETION_FLAG)]),
        hbct.text_node("SMAdjustedWalk_RecordIntro", "Uncle Chai",
            "I managed a shorter walk today. How should we record it?",
            "SMAdjustedWalk_RecordDecision", "VHV.Participant.UncleChai"),
        learn("SMAdjustedWalk_RecordDecision",
            "VHV.Activity.HBCT.SelfMonitoring.AdjustedWalkRecording",
            "SMAdjustedWalk_07", q5_author.TOPIC_ID),
        hbct.text_node("SMAdjustedWalk_07", "Uncle Chai",
            "Now I understand. I did walk today, but it wasn't the full 30 minutes. I should record what really happened.",
            "SMAdjustedWalk_08", "VHV.Participant.UncleChai"),
        hbct.text_node("SMAdjustedWalk_08", "Player",
            "Exactly. Your record isn't a score or a punishment. It shows what works and what needs adjusting.",
            "SMAdjustedWalk_09"),
        hbct.text_node("SMAdjustedWalk_09", "Uncle Chai",
            "On busy market days, I can plan a different time or a shorter walk when necessary. Then I can review my record and plan the next session.",
            "SMAdjustedWalk_10", "VHV.Participant.UncleChai"),
        hbct.text_node("SMAdjustedWalk_10", "Player",
            "That's self-monitoring. We observe, record, compare with the goal, and use what we learn to adjust the plan.",
            "SMAdjustedWalk_11"),
        hbct.text_node("SMAdjustedWalk_11", "Uncle Chai",
            "So instead of guessing or giving up, I can make decisions based on my own progress.",
            "SMAdjustedWalk_Summary", "VHV.Participant.UncleChai"),
        q5_author.learning_with_effect(
            "SMAdjustedWalk_Summary",
            "VHV.Activity.HBCT.SelfMonitoring.AdjustedRoutineSummary",
            REFLECTION_COMPLETE_FLAG),
    ]
    hbct.set_conversation(asset, "HBCT_SelfMonitoring_AdjustedWalkConversation", nodes)
    conversation = asset.get_editor_property("conversation")
    conversation.set_editor_property("use_world_space_speech_bubbles", True)
    asset.set_editor_property("conversation", conversation)
    asset.modify()
    return asset


def update_activities():
    level = unreal.EditorAssetLibrary.load_asset(LEVEL)
    if not level:
        raise RuntimeError("DA_Level_HBCT is missing")
    replacement_ids = {
        "AdjustedRoutineDecision", "AdjustedWalkRecording", "AdjustedRoutineSummary"}
    def activity_id(activity):
        name = str(unreal.GameplayTagLibrary.get_tag_name(
            activity.get_editor_property("activity_tag")))
        return name.rsplit(".", 1)[-1]

    replacements = [activity for activity in q5_author.build_activities()
                    if activity_id(activity) in replacement_ids]
    if len(replacements) != 3:
        raise RuntimeError("Could not build all three O14 learning activities")
    day = level.get_editor_property("day_data")
    topics = list(day.get_editor_property("topics"))
    topic_index = next((index for index, topic in enumerate(topics)
                        if str(topic.get_editor_property("topic_id")) == q5_author.TOPIC_ID), None)
    if topic_index is None:
        raise RuntimeError("Self-monitoring topic is missing")
    topic = topics[topic_index]
    activities = [activity for activity in list(topic.get_editor_property("activities"))
                  if activity_id(activity) not in replacement_ids]
    activities.extend(replacements)
    topic.set_editor_property("activities", activities)
    topics[topic_index] = topic
    day.set_editor_property("topics", topics)
    level.modify()
    level.set_editor_property("day_data", day)
    return level


def update_objective(conversation_asset):
    arc = unreal.EditorAssetLibrary.load_asset(ARC)
    if not arc:
        raise RuntimeError("DA_QuestArc_HBCT is missing")
    quests = list(arc.get_editor_property("quests"))
    quest_index = next((index for index, quest in enumerate(quests)
                        if str(quest.get_editor_property("quest_id")) == QUEST_ID), None)
    if quest_index is None:
        raise RuntimeError("Quest 5 is missing")
    quest = quests[quest_index]
    objectives = list(quest.get_editor_property("objectives"))
    objective_ids = [str(item.get_editor_property("objective_id")) for item in objectives]
    target_index = objective_ids.index(TARGET_OBJECTIVE)
    if target_index == 0 or objective_ids[target_index - 1] != SOURCE_OBJECTIVE:
        raise RuntimeError("O14 is no longer immediately after O13")
    objective = objectives[target_index]

    original_custom_event = str(unreal.GameplayTagLibrary.get_tag_name(
        objective.get_editor_property("custom_event_tag")))
    original_completion_effects = [story_effect_fingerprint(effect) for effect in
                                   objective.get_editor_property("completion_effects")]
    if original_custom_event != "VHV.CustomEvent.HBCT.SelfMonitoring.AdjustedRoutineComplete":
        raise RuntimeError("O14 custom-event completion contract changed unexpectedly")

    contextual = unreal.VHVQuestContextualConversation()
    contextual.set_editor_property("participant_tag", gameplay_tag("VHV.Participant.UncleChai"))
    contextual.set_editor_property("conversation", conversation_asset)
    contextual.set_editor_property("entry_node_id", "SMAdjustedWalk_01")
    contextual.set_editor_property("availability_conditions", condition_set(COMPLETION_FLAG, False))

    recording_contextual = unreal.VHVQuestContextualConversation()
    recording_contextual.set_editor_property(
        "participant_tag", gameplay_tag("VHV.Participant.UncleChai"))
    recording_contextual.set_editor_property("conversation", conversation_asset)
    recording_contextual.set_editor_property("entry_node_id", "SMAdjustedWalk_RecordIntro")
    recording_contextual.set_editor_property("availability_conditions", conditions(
        (ROUTE_COMPLETE_FLAG, True), (RECORDED_FLAG, False),
        (REFLECTION_COMPLETE_FLAG, False)))

    reflection_contextual = unreal.VHVQuestContextualConversation()
    reflection_contextual.set_editor_property(
        "participant_tag", gameplay_tag("VHV.Participant.UncleChai"))
    reflection_contextual.set_editor_property("conversation", conversation_asset)
    reflection_contextual.set_editor_property("entry_node_id", "SMAdjustedWalk_07")
    reflection_contextual.set_editor_property("availability_conditions", conditions(
        (RECORDED_FLAG, True), (REFLECTION_COMPLETE_FLAG, False)))

    objective.set_editor_property("participant_tag", gameplay_tag("VHV.Participant.UncleChai"))
    objective.set_editor_property(
        "objective_text", "Help Uncle Chai put his adjusted walking plan into practice")
    objective.set_editor_property(
        "location_tag", gameplay_tag("VHV.Location.HBCT.SelfMonitoring.SMAdjustedWalkStart"))
    objective.set_editor_property("ensure_participant_present_at_location", False)
    objective.set_editor_property("prepare_activation_npc_readiness_before_exposure", True)
    objective.set_editor_property("activation_npc_readiness", [npc_move(
        "VHV.Participant.UncleChai",
        "VHV.Location.HBCT.SelfMonitoring.SMAdjustedWalkStart")])
    objective.set_editor_property(
        "contextual_conversations", [contextual, recording_contextual, reflection_contextual])
    objective.set_editor_property("npc_move_stage_activation_conditions", conditions(
        (COMPLETION_FLAG, True), (ROUTE_COMPLETE_FLAG, False)))
    objective.set_editor_property("complete_after_npc_move_stages", False)
    objective.set_editor_property(
        "completion_conditions", condition_set(REFLECTION_COMPLETE_FLAG))

    stages = list(objective.get_editor_property("npc_move_stages"))
    if len(stages) != 2:
        raise RuntimeError("O14 must retain its two existing NPC move stages")
    first_stage = stages[0]
    first_stage.set_editor_property("ambient_conversation_receiver_tag", unreal.GameplayTag())
    stages[0] = first_stage
    objective.set_editor_property("npc_move_stages", stages)

    if [story_effect_fingerprint(effect) for effect in
            objective.get_editor_property("completion_effects")] != original_completion_effects:
        raise RuntimeError("O14 completion effects changed")
    objectives[target_index] = objective
    quest.set_editor_property("objectives", objectives)
    quests[quest_index] = quest
    arc.modify()
    arc.set_editor_property("quests", quests)
    return arc


def configure_sequence(actor_subsystem, actors, chai):
    matches = [actor for actor in actors if actor.get_actor_label() == SEQUENCE_LABEL]
    if len(matches) > 1:
        raise RuntimeError("Duplicate adjusted-routine time-skip sequences")
    sequence = matches[0] if matches else actor_subsystem.spawn_actor_from_class(
        unreal.VHVTimeSkipSequence, unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    if not sequence:
        raise RuntimeError("Could not create adjusted-routine time-skip sequence")
    sequence.set_actor_label(SEQUENCE_LABEL)
    sequence.set_folder_path("Village_Main/Q5_TimeSkip/AdjustedRoutine")

    stage = unreal.VHVTimeSkipStage()
    stage.set_editor_property("stage_id", "AdjustedRoutineThenLater")
    stage.set_editor_property("objective_id", TARGET_OBJECTIVE)
    stage.set_editor_property("transition_text", "Then Later...")
    stage.set_editor_property("observation_scene", None)
    stage.set_editor_property("stage_actors", [])
    stage.set_editor_property("player_arrival_target", None)
    stage.set_editor_property("player_arrival_relative_actor", chai)
    stage.set_editor_property("player_arrival_behind_distance", 250.0)
    stage.set_editor_property("player_arrival_lateral_offset", 55.0)
    stage.set_editor_property("face_relative_actor_toward_player", True)
    stage.set_editor_property("player_arrival_support_actor", None)
    stage.set_editor_property("legacy_trigger", None)
    stage.set_editor_property("auto_start_observation_after_transition", False)
    stage.set_editor_property("post_transition_observation_delay", 0.0)
    stage.set_editor_property("presentation_only", True)

    sequence.set_editor_property("required_quest_id", QUEST_ID)
    sequence.set_editor_property("required_objective_id", SOURCE_OBJECTIVE)
    sequence.set_editor_property("progress_counter_id", "")
    sequence.set_editor_property("advance_on_objective_completion", False)
    sequence.set_editor_property("transition_on_next_objective_activation", True)
    sequence.set_editor_property("wait_for_objective_ready_for_entry_transition", True)
    sequence.set_editor_property("require_navigation_projection", True)
    sequence.set_editor_property("request_objective_activation_on_sequence_completed", False)
    sequence.set_editor_property("keep_final_stage_after_objective", False)
    sequence.set_editor_property("stop_observations_when_deactivated", False)
    sequence.set_editor_property("fade_in_duration", 0.45)
    sequence.set_editor_property("text_hold_duration", 1.30)
    sequence.set_editor_property("fade_out_duration", 0.45)
    sequence.set_editor_property("transition_fail_safe_duration", 3.0)
    sequence.set_editor_property("stages", [stage])

    display_controller = exact_by_label(actors, DISPLAY_CONTROLLER_LABEL)
    additional = [item for item in list(
        display_controller.get_editor_property("additional_time_skip_sequences") or [])
                  if item and item != sequence]
    additional.append(sequence)
    display_controller.set_editor_property("additional_time_skip_sequences", additional)
    return sequence


def validate_readback(arc, level, conversation, sequence, chai, chai_transform):
    quests = list(arc.get_editor_property("quests"))
    quest = next(item for item in quests if str(item.get_editor_property("quest_id")) == QUEST_ID)
    objectives = list(quest.get_editor_property("objectives"))
    objective = next(item for item in objectives
                     if str(item.get_editor_property("objective_id")) == TARGET_OBJECTIVE)
    contextuals = list(objective.get_editor_property("contextual_conversations"))
    stages = list(objective.get_editor_property("npc_move_stages"))
    sequence_stages = list(sequence.get_editor_property("stages"))
    nodes = list(conversation.get_editor_property("conversation").get_editor_property("nodes"))
    node_ids = [str(node.get_editor_property("node_id")) for node in nodes]
    text_nodes = [node for node in nodes if node.get_editor_property("node_type")
                  == unreal.VHVDialogueNodeType.TEXT]
    day = level.get_editor_property("day_data")
    topic = next(item for item in day.get_editor_property("topics")
                 if str(item.get_editor_property("topic_id")) == q5_author.TOPIC_ID)
    activities = {str(unreal.GameplayTagLibrary.get_tag_name(
        item.get_editor_property("activity_tag"))).rsplit(".", 1)[-1]: item
        for item in topic.get_editor_property("activities")}
    decision = activities.get("AdjustedRoutineDecision")
    recording = activities.get("AdjustedWalkRecording")
    summary = activities.get("AdjustedRoutineSummary")

    def correct_option_index(activity):
        options = activity.get_editor_property("question").get_editor_property("options")
        return next((index for index, option in enumerate(options)
                     if option.get_editor_property("is_correct")), -1)

    checks = {
        "ObjectiveTypeCustomEvent": objective.get_editor_property("objective_type")
            == unreal.VHVQuestObjectiveType.CUSTOM_EVENT,
        "ContextualConversationCount": len(contextuals) == 3,
        "CompatibleParticipantStaging": not objective.get_editor_property(
            "ensure_participant_present_at_location")
            and objective.get_editor_property("prepare_activation_npc_readiness_before_exposure")
            and len(objective.get_editor_property("activation_npc_readiness")) == 1,
        "InteractionParticipant": str(unreal.GameplayTagLibrary.get_tag_name(
            contextuals[0].get_editor_property("participant_tag"))) == "VHV.Participant.UncleChai",
        "MoveGateConfigured": len(objective.get_editor_property(
            "npc_move_stage_activation_conditions").get_editor_property("conditions")) == 2,
        "RouteDoesNotCompleteObjective": not objective.get_editor_property(
            "complete_after_npc_move_stages"),
        "ReflectionGatesCompletion": len(objective.get_editor_property(
            "completion_conditions").get_editor_property("conditions")) == 1,
        "LegacyAmbientDetached": str(unreal.GameplayTagLibrary.get_tag_name(
            stages[0].get_editor_property("ambient_conversation_receiver_tag"))) in ("", "None"),
        "WorldBubblesEnabled": bool(conversation.get_editor_property(
            "conversation").get_editor_property("use_world_space_speech_bubbles")),
        "ConversationPhases": all(required in node_ids for required in (
            "SMAdjustedWalk_Decision", "SMAdjustedWalk_RecordIntro",
            "SMAdjustedWalk_RecordDecision", "SMAdjustedWalk_Summary")),
        "SpeakerOwnership": all(
            (str(node.get_editor_property("speaker_name")) == "Player"
             and str(unreal.GameplayTagLibrary.get_tag_name(
                 node.get_editor_property("speaker_tag"))) in ("", "None"))
            or (str(node.get_editor_property("speaker_name")) == "Uncle Chai"
                and str(unreal.GameplayTagLibrary.get_tag_name(
                    node.get_editor_property("speaker_tag"))) == "VHV.Participant.UncleChai")
            for node in text_nodes),
        "AllLinesSpoken": all(node.get_editor_property("presentation")
            == unreal.VHVDialoguePresentation.SPOKEN for node in nodes),
        "DecisionRetriesUntilCorrect": decision is not None
            and decision.get_editor_property("attempt_policy").get_editor_property("retry_until_correct")
            and correct_option_index(decision) == 0,
        "RecordingRetriesUntilCorrect": recording is not None
            and recording.get_editor_property("attempt_policy").get_editor_property("retry_until_correct")
            and correct_option_index(recording) == 0,
        "SummaryAcknowledgement": summary is not None
            and summary.get_editor_property("activity_type")
                == unreal.TextbookActivityType.OBSERVATION,
        "TenMinuteWalkNotCreditedAsFullDay": recording is not None
            and all(effect_item.get_editor_property("effect_type")
                    == unreal.VHVStoryEffectType.SET_FLAG
                    for effect_item in recording.get_editor_property("success_effects")),
        "TransitionText": str(sequence_stages[0].get_editor_property("transition_text"))
            == "Then Later...",
        "RelativeActor": sequence_stages[0].get_editor_property("player_arrival_relative_actor") == chai,
        "BehindDistance": abs(float(sequence_stages[0].get_editor_property(
            "player_arrival_behind_distance")) - 250.0) < 0.01,
        "LateralOffset": abs(float(sequence_stages[0].get_editor_property(
            "player_arrival_lateral_offset")) - 55.0) < 0.01,
        "ChaiFacesPlayer": bool(sequence_stages[0].get_editor_property(
            "face_relative_actor_toward_player")),
        "WaitsForStaging": bool(sequence.get_editor_property(
            "wait_for_objective_ready_for_entry_transition")),
        "ChaiTransformUnchanged": actor_transform(chai) == chai_transform,
    }
    failed = [name for name, passed in checks.items() if not passed]
    if failed:
        raise RuntimeError("Focused validation failed: {}".format(failed))
    return checks


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    if not world:
        raise RuntimeError("Could not load Lvl_Village_Main")
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    transforms_before = {actor.get_path_name(): actor_transform(actor) for actor in actors}
    chai = exact_participant(actors, "UncleChai")
    chai_transform = actor_transform(chai)

    conversation = build_conversation()
    level = update_activities()
    arc = update_objective(conversation)
    sequence = configure_sequence(actor_subsystem, actors, chai)

    changed_existing_transforms = [
        actor.get_actor_label() for actor in actor_subsystem.get_all_level_actors()
        if actor.get_path_name() in transforms_before
        and actor_transform(actor) != transforms_before[actor.get_path_name()]
    ]
    if changed_existing_transforms:
        raise RuntimeError("Existing actor transforms changed: {}".format(changed_existing_transforms))

    checks = validate_readback(arc, level, conversation, sequence, chai, chai_transform)
    if not unreal.EditorAssetLibrary.save_loaded_asset(conversation, only_if_is_dirty=False):
        raise RuntimeError("Failed to save adjusted-routine conversation")
    if not unreal.EditorAssetLibrary.save_loaded_asset(arc, only_if_is_dirty=False):
        raise RuntimeError("Failed to save DA_QuestArc_HBCT")
    if not unreal.EditorAssetLibrary.save_loaded_asset(level, only_if_is_dirty=False):
        raise RuntimeError("Failed to save DA_Level_HBCT")
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
        raise RuntimeError("Failed to save Lvl_Village_Main")

    output_dir = os.path.join(unreal.Paths.project_saved_dir(), "VHV_Q5AdjustedRoutine")
    os.makedirs(output_dir, exist_ok=True)
    report = {
        "Status": "SUCCESS",
        "Quest": QUEST_ID,
        "SourceObjective": SOURCE_OBJECTIVE,
        "Objective": TARGET_OBJECTIVE,
        "Conversation": conversation.get_path_name(),
        "Sequence": sequence.get_path_name(),
        "TransitionText": "Then Later...",
        "PlayerArrival": {"RelativeActor": chai.get_actor_label(),
                          "BehindDistanceCm": 250.0, "LateralOffsetCm": 55.0},
        "Checks": checks,
        "ExistingActorTransformsChanged": 0,
    }
    with open(os.path.join(output_dir, "validation.json"), "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
    unreal.log("[Q5 Adjusted Routine] {}".format(json.dumps(report)))


if __name__ == "__main__":
    main()
