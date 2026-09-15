"""Create or update the isolated VHV quest-flow smoke-test DataAssets.

Run from Unreal Editor with Tools > Execute Python Script, or from the
Output Log's Python console. This script only writes assets below
/Game/VHV_Stuff/Dev/QuestSmoke.
"""

import traceback
import unreal


ASSET_ROOT = "/Game/VHV_Stuff/Dev/QuestSmoke"
LEVEL_NAME = "DA_Level_QuestSmoke"
INTRO_NAME = "DA_Conversation_QuestSmoke_Intro"
LESSON_NAME = "DA_Conversation_QuestSmoke_Lesson"
ARC_NAME = "DA_QuestArc_QuestSmoke"


def _asset_path(asset_name):
    return "{}/{}".format(ASSET_ROOT, asset_name)


def _make_struct(struct_type, **properties):
    value = struct_type()
    value.set_editor_properties(properties)
    return value


def _load_or_create(asset_name, asset_type):
    path = _asset_path(asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset or asset.get_class() != asset_type.static_class():
            actual_class = asset.get_class().get_name() if asset else "unloadable"
            raise RuntimeError(
                "{} exists as {}; expected {}. Refusing to replace it.".format(
                    path, actual_class, asset_type.static_class().get_name()))
        unreal.log("[QuestSmoke] Updating {}".format(path))
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", asset_type)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name, ASSET_ROOT, asset_type, factory)
        if not asset:
            raise RuntimeError("Failed to create {}".format(path))
        unreal.log("[QuestSmoke] Created {}".format(path))

    asset.modify()
    return asset


def _make_single_choice(
        activity_id,
        title,
        prompt,
        incorrect_option,
        correct_option,
        correct_feedback,
        incorrect_feedback,
        teaching_title,
        teaching_content):
    question = _make_struct(
        unreal.QuestionData,
        question_id="{}_Q".format(activity_id),
        question_text=prompt,
        options=[
            _make_struct(
                unreal.QuestionOption,
                option_text=incorrect_option,
                is_correct=False),
            _make_struct(
                unreal.QuestionOption,
                option_text=correct_option,
                is_correct=True),
        ])
    teaching = _make_struct(
        unreal.TeachingContent,
        title=teaching_title,
        content=teaching_content,
        key_takeaways=[],
        media=[])
    return _make_struct(
        unreal.TextbookActivityData,
        activity_id=activity_id,
        activity_title=title,
        activity_type=unreal.TextbookActivityType.SINGLE_CHOICE,
        narrative_context="",
        prompt_text=prompt,
        question=question,
        correct_feedback=correct_feedback,
        incorrect_feedback=incorrect_feedback,
        partial_feedback="",
        teaching=teaching,
        hints=[],
        media=[],
        require_correct_answer_to_advance=False)


def _configure_level(level):
    activity_one = _make_single_choice(
        "SMOKE_A01",
        "What Does Health Mean?",
        "Does being healthy mean only being free from illness?",
        "Yes, health only means not having an illness.",
        "No. Health includes physical, mental, and social well-being.",
        "Correct. Health includes physical, mental, and social well-being.",
        "Health includes more than the absence of illness.",
        "Health and Well-being",
        "Health includes physical, mental, and social well-being.")

    activity_two = _make_single_choice(
        "SMOKE_A02",
        "Healthy Communities",
        "Which statement best describes good community health?",
        "Only individual health matters.",
        "Healthy individuals, families, and communities support each other.",
        "Correct. Community health is built through mutual support.",
        "Community health includes individuals, families, and their shared environment.",
        "Community Health",
        "Healthy individuals, families, and communities support each other.")

    observation_media = _make_struct(
        unreal.TextbookMediaReference,
        media_id="SMOKE_A03_MEDIA_01",
        media_type=unreal.TextbookMediaType.DIAGRAM,
        description=(
            "A community situation showing how people and their surroundings "
            "influence health."),
        required=False)
    observation_teaching = _make_struct(
        unreal.TeachingContent,
        title="Community Observation",
        content=(
            "Community conditions can affect physical, mental, and social "
            "well-being."),
        key_takeaways=[],
        media=[])
    activity_three = _make_struct(
        unreal.TextbookActivityData,
        activity_id="SMOKE_A03",
        activity_title="Community Observation",
        activity_type=unreal.TextbookActivityType.OBSERVATION,
        narrative_context="Observe the community situation.",
        prompt_text="Review the situation and continue when ready.",
        correct_feedback="Observation complete.",
        incorrect_feedback="",
        partial_feedback="",
        teaching=observation_teaching,
        hints=[],
        media=[observation_media],
        require_correct_answer_to_advance=False)

    topic = _make_struct(
        unreal.TopicData,
        topic_id="SMOKE_T01",
        topic_title="Understanding Health",
        narrative_introduction=(
            "A small isolated topic used by the quest smoke test."),
        activities=[activity_one, activity_two, activity_three])
    day = _make_struct(
        unreal.DayData,
        day_number=1,
        day_title="Quest Smoke Day",
        narrative_role="Quest framework smoke test",
        introduction_text="Complete the isolated quest objectives.",
        topics=[topic],
        completion_summary="Quest smoke activities complete.",
        completion_takeaways=[])
    level.set_editor_properties({
        "level_number": 1,
        "level_title": "Quest Flow Smoke Test",
        "level_description": (
            "Isolated textbook content for testing quest orchestration."),
        "day_data": day,
    })


def _make_text_node(node_id, line, next_node_id="", checkpoint_id=""):
    return _make_struct(
        unreal.DialogueNode,
        node_id=node_id,
        node_type=unreal.VHVDialogueNodeType.TEXT,
        speaker_id="Instructor",
        speaker_name="Instructor",
        text=line,
        choices=[],
        next_node_id=next_node_id,
        is_checkpoint=bool(checkpoint_id),
        checkpoint_id=checkpoint_id)


def _configure_intro_conversation(asset):
    conversation = _make_struct(
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
                checkpoint_id="CP_SMOKE_INTRO_DONE"),
        ])
    asset.set_editor_property("conversation", conversation)


