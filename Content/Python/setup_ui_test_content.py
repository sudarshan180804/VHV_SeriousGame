"""Create or update the dedicated VHV activity UI test content.

This script is safe to rerun. It only creates or updates the three named
DataAssets below /Game/VHV_Stuff/Dev/UITest and never deletes content.
"""

import traceback
import unreal


ROOT = "/Game/VHV_Stuff/Dev/UITest"
ACTIVITIES_DIR = ROOT + "/Activities"
QUESTS_DIR = ROOT + "/Quests"
DIALOGUE_DIR = ROOT + "/Dialogue"

LEVEL_NAME = "DA_Level_UITest"
ARC_NAME = "DA_QuestArc_UITest"
CONVERSATION_NAME = "DA_Conversation_UITest_Dialogue"

ACTIVITIES = (
    ("UI_TEST_SINGLE", "VHV.Activity.UITest.UI_TEST_SINGLE", unreal.TextbookActivityType.SINGLE_CHOICE),
    ("UI_TEST_MULTI", "VHV.Activity.UITest.UI_TEST_MULTI", unreal.TextbookActivityType.MULTI_CHOICE),
    ("UI_TEST_ORDERING", "VHV.Activity.UITest.UI_TEST_ORDERING", unreal.TextbookActivityType.ORDERING),
    ("UI_TEST_MATCHING", "VHV.Activity.UITest.UI_TEST_MATCHING", unreal.TextbookActivityType.MATCHING),
    ("UI_TEST_OBSERVATION", "VHV.Activity.UITest.UI_TEST_OBSERVATION", unreal.TextbookActivityType.OBSERVATION),
    ("UI_TEST_DIALOGUE", "VHV.Activity.UITest.UI_TEST_DIALOGUE", unreal.TextbookActivityType.DIALOGUE_CHOICE),
)


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


def _ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError("Failed to create {}".format(path))


