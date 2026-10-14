"""Read-only validation for the Q5 O07/O08 world-display visibility fix."""

import hashlib
import json
import os

import unreal


TARGET_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
CONTROLLER_LABEL = "VHV_Q5_ObjectiveDisplayVisibility"
EXPECTED = {
    "O07_RecordBreakfast": (
        "HBCT_SM_Display_BreakfastRecord",
        "TODAY",
        "Vegetables",
        "Sweet Drink",
    ),
    "O08_RecordAfternoon": (
        "HBCT_SM_Display_AfternoonRecord",
        "TODAY",
        "Vegetables",
        "Sweet Drink",
    ),
}


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def transform(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return (
        float(location.x), float(location.y), float(location.z),
        float(rotation.pitch), float(rotation.yaw), float(rotation.roll),
        float(scale.x), float(scale.y), float(scale.z),
    )


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world_path = world.get_path_name().split(":", 1)[0] if world else ""
    if not world or not (world_path == TARGET_MAP or world_path.startswith(TARGET_MAP + ".")):
        raise RuntimeError("Lvl_Village_Main is not the active editor world")

    actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    by_label = {}
    for actor in actors:
        by_label.setdefault(actor.get_actor_label(), []).append(actor)
    transforms_before = {actor.get_path_name(): transform(actor) for actor in actors}

    controllers = by_label.get(CONTROLLER_LABEL, [])
    if len(controllers) != 1:
        raise RuntimeError("Expected exactly one Q5 display controller")
    controller = controllers[0]
    if controller.get_class().get_name() != "VHVQ5DisplayVisibilityController":
        raise RuntimeError("Q5 display controller has the wrong class")

    configured = {}
    all_configured_labels = []
    for group in controller.get_editor_property("objective_displays"):
        objective_id = str(group.get_editor_property("objective_id"))
        labels = [actor.get_actor_label() for actor in group.get_editor_property("display_actors")]
        configured[objective_id] = labels
        all_configured_labels.extend(labels)

    details = {}
    for objective_id, expected in EXPECTED.items():
        label, *required_text = expected
        if configured.get(objective_id) != [label]:
            raise RuntimeError("{} mapping is {} instead of {}".format(
                objective_id, configured.get(objective_id), [label]))
        matches = by_label.get(label, [])
        if len(matches) != 1:
            raise RuntimeError("Expected exactly one {}".format(label))
        actor = matches[0]
        component = actor.get_component_by_class(unreal.TextRenderComponent)
        if not component:
            raise RuntimeError("{} has no TextRenderComponent".format(label))
        display_text = str(component.get_editor_property("text"))
        missing_text = [value for value in required_text if value not in display_text]
        if missing_text:
            raise RuntimeError("{} is missing authored text {}".format(label, missing_text))
        details[objective_id] = {
            "Actor": label,
            "Text": display_text,
            "ComponentVisibleDefault": bool(component.get_editor_property("visible")),
            "ComponentHiddenInGameDefault": bool(component.get_editor_property("hidden_in_game")),
            "WorldTransform": list(transform(actor)),
        }

    if len(all_configured_labels) != 36 or len(set(all_configured_labels)) != 36:
        raise RuntimeError("Expected 36 unique configured Q5 display actors")

    component_inventory = {}
    helper_component_classes = {}
    for label in all_configured_labels:
        matches = by_label.get(label, [])
        if len(matches) != 1:
            raise RuntimeError("Expected exactly one configured display actor {}".format(label))
        actor = matches[0]
        components = list(actor.get_components_by_class(unreal.PrimitiveComponent))
        gameplay_components = [component for component in components if isinstance(
            component, (unreal.TextRenderComponent, unreal.WidgetComponent))]
        if not gameplay_components:
            raise RuntimeError("{} has no gameplay text/widget component".format(label))
        helper_components = [component for component in components
                             if component not in gameplay_components]
        helper_classes = [component.get_class().get_name() for component in helper_components]
        component_inventory[label] = {
            "Gameplay": [component.get_class().get_name() for component in gameplay_components],
            "Helpers": helper_classes,
        }
        for class_name in helper_classes:
            helper_component_classes[class_name] = helper_component_classes.get(class_name, 0) + 1

    time_skip = controller.get_editor_property("time_skip_sequence")
    if not time_skip or time_skip.get_actor_label() != "VHV_Q5_TimeSkip_DailyRecording":
        raise RuntimeError("Q5 display controller has the wrong time-skip reference")
    stage_displays = {}
    for stage in time_skip.get_editor_property("stages"):
        objective_id = str(stage.get_editor_property("objective_id"))
        labels = [actor.get_actor_label() for actor in stage.get_editor_property("stage_actors")]
        stage_displays[objective_id] = [label for label in labels if label.startswith("HBCT_SM_Display_")]
    for objective_id, expected in EXPECTED.items():
        if expected[0] not in stage_displays.get(objective_id, []):
            raise RuntimeError("{} time-skip stage does not reference {}".format(
                objective_id, expected[0]))

    transforms_after = {actor.get_path_name(): transform(actor) for actor in actors}
    changed = [path for path, value in transforms_before.items()
               if transforms_after.get(path) != value]
    if changed:
        raise RuntimeError("Editor validation changed transforms: {}".format(changed))

    project_dir = unreal.Paths.project_dir()
    main_file = os.path.join(project_dir, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
    blockout_file = os.path.join(project_dir, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Blockout.umap")
    report = {
        "Status": "PASS",
        "Controller": CONTROLLER_LABEL,
        "ConfiguredDisplayActors": len(all_configured_labels),
        "O07O08": details,
        "DisplayComponentInventory": component_inventory,
        "HelperComponentClasses": helper_component_classes,
        "TimeSkipStageDisplays": stage_displays,
        "ExistingActorTransformsChanged": len(changed),
        "MainMapHash": sha256(main_file),
        "BlockoutHash": sha256(blockout_file),
        "MapSavedByValidation": False,
    }
    unreal.log("Q5_DISPLAY_FIX_VALIDATION=" + json.dumps(report, sort_keys=True))


main()
