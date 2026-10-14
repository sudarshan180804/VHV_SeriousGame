"""Install the Q5-only objective display visibility controller without moving actors."""

import datetime
import hashlib
import json
import os
import traceback

import unreal


TARGET_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
CONTROLLER_LABEL = "VHV_Q5_ObjectiveDisplayVisibility"
TIME_SKIP_LABEL = "VHV_Q5_TimeSkip_DailyRecording"
OUTPUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "VHV_Q5DisplayVisibility")

DISPLAY_GROUPS = {
    "O01_CheckInWithChai": ["HBCT_SM_Display_BlankCalendar"],
    "O03_IdentifyTargetBehavior": [
        "HBCT_SM_Label_Calendar", "HBCT_SM_Label_Calendar_Description",
        "HBCT_SM_Label_GlucoseRecord", "HBCT_SM_Label_GlucoseRecord_Description",
        "HBCT_SM_Label_Notebook", "HBCT_SM_Label_Notebook_Description",
        "HBCT_SM_Label_Scale", "HBCT_SM_Label_Scale_Description",
        "HBCT_SM_Label_SweetDrink", "HBCT_SM_Label_SweetDrink_Description",
        "HBCT_SM_Label_Vegetables", "HBCT_SM_Label_Vegetables_Description",
        "HBCT_SM_Label_WalkingShoes", "HBCT_SM_Label_WalkingShoes_Description",
    ],
    "O05_ChooseMonitoringMethod": [
        "HBCT_SM_MethodLabel_Calendar", "HBCT_SM_MethodLabel_Calendar_Description",
        "HBCT_SM_MethodLabel_Notebook", "HBCT_SM_MethodLabel_Notebook_Description",
        "HBCT_SM_MethodLabel_Phone", "HBCT_SM_MethodLabel_Phone_Description",
    ],
    "O06_PrepareChaiTracker": ["HBCT_SM_Display_PreparedTracker"],
    "O07_RecordBreakfast": ["HBCT_SM_Display_BreakfastRecord"],
    "O08_RecordAfternoon": ["HBCT_SM_Display_AfternoonRecord"],
    "O09_RecordEveningWalk": ["HBCT_SM_Display_CompleteDay"],
    "O10_ReviewFirstDay": ["HBCT_SM_Display_FirstDayReview"],
    "O11_OneWeekLater": ["HBCT_SM_Display_WeeklyCalendar"],
    "O12_FindBehaviorPatterns": [
        "HBCT_SM_Evidence_MarketSchedule", "HBCT_SM_Evidence_SweetDrinkVisits",
        "HBCT_SM_Evidence_WalkingGoal", "HBCT_SM_Evidence_WeeklyCalendar",
    ],
    "O15_BuildProgressGraph": ["HBCT_SM_Display_ProgressGraph"],
    "O16_SetSelfReminder": ["HBCT_SM_Display_ReminderCalendar"],
    "O17_WatchChaiSelfMonitor": [
        "HBCT_SM_Display_SelfMonitorTracker",
        "HBCT_SM_Display_SelfMonitorTrackerPending",
    ],
    "O18_FinalDebrief": ["HBCT_SM_Display_Final"],
}


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def transform_values(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return [float(location.x), float(location.y), float(location.z),
            float(rotation.pitch), float(rotation.yaw), float(rotation.roll),
            float(scale.x), float(scale.y), float(scale.z)]


def map_path(world):
    outer = world.get_outer()
    package = outer.get_path_name() if outer else world.get_path_name()
    return package.split(".")[0]


def exact_actor(by_label, actor_label):
    matches = by_label.get(actor_label, [])
    if len(matches) != 1:
        raise RuntimeError("Expected one '%s'; found %d" % (actor_label, len(matches)))
    return matches[0]


def sequence_state(actor):
    return {
        "Path": actor.get_path_name(),
        "Transform": transform_values(actor),
        "Quest": str(actor.get_editor_property("required_quest_id")),
        "Objective": str(actor.get_editor_property("required_objective_id")),
        "FadeIn": float(actor.get_editor_property("fade_in_duration")),
        "Hold": float(actor.get_editor_property("text_hold_duration")),
        "FadeOut": float(actor.get_editor_property("fade_out_duration")),
        "Stages": len(list(actor.get_editor_property("stages") or [])),
    }


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world or map_path(world) != TARGET_MAP:
        raise RuntimeError("Lvl_Village_Main must be loaded")
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors_before = list(actor_subsystem.get_all_level_actors())
    by_label = {}
    for actor in actors_before:
        by_label.setdefault(actor.get_actor_label(), []).append(actor)
    if by_label.get(CONTROLLER_LABEL):
        raise RuntimeError("Q5 display visibility controller is already installed")

    mapped_labels = {label for labels in DISPLAY_GROUPS.values() for label in labels}
    if len(mapped_labels) != 36:
        raise RuntimeError("Expected 36 unique mapped Q5 displays; found %d" % len(mapped_labels))
    mapped_actors = {}
    for actor_label in sorted(mapped_labels):
        actor = exact_actor(by_label, actor_label)
        if actor.get_class().get_name() != "TextRenderActor":
            raise RuntimeError("Mapped display has unexpected class: %s/%s" %
                               (actor_label, actor.get_class().get_name()))
        mapped_actors[actor_label] = actor

    discovered_text = set()
    ambiguous_widgets = []
    for actor in actors_before:
        actor_label = actor.get_actor_label()
        if not actor_label.startswith("HBCT_SM_"):
            continue
        if actor.get_class().get_name() == "TextRenderActor":
            discovered_text.add(actor_label)
        if actor.get_class().get_name() != "BP_VHVNPCBase_C":
            widgets = list(actor.get_components_by_class(unreal.WidgetComponent) or [])
            if widgets:
                ambiguous_widgets.append({"Actor": actor_label,
                                          "Components": [item.get_name() for item in widgets]})
    missing_mappings = sorted(discovered_text - mapped_labels)
    stale_mappings = sorted(mapped_labels - discovered_text)
    if missing_mappings or stale_mappings or ambiguous_widgets:
        raise RuntimeError("Q5 display ownership is ambiguous: unmapped=%s stale=%s widgets=%s" %
                           (missing_mappings, stale_mappings, ambiguous_widgets))

    time_skip = exact_actor(by_label, TIME_SKIP_LABEL)
    if time_skip.get_class().get_name() != "VHVTimeSkipSequence":
        raise RuntimeError("Q5 time-skip actor has unexpected class")
    time_skip_before = sequence_state(time_skip)
    transforms_before = {actor.get_path_name(): transform_values(actor) for actor in actors_before}
    paths_before = {actor.get_path_name() for actor in actors_before}
    main_file = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir() + "VHV_Stuff/Maps/Lvl_Village_Main.umap")
    blockout_file = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir() + "VHV_Stuff/Maps/Lvl_Village_Blockout.umap")
    main_hash_before = sha256(main_file)
    blockout_hash_before = sha256(blockout_file)

    controller = None
    try:
        controller = actor_subsystem.spawn_actor_from_class(
            unreal.VHVQ5DisplayVisibilityController,
            unreal.Vector(0.0, 0.0, 0.0),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
        if not controller:
            raise RuntimeError("Failed to create VHVQ5DisplayVisibilityController")
        controller.set_actor_label(CONTROLLER_LABEL)
        controller.set_folder_path("Village_Main/Q5_Presentation")
        groups = []
        for objective_id, actor_labels in DISPLAY_GROUPS.items():
            group = unreal.VHVQ5ObjectiveDisplayGroup()
            group.set_editor_property("objective_id", objective_id)
            group.set_editor_property(
                "display_actors", [mapped_actors[item] for item in actor_labels])
            groups.append(group)
        controller.set_editor_property("required_quest_id", "Q_HBCT_05_SELF_MONITORING")
        controller.set_editor_property("objective_displays", groups)
        controller.set_editor_property("time_skip_sequence", time_skip)

        read_groups = list(controller.get_editor_property("objective_displays") or [])
        if len(read_groups) != len(DISPLAY_GROUPS):
            raise RuntimeError("Controller group read-back failed")
        read_mapping = {}
        for group in read_groups:
            objective_id = str(group.get_editor_property("objective_id"))
            read_mapping[objective_id] = sorted(
                actor.get_actor_label() for actor in list(group.get_editor_property("display_actors") or []))
        expected_mapping = {key: sorted(value) for key, value in DISPLAY_GROUPS.items()}
        if read_mapping != expected_mapping:
            raise RuntimeError("Controller objective/display reference read-back mismatch")
        if controller.get_editor_property("time_skip_sequence") != time_skip:
            raise RuntimeError("Controller Q5 time-skip reference read-back mismatch")

        actors_after = list(actor_subsystem.get_all_level_actors())
        paths_after = {actor.get_path_name() for actor in actors_after}
        if len(actors_after) != len(actors_before) + 1 or not paths_before.issubset(paths_after):
            raise RuntimeError("Actor identity verification failed")
        transform_changes = []
        for actor in actors_after:
            before = transforms_before.get(actor.get_path_name())
            if before is not None and transform_values(actor) != before:
                transform_changes.append(actor.get_actor_label())
        if transform_changes:
            raise RuntimeError("Pre-existing actor transforms changed: %s" % transform_changes)
        if sequence_state(time_skip) != time_skip_before:
            raise RuntimeError("Q5 time-skip instance configuration changed")
        if sha256(blockout_file) != blockout_hash_before:
            raise RuntimeError("Protected blockout changed before save")

        if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
            raise RuntimeError("Failed to save Lvl_Village_Main")
        if sha256(blockout_file) != blockout_hash_before:
            raise RuntimeError("Protected blockout changed after save")

        report = {
            "Status": "SUCCESS",
            "GeneratedAtLocal": datetime.datetime.now().isoformat(),
            "Controller": CONTROLLER_LABEL,
            "ControllerClass": controller.get_class().get_name(),
            "QuestID": "Q_HBCT_05_SELF_MONITORING",
            "DisplayActorCount": len(mapped_labels),
            "LogicalDisplayGroupCount": len(DISPLAY_GROUPS),
            "ObjectiveDisplayMapping": expected_mapping,
            "ObjectivesWithNoDisplay": [
                "O02_LearnWhyRecordsMatter", "O04_BuildMonitoringProcess",
                "O13_AdjustThePlan", "O14_PracticeAdjustedRoutine"],
            "TimeSkipSequence": TIME_SKIP_LABEL,
            "TimeSkipConfigurationChanged": False,
            "ExistingActorTransformsChanged": 0,
            "ActorsCreated": [CONTROLLER_LABEL],
            "Q0ToQ4ActorChanges": 0,
            "Q6ActorChanges": 0,
            "MainMapSaved": True,
            "MainMapHashBefore": main_hash_before,
            "MainMapHashAfter": sha256(main_file),
            "BlockoutHashBefore": blockout_hash_before,
            "BlockoutHashAfter": sha256(blockout_file),
            "BlockoutUntouched": True,
            "ManualPIERequired": True,
        }
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        report_path = os.path.join(
            OUTPUT_DIR, "q5_display_visibility_install_%s.json" %
            datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))
        with open(report_path, "w", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2)
        unreal.log("Q5_DISPLAY_VISIBILITY_SUCCESS=" + json.dumps({
            "Displays": len(mapped_labels), "Groups": len(DISPLAY_GROUPS),
            "Report": report_path, "MapSaved": True}))
        return report
    except Exception:
        if controller:
            actor_subsystem.destroy_actor(controller)
        raise


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        failure_path = os.path.join(
            OUTPUT_DIR, "q5_display_visibility_install_failed_%s.json" %
            datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))
        with open(failure_path, "w", encoding="utf-8") as stream:
            json.dump({"Status": "FAILED", "Error": str(exc),
                       "Traceback": traceback.format_exc(), "MainMapSaved": False},
                      stream, indent=2)
        unreal.log_error("Q5_DISPLAY_VISIBILITY_FAILED=" + str(exc))
        raise
