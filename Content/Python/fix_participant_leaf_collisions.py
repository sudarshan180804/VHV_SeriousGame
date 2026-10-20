"""Give the two authored roadside villagers unique runtime participant IDs."""

import traceback
import unreal


LOG = "[VHV Participant Collision Fix]"
ARC_PATH = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
CONVERSATIONS = {
    "/Game/VHV_Stuff/HBCT/Dialogue/DA_Conversation_HBCT_GoalSetting_RoadsidePractice": (
        "VHV.Participant.GoalSetting.GoalSettingRoadVillager", "GoalSettingRoadVillager"),
    "/Game/VHV_Stuff/HBCT/Dialogue/DA_Conversation_HBCT_RoleModel_Roadside": (
        "VHV.Participant.RoleModel.RoleModelRoadVillager", "RoleModelRoadVillager"),
}
MAP_PATHS = (
    "/Game/VHV_Stuff/Maps/Lvl_Village_Main",
    "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout",
)
ACTORS = {
    "HBCT_GoalSetting_RoadVillager": (
        "VHV.Participant.GoalSetting.GoalSettingRoadVillager", "GoalSettingRoadVillager"),
    "HBCT_RoleModel_RoadVillager": (
        "VHV.Participant.RoleModel.RoleModelRoadVillager", "RoleModelRoadVillager"),
}


def gameplay_tag(name):
    value = unreal.GameplayTag()
    if not value.import_text(name):
        raise RuntimeError("Gameplay Tag is not registered: {}".format(name))
    return value


def save_asset(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def update_quest():
    arc = unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    arc.modify()
    quests = list(arc.get_editor_property("quests"))
    expected = {
        ("Q_HBCT_02_GOAL_SETTING", "O15B_RoadsidePractice"):
            "VHV.Participant.GoalSetting.GoalSettingRoadVillager",
        ("Q_HBCT_03_ROLE_MODEL", "O15_FinalApplication"):
            "VHV.Participant.RoleModel.RoleModelRoadVillager",
    }
    found = set()
    for quest in quests:
        quest_id = str(quest.get_editor_property("quest_id"))
        objectives = list(quest.get_editor_property("objectives"))
        for objective in objectives:
            key = (quest_id, str(objective.get_editor_property("objective_id")))
            if key in expected:
                objective.set_editor_property("participant_tag", gameplay_tag(expected[key]))
                found.add(key)
        quest.set_editor_property("objectives", objectives)
    if found != set(expected):
        raise RuntimeError("Missing roadside objective(s): {}".format(set(expected) - found))
    arc.set_editor_property("quests", quests)
    save_asset(arc)


def update_conversations():
    for path, (tag_name, _) in CONVERSATIONS.items():
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            raise RuntimeError("Missing {}".format(path))
        asset.modify()
        conversation = asset.get_editor_property("conversation")
        nodes = list(conversation.get_editor_property("nodes"))
        changed = 0
        for node in nodes:
            speaker_tag = node.get_editor_property("speaker_tag")
            if str(unreal.GameplayTagLibrary.get_tag_name(speaker_tag)).endswith("RoadVillager"):
                node.set_editor_property("speaker_tag", gameplay_tag(tag_name))
                changed += 1
        if not changed:
            raise RuntimeError("No roadside speaker nodes found in {}".format(path))
        conversation.set_editor_property("nodes", nodes)
        asset.set_editor_property("conversation", conversation)
        save_asset(asset)


def update_maps():
    for map_path in MAP_PATHS:
        if not unreal.EditorLoadingAndSavingUtils.load_map(map_path):
            raise RuntimeError("Could not load {}".format(map_path))
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        by_label = {actor.get_actor_label(): actor for actor in actors}
        for label, (tag_name, participant_id) in ACTORS.items():
            actor = by_label.get(label)
            if not actor:
                raise RuntimeError("{} is missing {}".format(map_path, label))
            component = actor.get_quest_participant_component()
            component.modify()
            component.set_editor_properties({
                "participant_tag": gameplay_tag(tag_name),
                "participant_id": participant_id,
            })
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
            raise RuntimeError("Could not save {}".format(map_path))
        unreal.log("{} updated {}".format(LOG, map_path))


def validate():
    for map_path in MAP_PATHS:
        unreal.EditorLoadingAndSavingUtils.load_map(map_path)
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        ids = {}
        for actor in actors:
            try:
                component = actor.get_quest_participant_component()
            except Exception:
                continue
            if not component.get_editor_property("quest_participation_enabled"):
                continue
            participant_id = str(component.get_effective_participant_id())
            if participant_id in ids:
                raise RuntimeError("Duplicate participant ID {} on {} and {} in {}".format(
                    participant_id, ids[participant_id], actor.get_actor_label(), map_path))
            ids[participant_id] = actor.get_actor_label()
        for _, participant_id in ACTORS.values():
            if participant_id not in ids:
                raise RuntimeError("{} lacks participant {}".format(map_path, participant_id))
    unreal.log("{} all authored participant IDs are unique.".format(LOG))


def run():
    update_quest()
    update_conversations()
    update_maps()
    validate()


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} {}".format(LOG, traceback.format_exc()))
        raise