def _configure_lesson_conversation(asset):
    conversation = _make_struct(
        unreal.DialogueConversation,
        conversation_id="QuestSmoke_Lesson",
        start_node_id="SmokeLesson_01",
        nodes=[
            _make_text_node(
                "SmokeLesson_01",
                "Good. Let's continue with two short learning activities.",
                "SmokeLesson_02"),
            _make_text_node(
                "SmokeLesson_02",
                "Think about how individual health connects with the community.",
                checkpoint_id="CP_SMOKE_LESSON_DONE"),
        ])
    asset.set_editor_property("conversation", conversation)


def _make_talk_objective(objective_id, text, conversation, entry_node_id):
    return _make_struct(
        unreal.VHVQuestObjectiveDefinition,
        objective_id=objective_id,
        objective_text=text,
        objective_type=unreal.VHVQuestObjectiveType.TALK,
        target_id="Instructor",
        activity_id="",
        conversation=conversation,
        entry_node_id=entry_node_id,
        auto_start=False)


def _make_activity_objective(objective_id, text, activity_id):
    return _make_struct(
        unreal.VHVQuestObjectiveDefinition,
        objective_id=objective_id,
        objective_text=text,
        objective_type=unreal.VHVQuestObjectiveType.LEARNING_ACTIVITY,
        target_id="Instructor",
        activity_id=activity_id,
        entry_node_id="",
        auto_start=True)


def _configure_quest_arc(arc, level, intro, lesson):
    quest = _make_struct(
        unreal.VHVQuestDefinition,
        quest_id="Q_SMOKE_01",
        quest_title="Understanding Health",
        quest_description=(
            "Talk with the Instructor and complete three short learning activities."),
        category=unreal.VHVQuestCategory.MAIN_STORY,
        objectives=[
            _make_talk_objective(
                "Q_SMOKE_OBJ_01_TALK",
                "Talk to the Instructor",
                intro,
                "SmokeIntro_01"),
            _make_activity_objective(
                "Q_SMOKE_OBJ_02_QUESTION",
                "Answer the health question",
                "SMOKE_A01"),
            _make_talk_objective(
                "Q_SMOKE_OBJ_03_TALK_AGAIN",
                "Talk to the Instructor again",
                lesson,
                "SmokeLesson_01"),
            _make_activity_objective(
                "Q_SMOKE_OBJ_04_QUESTION",
                "Answer the community health question",
                "SMOKE_A02"),
            _make_activity_objective(
                "Q_SMOKE_OBJ_05_OBSERVE",
                "Complete the community observation",
                "SMOKE_A03"),
        ],
        auto_track=True,
        auto_start_next_quest=False)
    arc.set_editor_properties({
        "quest_arc_id": "QuestSmoke",
        "quest_arc_title": "Quest Flow Smoke Test",
        "quest_arc_description": (
            "An isolated end-to-end smoke test for quest-directed dialogue "
            "and learning activities."),
        "associated_level_data": level,
        "quests": [quest],
        "auto_start_next_quest": False,
    })


def _validate_level(level):
    day = level.get_editor_property("day_data")
    topics = day.get_editor_property("topics")
    if len(topics) != 1:
        raise RuntimeError("QuestSmoke level must contain exactly one topic")
    activities = topics[0].get_editor_property("activities")
    if len(activities) != 3:
        raise RuntimeError("QuestSmoke level must contain exactly three activities")

    expected_ids = ("SMOKE_A01", "SMOKE_A02", "SMOKE_A03")
    for index, activity in enumerate(activities):
        if str(activity.get_editor_property("activity_id")) != expected_ids[index]:
            raise RuntimeError("Unexpected activity ID or order at index {}".format(index))
        if index < 2:
            question = activity.get_editor_property("question")
            options = question.get_editor_property("options")
            correct_count = sum(
                1 for option in options if option.get_editor_property("is_correct"))
            if (activity.get_editor_property("activity_type")
                    != unreal.TextbookActivityType.SINGLE_CHOICE
                    or not question.get_editor_property("question_text")
                    or len(options) != 2
                    or correct_count != 1):
                raise RuntimeError("Invalid SingleChoice activity at index {}".format(index))

    observation = activities[2]
    media = observation.get_editor_property("media")
    if (observation.get_editor_property("activity_type")
            != unreal.TextbookActivityType.OBSERVATION
            or not observation.get_editor_property("prompt_text")
            or len(media) != 1
            or not media[0].get_editor_property("description")):
        raise RuntimeError("Invalid Observation activity")


