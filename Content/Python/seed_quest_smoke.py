"""Reset QuestSmoke to its six-objective framework regression scope.

This idempotent script updates only the intended assets below
/Game/VHV_Stuff/Dev/QuestSmoke and never deletes content.
"""

import traceback
import unreal


ASSET_ROOT = "/Game/VHV_Stuff/Dev/QuestSmoke"
LEVEL_NAME = "DA_Level_QuestSmoke"
INTRO_NAME = "DA_Conversation_QuestSmoke_Intro"
ARC_NAME = "DA_QuestArc_QuestSmoke"


def _asset_path(asset_name):
    return "{}/{}".format(ASSET_ROOT, asset_name)


def _make_struct(struct_type, **properties):
    value = struct_type()
    value.set_editor_properties(properties)
    return value


def _tag(tag_name):
    value = unreal.GameplayTag()
    if not value.import_text(tag_name):
        raise RuntimeError("Gameplay Tag is not registered: {}".format(tag_name))
    return value


def _tag_name(value):
    return str(unreal.GameplayTagLibrary.get_tag_name(value))


def _load_required(asset_name, asset_type):
    path = _asset_path(asset_name)
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset or asset.get_class() != asset_type.static_class():
        actual_class = asset.get_class().get_name() if asset else "missing or unloadable"
        raise RuntimeError(
            "{} is {}; expected {}. Refusing to replace it.".format(
                path, actual_class, asset_type.static_class().get_name()))
    asset.modify()
    unreal.log("[QuestSmoke] Updating {}".format(path))
    return asset


def _make_teaching(title, content):
    return _make_struct(
        unreal.TeachingContent,
        title=title,
        content=content,
        key_takeaways=[],
        media=[])


def _configure_level(level):
    question = _make_struct(
        unreal.QuestionData,
        question_id="SMOKE_A01_Q",
        question_text="Does being healthy mean only being free from illness?",
        options=[
            _make_struct(
                unreal.QuestionOption,
                option_text="Yes, health only means not having an illness.",
                is_correct=False),
            _make_struct(
                unreal.QuestionOption,
                option_text="No. Health includes physical, mental, and social well-being.",
                is_correct=True),
        ])
    activity = _make_struct(
        unreal.TextbookActivityData,
        activity_tag=_tag("VHV.Activity.QuestSmoke.SMOKE_A01"),
        activity_title="What Does Health Mean?",
        activity_type=unreal.TextbookActivityType.SINGLE_CHOICE,
        narrative_context="",
        prompt_text="Does being healthy mean only being free from illness?",
        question=question,
        correct_feedback="Correct. Health includes physical, mental, and social well-being.",
        incorrect_feedback="Health includes more than the absence of illness.",
        partial_feedback="",
        teaching=_make_teaching(
            "Health and Well-being",
            "Health includes physical, mental, and social well-being."),
        hints=[],
        media=[],
        require_correct_answer_to_advance=False,
        success_effects=[],
        failure_effects=[])
    topic = _make_struct(
        unreal.TopicData,
        topic_id="SMOKE_T01",
        topic_title="Understanding Health",
        narrative_introduction="A small isolated topic used by the quest smoke test.",
        activities=[activity])
    day = _make_struct(
        unreal.DayData,
        day_number=1,
        day_title="Quest Smoke Day",
        narrative_role="Quest framework smoke test",
        introduction_text="Complete the isolated quest objectives.",
        topics=[topic],
        completion_summary="Quest smoke activity complete.",
        completion_takeaways=[])
    level.set_editor_properties({
        "level_number": 1,
        "level_title": "Quest Flow Smoke Test",
        "level_description": "Isolated textbook content for quest framework regression testing.",
        "day_data": day,
    })


