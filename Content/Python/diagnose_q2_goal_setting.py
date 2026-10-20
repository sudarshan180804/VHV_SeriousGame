"""Read-only serialized-asset audit for Technique 2 (Goal Setting).

Run with UnrealEditor-Cmd -ExecutePythonScript. The report is written under
Saved/Diagnostics so the project assets remain untouched.
"""

import json
import os
import traceback
import unreal


LOG = "[VHV Q2 Audit]"
ARC_PATH = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
MAP_PATHS = (
    "/Game/VHV_Stuff/Maps/Lvl_Village_Main",
    "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout",
)
QUEST_ID = "Q_HBCT_02_GOAL_SETTING"


def vector(value):
    return [round(value.x, 3), round(value.y, 3), round(value.z, 3)]


def rotation(value):
    return [round(value.pitch, 3), round(value.yaw, 3), round(value.roll, 3)]


def tag_text(value):
    if not value:
        return ""
    try:
        return str(value.get_editor_property("tag_name"))
    except Exception:
        return str(value)


def safe_property(owner, name, default=None):
    try:
        return owner.get_editor_property(name)
    except Exception:
        return default


def component_snapshot(actor):
    result = {}
    try:
        participant = actor.get_quest_participant_component()
    except Exception:
        participant = safe_property(actor, "quest_participant_component")
    if participant:
        result["participant"] = {
            "enabled": bool(safe_property(participant, "quest_participation_enabled", False)),
            "tag": tag_text(safe_property(participant, "participant_tag")),
            "effective_id": str(participant.get_effective_participant_id()),
        }
    try:
        interaction = actor.get_npc_interaction_component()
    except Exception:
        interaction = safe_property(actor, "npc_interaction_component")
    if interaction:
        result["interaction"] = {
            "enabled": bool(safe_property(interaction, "interaction_enabled", False)),
            "distance": safe_property(interaction, "interaction_distance"),
            "prompt": str(safe_property(interaction, "default_interaction_prompt", "")),
        }
    box = safe_property(actor, "box_component")
    if box:
        result["box_extent"] = vector(box.get_unscaled_box_extent())
        result["collision_enabled"] = str(box.get_collision_enabled())
    try:
        capsule = actor.get_component_by_class(unreal.CapsuleComponent)
    except Exception:
        capsule = None
    if capsule:
        result["capsule"] = {
            "radius": capsule.get_unscaled_capsule_radius(),
            "half_height": capsule.get_unscaled_capsule_half_height(),
            "relative_location": vector(safe_property(
                capsule, "relative_location", unreal.Vector())),
            "collision_enabled": str(capsule.get_collision_enabled()),
        }
    return result


def actor_snapshot(actor):
    result = {
        "label": actor.get_actor_label(),
        "object_name": actor.get_name(),
        "class": actor.get_class().get_path_name(),
        "location": vector(actor.get_actor_location()),
        "rotation": rotation(actor.get_actor_rotation()),
        "folder": str(actor.get_folder_path()),
        "actor_tags": sorted(str(value) for value in actor.tags),
        "components": component_snapshot(actor),
    }
    if isinstance(actor, unreal.VHVQuestLocationVolume):
        result["semantic_location"] = {
            "tag": tag_text(safe_property(actor, "location_tag")),
            "effective_id": str(actor.get_effective_location_id()),
            "enabled": bool(safe_property(actor, "enabled", False)),
            "required_quest": str(safe_property(actor, "required_active_quest_id", "")),
            "required_objective": str(safe_property(actor, "required_active_objective_id", "")),
            "activate_objective": bool(safe_property(actor, "activate_current_objective", False)),
            "trigger_world_action": bool(safe_property(actor, "trigger_world_action", False)),
        }
    meshes = []
    try:
        mesh_components = actor.get_components_by_class(unreal.StaticMeshComponent)
    except Exception:
        mesh_components = []
    for component in mesh_components:
        mesh = safe_property(component, "static_mesh")
        if mesh:
            meshes.append(mesh.get_path_name())
    if meshes:
        result["static_meshes"] = sorted(set(meshes))
    return result