def _validate_conversation(asset, expected_checkpoint):
    conversation = asset.get_editor_property("conversation")
    nodes = conversation.get_editor_property("nodes")
    node_ids = [str(node.get_editor_property("node_id")) for node in nodes]
    if (not conversation.get_editor_property("conversation_id")
            or len(nodes) != 2
            or len(set(node_ids)) != len(node_ids)
            or str(conversation.get_editor_property("start_node_id")) not in node_ids):
        raise RuntimeError("Invalid conversation structure in {}".format(asset.get_name()))

    found_checkpoint = False
    for node in nodes:
        if node.get_editor_property("node_type") != unreal.VHVDialogueNodeType.TEXT:
            raise RuntimeError("{} contains a non-Text node".format(asset.get_name()))
        next_node_id = str(node.get_editor_property("next_node_id"))
        if next_node_id and next_node_id not in node_ids:
            raise RuntimeError("{} has a missing NextNodeID".format(asset.get_name()))
        found_checkpoint |= (
            node.get_editor_property("is_checkpoint")
            and str(node.get_editor_property("checkpoint_id")) == expected_checkpoint)
    if not found_checkpoint:
        raise RuntimeError("{} is missing checkpoint {}".format(
            asset.get_name(), expected_checkpoint))


def _validate_quest_arc(arc):
    quests = arc.get_editor_property("quests")
    if len(quests) != 1:
        raise RuntimeError("QuestSmoke arc must contain exactly one quest")
    objectives = quests[0].get_editor_property("objectives")
    if len(objectives) != 5:
        raise RuntimeError("QuestSmoke quest must contain exactly five objectives")
    expected_types = (
        unreal.VHVQuestObjectiveType.TALK,
        unreal.VHVQuestObjectiveType.LEARNING_ACTIVITY,
        unreal.VHVQuestObjectiveType.TALK,
        unreal.VHVQuestObjectiveType.LEARNING_ACTIVITY,
        unreal.VHVQuestObjectiveType.LEARNING_ACTIVITY,
    )
    if any(objectives[index].get_editor_property("objective_type") != expected_types[index]
           for index in range(len(objectives))):
        raise RuntimeError("QuestSmoke objective order or types are invalid")

    validation_game_instance = unreal.new_object(unreal.GameInstance)
    validator = unreal.new_object(
        unreal.VHVQuestSubsystem,
        outer=validation_game_instance)
    if not validator.register_quest_arc(arc):
        raise RuntimeError("UVHVQuestSubsystem rejected the generated quest arc")


def _save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Failed to save {}".format(asset.get_path_name()))
    unreal.log("[QuestSmoke] Saved {}".format(asset.get_path_name()))


def generate():
    unreal.log(
        "[QuestSmoke] Generating isolated assets under {}. "
        "Production Unit 1 assets are not loaded for editing.".format(ASSET_ROOT))
    if not unreal.EditorAssetLibrary.does_directory_exist(ASSET_ROOT):
        if not unreal.EditorAssetLibrary.make_directory(ASSET_ROOT):
            raise RuntimeError("Failed to create {}".format(ASSET_ROOT))

    level = _load_or_create(LEVEL_NAME, unreal.VHVLevelData)
    intro = _load_or_create(INTRO_NAME, unreal.VHVConversationDataAsset)
    lesson = _load_or_create(LESSON_NAME, unreal.VHVConversationDataAsset)
    arc = _load_or_create(ARC_NAME, unreal.VHVQuestArcData)

    _configure_level(level)
    _configure_intro_conversation(intro)
    _configure_lesson_conversation(lesson)
    _configure_quest_arc(arc, level, intro, lesson)

    _validate_level(level)
    _validate_conversation(intro, "CP_SMOKE_INTRO_DONE")
    _validate_conversation(lesson, "CP_SMOKE_LESSON_DONE")
    _validate_quest_arc(arc)

    for asset in (level, intro, lesson, arc):
        _save(asset)
    unreal.log("[QuestSmoke] Generation and validation succeeded for all four assets.")


if __name__ == "__main__":
    try:
        generate()
    except Exception:
        unreal.log_error("[QuestSmoke] Generation failed:\n{}".format(traceback.format_exc()))
        raise