def _make_text_node(node_id, line, next_node_id="", checkpoint_tag=""):
    properties = {
        "node_id": node_id,
        "node_type": unreal.VHVDialogueNodeType.TEXT,
        "speaker_tag": _tag("VHV.Participant.Instructor"),
        "speaker_name": "Instructor",
        "text": line,
        "choices": [],
        "next_node_id": next_node_id,
        "is_checkpoint": bool(checkpoint_tag),
        "completion_effects": [],
    }
    if checkpoint_tag:
        properties["checkpoint_tag"] = _tag(checkpoint_tag)
    return _make_struct(unreal.DialogueNode, **properties)


def _configure_intro_conversation(asset):
    asset.set_editor_property(
        "conversation",
        _make_struct(
            unreal.DialogueConversation,
            conversation_id="QuestSmoke_Intro",
            start_node_id="SmokeIntro_01",
            nodes=[
                _make_text_node(
                    "SmokeIntro_01",
                    "Welcome. Before we begin, let's think about what health really means.",
                    "SmokeIntro_02"),
                _make_text_node(
                    "SmokeIntro_02",
                    "Answer one quick question, then you can continue exploring.",
                    checkpoint_tag="VHV.Checkpoint.CP_SMOKE_INTRO_DONE"),
            ]))


def _objective(objective_id, text, objective_type, **properties):
    values = {
        "objective_id": objective_id,
        "objective_text": text,
        "objective_type": objective_type,
        "completion_effects": [],
    }
    values.update(properties)
    return _make_struct(unreal.VHVQuestObjectiveDefinition, **values)


def _configure_quest_arc(arc, level, intro):
    objectives = [
        _objective(
            "Q_SMOKE_OBJ_01_TALK",
            "Talk to the Instructor",
            unreal.VHVQuestObjectiveType.TALK,
            participant_tag=_tag("VHV.Participant.Instructor"),
            conversation=intro,
            entry_node_id="SmokeIntro_01",
            auto_start=False),
        _objective(
            "Q_SMOKE_OBJ_02_ACTIVITY",
            "Complete the health activity",
            unreal.VHVQuestObjectiveType.LEARNING_ACTIVITY,
            activity_tag=_tag("VHV.Activity.QuestSmoke.SMOKE_A01"),
            auto_start=True),
        _objective(
            "Q_SMOKE_OBJ_03_MOVE",
            "Move the Instructor to the desk",
            unreal.VHVQuestObjectiveType.NPC_ACTION,
            npc_participant_tag=_tag("VHV.Participant.Instructor"),
            npc_command_type=unreal.VHVNPCQuestCommandType.MOVE_TO_TARGET,
            npc_behavior_target_tag=_tag("VHV.BehaviorTarget.InstructorDesk")),
        _objective(
            "Q_SMOKE_OBJ_04_WAVE",
            "Ask the Instructor to wave",
            unreal.VHVQuestObjectiveType.NPC_ACTION,
            npc_participant_tag=_tag("VHV.Participant.Instructor"),
            npc_command_type=unreal.VHVNPCQuestCommandType.PLAY_ACTION,
            npc_action_tag=_tag("VHV.NPCAction.Wave")),
        _objective(
            "Q_SMOKE_OBJ_05_WORLD",
            "Activate the smoke test object",
            unreal.VHVQuestObjectiveType.WORLD_ACTION,
            world_action_receiver_tag=_tag("VHV.WorldReceiver.TestObject"),
            world_action_tag=_tag("VHV.WorldAction.Activate")),
        _objective(
            "Q_SMOKE_OBJ_06_LOCATION",
            "Reach the smoke test location",
            unreal.VHVQuestObjectiveType.REACH_LOCATION,
            location_tag=_tag("VHV.Location.TestLocation")),
    ]
    quest = _make_struct(
        unreal.VHVQuestDefinition,
        quest_id="Q_SMOKE_01",
        quest_title="Quest Flow Smoke Test",
        quest_description="Compact regression coverage for the six core quest objective paths.",
        category=unreal.VHVQuestCategory.MAIN_STORY,
        objectives=objectives,
        auto_track=True,
        auto_start_next_quest=False)
    arc.set_editor_properties({
        "quest_arc_id": "QuestSmoke",
        "quest_arc_title": "Quest Flow Smoke Test",
        "quest_arc_description": "Compact framework regression quest.",
        "associated_level_data": level,
        "quests": [quest],
        "auto_start_next_quest": False,
    })