def is_relevant(actor):
    searchable = " ".join((actor.get_actor_label(), actor.get_name())).lower()
    if any(token in searchable for token in (
            "goal", "noticeboard", "instructor", "healthpost", "health_post", "mali")):
        return True
    if isinstance(actor, unreal.VHVQuestLocationVolume):
        location = tag_text(safe_property(actor, "location_tag")).lower()
        if "goalsetting" in location or location.endswith("healthpost"):
            return True
    try:
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
    except Exception:
        components = []
    for component in components:
        mesh = safe_property(component, "static_mesh")
        if mesh and (any(token in mesh.get_path_name().lower()
                         for token in ("goal", "noticeboard"))
                     or mesh.get_name() in ("P2", "P3")):
            return True
    return False


def objective_snapshot(objective):
    readiness = []
    for move in safe_property(objective, "activation_npc_readiness", []) or []:
        readiness.append({
            "participant": tag_text(safe_property(move, "participant_tag")),
            "destination": tag_text(safe_property(move, "destination_location_tag")),
            "face_rotation": bool(safe_property(move, "face_destination_rotation", False)),
        })
    tracking_destinations = []
    for destination in safe_property(objective, "tracking_destinations", []) or []:
        tracking_destinations.append(tag_text(
            safe_property(destination, "destination_location_tag")))
    return {
        "id": str(safe_property(objective, "objective_id", "")),
        "text": str(safe_property(objective, "objective_text", "")),
        "type": str(safe_property(objective, "objective_type", "")),
        "participant": tag_text(safe_property(objective, "participant_tag")),
        "location": tag_text(safe_property(objective, "location_tag")),
        "conversation": str(safe_property(objective, "conversation", "")),
        "entry_node": str(safe_property(objective, "entry_node_id", "")),
        "auto_start": bool(safe_property(objective, "auto_start", False)),
        "ensure_participant": bool(safe_property(
            objective, "ensure_participant_present_at_location", False)),
        "prepare_before_exposure": bool(safe_property(
            objective, "prepare_activation_npc_readiness_before_exposure", False)),
        "activation_readiness": readiness,
        "tracking_enabled": bool(safe_property(objective, "enable_location_tracking", False)),
        "tracking_location": tag_text(safe_property(objective, "tracking_location_tag")),
        "tracking_destinations": tracking_destinations,
    }


def run():
    report = {"quest_id": QUEST_ID, "objectives": [], "maps": {}}
    arc = unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    if not arc:
        raise RuntimeError("Missing {}".format(ARC_PATH))
    quest = next((item for item in arc.get_editor_property("quests")
                  if str(item.get_editor_property("quest_id")) == QUEST_ID), None)
    if not quest:
        raise RuntimeError("Missing {}".format(QUEST_ID))
    report["objectives"] = [objective_snapshot(item)
                            for item in quest.get_editor_property("objectives")]

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for map_path in MAP_PATHS:
        if not unreal.EditorLoadingAndSavingUtils.load_map(map_path):
            report["maps"][map_path] = {"error": "load failed"}
            continue
        actors = actor_subsystem.get_all_level_actors()
        relevant = [actor_snapshot(actor) for actor in actors if is_relevant(actor)]
        instructor_participants = []
        for actor in actors:
            try:
                participant = actor.get_quest_participant_component()
            except Exception:
                participant = safe_property(actor, "quest_participant_component")
            if participant and str(participant.get_effective_participant_id()) == "Instructor":
                instructor_participants.append(actor_snapshot(actor))
        report["maps"][map_path] = {
            "actor_count": len(actors),
            "relevant_actors": relevant,
            "instructor_participants": instructor_participants,
        }

    output_dir = os.path.join(unreal.Paths.project_saved_dir(), "Diagnostics")
    os.makedirs(output_dir, exist_ok=True)
    for asset_path in ("/Game/3D_Models/P2", "/Game/3D_Models/P3"):
        mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not mesh:
            continue
        task = unreal.AssetExportTask()
        task.set_editor_properties({
            "object": mesh,
            "filename": os.path.join(output_dir, mesh.get_name() + ".obj"),
            "automated": True,
            "prompt": False,
            "replace_identical": True,
            "exporter": unreal.StaticMeshExporterOBJ(),
        })
        unreal.Exporter.run_asset_export_task(task)
    output_path = os.path.join(output_dir, "q2_goal_setting_audit.json")
    with open(output_path, "w", encoding="utf-8") as output:
        json.dump(report, output, indent=2, sort_keys=True)
    unreal.log("{} Wrote {}".format(LOG, output_path))


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} {}".format(LOG, traceback.format_exc()))
        raise
