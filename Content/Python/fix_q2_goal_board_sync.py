"""Idempotently synchronize Technique 2 Instructor placement and navigation.

This focused repair updates the HBCT Quest 2 definition and validates the
existing ground-level FinalInstructor anchor in both village authoring maps.
"""

import traceback
import unreal


LOG = "[VHV Q2 Goal Board Fix]"
ARC_PATH = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
QUEST_ID = "Q_HBCT_02_GOAL_SETTING"
MAP_PATHS = (
    "/Game/VHV_Stuff/Maps/Lvl_Village_Main",
    "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout",
)
INSTRUCTOR_TAG = "VHV.Participant.Instructor"
HEALTH_POST_TAG = "VHV.Location.HBCT.HealthPost"
FINAL_INSTRUCTOR_TAG = "VHV.Location.HBCT.GoalSetting.FinalInstructor"
BOARD_VISUAL_LABEL = "HBCT_Location_GoalSetting_GoodGoalBoard"
FINAL_INSTRUCTOR_LABEL = "HBCT_Location_GoalSetting_FinalInstructor"


INSTRUCTOR_OBJECTIVE_LOCATIONS = {
    "O01_ReachHealthPost": HEALTH_POST_TAG,
    "O04_LearnGoalSetting": HEALTH_POST_TAG,
    "O06_ReflectOnDirection": HEALTH_POST_TAG,
    "O08_IdentifyGoalMethods": HEALTH_POST_TAG,
    "O09_EvaluateGoals": FINAL_INSTRUCTOR_TAG,
    "O16_FinalDebrief": FINAL_INSTRUCTOR_TAG,
}


def gameplay_tag(name):
    value = unreal.GameplayTag()
    if not value.import_text(name):
        raise RuntimeError("Gameplay Tag is not registered: {}".format(name))
    return value


def save_asset(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def tag_name(value):
    return str(value.get_editor_property("tag_name"))


def configure_quest():
    arc = unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    if not arc:
        raise RuntimeError("Missing {}".format(ARC_PATH))
    arc.modify()
    quests = list(arc.get_editor_property("quests"))
    quest = next((item for item in quests
                  if str(item.get_editor_property("quest_id")) == QUEST_ID), None)
    if not quest:
        raise RuntimeError("Missing {}".format(QUEST_ID))

    objectives = list(quest.get_editor_property("objectives"))
    by_id = {str(item.get_editor_property("objective_id")): item
             for item in objectives}
    missing = sorted(set(INSTRUCTOR_OBJECTIVE_LOCATIONS) - set(by_id))
    if missing:
        raise RuntimeError("Missing Quest 2 Instructor objectives: {}".format(missing))

    for objective_id, location_tag in INSTRUCTOR_OBJECTIVE_LOCATIONS.items():
        objective = by_id[objective_id]
        if tag_name(objective.get_editor_property("participant_tag")) != INSTRUCTOR_TAG:
            raise RuntimeError("{} no longer targets the Instructor".format(objective_id))
        objective.set_editor_properties({
            "location_tag": gameplay_tag(location_tag),
            "ensure_participant_present_at_location": True,
        })
        if objective_id == "O09_EvaluateGoals":
            objective.set_editor_properties({
                "enable_location_tracking": True,
                "tracking_location_tag": gameplay_tag(FINAL_INSTRUCTOR_TAG),
            })

    quest.set_editor_property("objectives", objectives)
    arc.set_editor_property("quests", quests)
    save_asset(arc)
    unreal.log("{} Quest 2 Instructor objectives now own explicit placement anchors.".format(LOG))


def configure_map(map_path):
    if not unreal.EditorLoadingAndSavingUtils.load_map(map_path):
        raise RuntimeError("Could not load {}".format(map_path))
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    by_label = {actor.get_actor_label(): actor for actor in actors}

    instructors = []
    for actor in actors:
        try:
            participant = actor.get_quest_participant_component()
        except Exception:
            participant = None
        if participant and str(participant.get_effective_participant_id()) == "Instructor":
            instructors.append(actor)
    if len(instructors) != 1:
        raise RuntimeError("{} has {} Instructor participants; expected exactly one".format(
            map_path, len(instructors)))

    board_visual = by_label.get(BOARD_VISUAL_LABEL)
    final_anchor = by_label.get(FINAL_INSTRUCTOR_LABEL)
    if not isinstance(board_visual, unreal.VHVQuestLocationVolume):
        raise RuntimeError("{} is missing {}".format(map_path, BOARD_VISUAL_LABEL))
    if not isinstance(final_anchor, unreal.VHVQuestLocationVolume):
        raise RuntimeError("{} is missing {}".format(map_path, FINAL_INSTRUCTOR_LABEL))

    source_location = final_anchor.get_actor_location()
    board_location = board_visual.get_actor_location()
    if unreal.MathLibrary.vector_distance2d(board_location, source_location) > 150.0:
        raise RuntimeError(
            "{} board visual and Instructor anchor are not synchronized in XY".format(map_path))

    unreal.log("{} {}: Instructor={} board interaction=({:.1f}, {:.1f}, {:.1f})".format(
        LOG, map_path, instructors[0].get_actor_label(),
        source_location.x, source_location.y, source_location.z))


def validate():
    arc = unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    quest = next(item for item in arc.get_editor_property("quests")
                 if str(item.get_editor_property("quest_id")) == QUEST_ID)
    by_id = {str(item.get_editor_property("objective_id")): item
             for item in quest.get_editor_property("objectives")}
    for objective_id, expected_location in INSTRUCTOR_OBJECTIVE_LOCATIONS.items():
        objective = by_id[objective_id]
        if not objective.get_editor_property("ensure_participant_present_at_location"):
            raise RuntimeError("{} does not enforce Instructor presence".format(objective_id))
        if tag_name(objective.get_editor_property("location_tag")) != expected_location:
            raise RuntimeError("{} has the wrong placement anchor".format(objective_id))
    board = by_id["O09_EvaluateGoals"]
    if (not board.get_editor_property("enable_location_tracking")
            or tag_name(board.get_editor_property("tracking_location_tag"))
            != FINAL_INSTRUCTOR_TAG):
        raise RuntimeError("O09 navigation does not use the board interaction anchor")
    unreal.log("{} Serialized Quest 2 repair validated.".format(LOG))


def run():
    configure_quest()
    for map_path in MAP_PATHS:
        configure_map(map_path)
    validate()


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} {}".format(LOG, traceback.format_exc()))
        raise