def _validate(level, intro, arc):
    topics = level.get_editor_property("day_data").get_editor_property("topics")
    activities = topics[0].get_editor_property("activities") if len(topics) == 1 else []
    if len(activities) != 1:
        raise RuntimeError("QuestSmoke level must contain exactly one activity")
    if _tag_name(activities[0].get_editor_property("activity_tag")) != "VHV.Activity.QuestSmoke.SMOKE_A01":
        raise RuntimeError("QuestSmoke activity tag is invalid")

    conversation = intro.get_editor_property("conversation")
    if (str(conversation.get_editor_property("conversation_id")) != "QuestSmoke_Intro"
            or len(conversation.get_editor_property("nodes")) != 2):
        raise RuntimeError("QuestSmoke intro conversation is invalid")

    quests = arc.get_editor_property("quests")
    objectives = quests[0].get_editor_property("objectives") if len(quests) == 1 else []
    expected_types = (
        unreal.VHVQuestObjectiveType.TALK,
        unreal.VHVQuestObjectiveType.LEARNING_ACTIVITY,
        unreal.VHVQuestObjectiveType.NPC_ACTION,
        unreal.VHVQuestObjectiveType.NPC_ACTION,
        unreal.VHVQuestObjectiveType.WORLD_ACTION,
        unreal.VHVQuestObjectiveType.REACH_LOCATION,
    )
    if len(objectives) != len(expected_types):
        raise RuntimeError("QuestSmoke quest must contain exactly six objectives")
    if any(objectives[index].get_editor_property("objective_type") != expected_types[index]
           for index in range(len(expected_types))):
        raise RuntimeError("QuestSmoke objective order is invalid")

    validation_game_instance = unreal.new_object(unreal.GameInstance)
    validator = unreal.new_object(unreal.VHVQuestSubsystem, outer=validation_game_instance)
    if not validator.register_quest_arc(arc):
        raise RuntimeError("UVHVQuestSubsystem rejected the cleaned QuestSmoke arc")


def _save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Failed to save {}".format(asset.get_path_name()))
    unreal.log("[QuestSmoke] Saved {}".format(asset.get_path_name()))


def _validate_with_editor(assets):
    subsystem = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    for asset in assets:
        result, errors, warnings = subsystem.is_object_valid(
            asset, unreal.DataValidationUsecase.MANUAL)
        if result == unreal.DataValidationResult.INVALID:
            raise RuntimeError(
                "Editor validation rejected {}: {}".format(asset.get_path_name(), errors))
        if warnings:
            unreal.log_warning(
                "[QuestSmoke] Editor validation warnings for {}: {}".format(
                    asset.get_path_name(), warnings))


def generate():
    level = _load_required(LEVEL_NAME, unreal.VHVLevelData)
    intro = _load_required(INTRO_NAME, unreal.VHVConversationDataAsset)
    arc = _load_required(ARC_NAME, unreal.VHVQuestArcData)

    _configure_level(level)
    _configure_intro_conversation(intro)
    _configure_quest_arc(arc, level, intro)
    _validate(level, intro, arc)

    _validate_with_editor((level, intro, arc))

    for asset in (level, intro, arc):
        _save(asset)
    unreal.log("[QuestSmoke] Cleanup and validation succeeded for the three intended assets.")


if __name__ == "__main__":
    try:
        generate()
    except Exception:
        unreal.log_error("[QuestSmoke] Cleanup failed:\n{}".format(traceback.format_exc()))
        raise