def _load_or_create(directory, asset_name, asset_type):
    path = "{}/{}".format(directory, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset or asset.get_class() != asset_type.static_class():
            actual_class = asset.get_class().get_name() if asset else "unloadable"
            raise RuntimeError(
                "{} exists as {}; expected {}. Refusing to replace it.".format(
                    path, actual_class, asset_type.static_class().get_name()))
        unreal.log("[UITest] Updating {}".format(path))
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", asset_type)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name, directory, asset_type, factory)
        if not asset:
            raise RuntimeError("Failed to create {}".format(path))
        unreal.log("[UITest] Created {}".format(path))
    asset.modify()
    return asset


def _teaching(title):
    return _make_struct(
        unreal.TeachingContent,
        title=title,
        content="Placeholder teaching content for UI layout testing.",
        key_takeaways=[],
        media=[])


def _question(question_id, prompt, correct_indices):
    labels = ("Option A", "Option B", "Option C", "Option D")
    return _make_struct(
        unreal.QuestionData,
        question_id=question_id,
        question_text=prompt,
        options=[
            _make_struct(
                unreal.QuestionOption,
                option_text=label,
                is_correct=index in correct_indices)
            for index, label in enumerate(labels)
        ])


def _activity(tag_name, title, activity_type, prompt, **properties):
    values = {
        "activity_tag": _tag(tag_name),
        "activity_title": title,
        "activity_type": activity_type,
        "narrative_context": "Dedicated placeholder content for activity UI testing.",
        "prompt_text": prompt,
        "correct_feedback": "Correct.",
        "incorrect_feedback": "Try again.",
        "partial_feedback": "Some selections are correct.",
        "teaching": _teaching(title),
        "hints": [],
        "media": [],
        "require_correct_answer_to_advance": False,
        "success_effects": [],
        "failure_effects": [],
    }
    values.update(properties)
    return _make_struct(unreal.TextbookActivityData, **values)


def _build_activities():
    single_prompt = "Which option is correct?"
    multi_prompt = "Select the two correct options."
    ordering_prompt = "Arrange the cards in the correct order."
    matching_prompt = "Match each item with its pair."
    observation_prompt = "Observe the information shown, then continue."
    dialogue_prompt = "Choose a response."

    return [
        _activity(
            "VHV.Activity.UITest.UI_TEST_SINGLE",
            "Single Choice",
            unreal.TextbookActivityType.SINGLE_CHOICE,
            single_prompt,
            question=_question("UI_TEST_SINGLE_Q", single_prompt, {1})),
        _activity(
            "VHV.Activity.UITest.UI_TEST_MULTI",
            "Multi Choice",
            unreal.TextbookActivityType.MULTI_CHOICE,
            multi_prompt,
            question=_question("UI_TEST_MULTI_Q", multi_prompt, {1, 3})),
        _activity(
            "VHV.Activity.UITest.UI_TEST_ORDERING",
            "Ordering",
            unreal.TextbookActivityType.ORDERING,
            ordering_prompt,
            ordering_items=[
                _make_struct(unreal.OrderingItem, item_id=str(index), item_text="Step {}".format(index))
                for index in range(1, 5)
            ],
            correct_order=["1", "2", "3", "4"]),
        _activity(
            "VHV.Activity.UITest.UI_TEST_MATCHING",
            "Matching",
            unreal.TextbookActivityType.MATCHING,
            matching_prompt,
            matching_pairs=[
                _make_struct(unreal.MatchingPair, left_text="Item {}".format(letter), right_text="Match {}".format(letter))
                for letter in ("A", "B", "C", "D")
            ]),
        _activity(
            "VHV.Activity.UITest.UI_TEST_OBSERVATION",
            "Observation",
            unreal.TextbookActivityType.OBSERVATION,
            observation_prompt,
            narrative_context=observation_prompt,
            media=[
                _make_struct(
                    unreal.TextbookMediaReference,
                    media_id="UI_TEST_OBSERVATION_INFO",
                    media_type=unreal.TextbookMediaType.DIAGRAM,
                    description=observation_prompt,
                    required=False),
            ]),
        _activity(
            "VHV.Activity.UITest.UI_TEST_DIALOGUE",
            "Dialogue Choice",
            unreal.TextbookActivityType.DIALOGUE_CHOICE,
            dialogue_prompt,
            dialogue_choice=_make_struct(
                unreal.DialogueChoiceActivityReference,
                conversation_id="UITest_Dialogue",
                choice_node_id="UI_TEST_DIALOGUE_CHOICE",
                required_for_progress=True)),
    ]


def _configure_level(level):
    topic = _make_struct(
        unreal.TopicData,
        topic_id="UI_TEST_TOPIC",
        topic_title="Activity UI Showcase",
        narrative_introduction="Sequential placeholder activities for UI iteration.",
        activities=_build_activities())
    day = _make_struct(
        unreal.DayData,
        day_number=1,
        day_title="UI Activity Test",
        narrative_role="UI test content",
        introduction_text="Complete each activity UI in sequence.",
        topics=[topic],
        completion_summary="All activity UI examples complete.",
        completion_takeaways=[])
    level.set_editor_properties({
        "level_number": 1,
        "level_title": "UI Activity Test",
        "level_description": "Dedicated content for testing every implemented activity UI.",
        "day_data": day,
    })


def _choice(option_id, text, is_default=False):
    return _make_struct(
        unreal.DialogueChoiceOption,
        option_id=option_id,
        option_text=text,
        next_node_id="",
        is_default=is_default,
        selection_effects=[])


def _configure_conversation(asset):
    choice_node = _make_struct(
        unreal.DialogueNode,
        node_id="UI_TEST_DIALOGUE_CHOICE",
        node_type=unreal.VHVDialogueNodeType.CHOICE,
        speaker_tag=_tag("VHV.Participant.Instructor"),
        speaker_name="Instructor",
        text="Choose a response.",
        choices=[
            _choice("RESPONSE_A", "Response A", True),
            _choice("RESPONSE_B", "Response B"),
            _choice("RESPONSE_C", "Response C"),
        ],
        next_node_id="",
        is_checkpoint=False,
        completion_effects=[])
    asset.set_editor_property(
        "conversation",
        _make_struct(
            unreal.DialogueConversation,
            conversation_id="UITest_Dialogue",
            start_node_id="UI_TEST_DIALOGUE_CHOICE",
            nodes=[choice_node]))


def _configure_arc(arc, level):
    objectives = []
    labels = (
        "Complete Single Choice",
        "Complete Multi Choice",
        "Complete Ordering",
        "Complete Matching",
        "Complete Observation",
        "Complete Dialogue Choice",
    )
    for index, ((activity_id, tag_name, _), label) in enumerate(zip(ACTIVITIES, labels), 1):
        objectives.append(_make_struct(
            unreal.VHVQuestObjectiveDefinition,
            objective_id="Q_UI_TEST_OBJ_{:02d}".format(index),
            objective_text=label,
            objective_type=unreal.VHVQuestObjectiveType.LEARNING_ACTIVITY,
            activity_tag=_tag(tag_name),
            auto_start=True,
            completion_effects=[]))

    quest = _make_struct(
        unreal.VHVQuestDefinition,
        quest_id="Q_UI_TEST",
        quest_title="UI Activity Test",
        quest_description="Linear showcase of every implemented learning activity UI.",
        category=unreal.VHVQuestCategory.MAIN_STORY,
        objectives=objectives,
        auto_track=True,
        auto_start_next_quest=False)
    arc.set_editor_properties({
        "quest_arc_id": "UITest",
        "quest_arc_title": "UI Activity Test",
        "quest_arc_description": "Dedicated quest arc for activity UI iteration.",
        "associated_level_data": level,
        "quests": [quest],
        "auto_start_next_quest": False,
    })


def _validate(level, conversation_asset, arc):
    topics = level.get_editor_property("day_data").get_editor_property("topics")
    activities = topics[0].get_editor_property("activities") if len(topics) == 1 else []
    if len(activities) != len(ACTIVITIES):
        raise RuntimeError("UI test level must contain all six implemented activity types")

    for index, (expected_id, expected_tag, expected_type) in enumerate(ACTIVITIES):
        activity = activities[index]
        if _tag_name(activity.get_editor_property("activity_tag")) != expected_tag:
            raise RuntimeError("Unexpected activity tag at index {}".format(index))
        if activity.get_editor_property("activity_type") != expected_type:
            raise RuntimeError("Unexpected activity type at index {}".format(index))
        if not expected_tag.endswith("." + expected_id):
            raise RuntimeError("Activity tag leaf does not match runtime ID {}".format(expected_id))

    single_options = activities[0].get_editor_property("question").get_editor_property("options")
    multi_options = activities[1].get_editor_property("question").get_editor_property("options")
    if len(single_options) != 4 or sum(o.get_editor_property("is_correct") for o in single_options) != 1:
        raise RuntimeError("SingleChoice test data is invalid")
    if len(multi_options) != 4 or sum(o.get_editor_property("is_correct") for o in multi_options) != 2:
        raise RuntimeError("MultiChoice test data is invalid")
    if len(activities[2].get_editor_property("ordering_items")) != 4:
        raise RuntimeError("Ordering test data is invalid")
    if len(activities[3].get_editor_property("matching_pairs")) != 4:
        raise RuntimeError("Matching test data is invalid")
    if len(activities[4].get_editor_property("media")) != 1:
        raise RuntimeError("Observation test data is invalid")

    conversation = conversation_asset.get_editor_property("conversation")
    nodes = conversation.get_editor_property("nodes")
    if (str(conversation.get_editor_property("conversation_id")) != "UITest_Dialogue"
            or len(nodes) != 1
            or len(nodes[0].get_editor_property("choices")) != 3):
        raise RuntimeError("DialogueChoice conversation is invalid")

    quests = arc.get_editor_property("quests")
    objectives = quests[0].get_editor_property("objectives") if len(quests) == 1 else []
    if len(objectives) != len(ACTIVITIES):
        raise RuntimeError("UI test quest must contain exactly six objectives")
    for index, objective in enumerate(objectives):
        if (objective.get_editor_property("objective_type") != unreal.VHVQuestObjectiveType.LEARNING_ACTIVITY
                or not objective.get_editor_property("auto_start")
                or _tag_name(objective.get_editor_property("activity_tag")) != ACTIVITIES[index][1]
                or objective.get_editor_property("completion_effects")):
            raise RuntimeError("UI test objective {} is not isolated and auto-starting".format(index))

    validation_game_instance = unreal.new_object(unreal.GameInstance)
    validator = unreal.new_object(unreal.VHVQuestSubsystem, outer=validation_game_instance)
    if not validator.register_quest_arc(arc):
        raise RuntimeError("UVHVQuestSubsystem rejected the generated UI test arc")


def _save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Failed to save {}".format(asset.get_path_name()))
    unreal.log("[UITest] Saved {}".format(asset.get_path_name()))


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
                "[UITest] Editor validation warnings for {}: {}".format(
                    asset.get_path_name(), warnings))


def generate():
    for directory in (ROOT, ACTIVITIES_DIR, QUESTS_DIR, DIALOGUE_DIR):
        _ensure_directory(directory)

    level = _load_or_create(ACTIVITIES_DIR, LEVEL_NAME, unreal.VHVLevelData)
    conversation = _load_or_create(
        DIALOGUE_DIR, CONVERSATION_NAME, unreal.VHVConversationDataAsset)
    arc = _load_or_create(QUESTS_DIR, ARC_NAME, unreal.VHVQuestArcData)

    _configure_level(level)
    _configure_conversation(conversation)
    _configure_arc(arc, level)
    _validate(level, conversation, arc)

    _validate_with_editor((level, conversation, arc))

    for asset in (level, conversation, arc):
        _save(asset)
    unreal.log("[UITest] Generation and validation succeeded for all three assets.")


if __name__ == "__main__":
    try:
        generate()
    except Exception:
        unreal.log_error("[UITest] Generation failed:\n{}".format(traceback.format_exc()))
        raise
