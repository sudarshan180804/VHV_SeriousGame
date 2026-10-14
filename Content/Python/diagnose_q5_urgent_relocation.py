"""Read-only snapshot and diagnosis for the urgent Q5 relocation repair."""

import datetime
import hashlib
import json
import os

import unreal


TARGET_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
PROTECTED_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout"
QUEST_ARC = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
QUEST_ID = "Q_HBCT_05_SELF_MONITORING"
OUTPUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "VHV_Q5UrgentFix")


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
    return [
        float(location.x), float(location.y), float(location.z),
        float(rotation.pitch), float(rotation.yaw), float(rotation.roll),
        float(scale.x), float(scale.y), float(scale.z),
    ]


def actor_label(actor):
    return actor.get_actor_label() if actor else ""


def component_state(component):
    result = {
        "Name": component.get_name(),
        "Class": component.get_class().get_name(),
    }
    for prop, key in (("visible", "Visible"), ("hidden_in_game", "HiddenInGame")):
        try:
            result[key] = bool(component.get_editor_property(prop))
        except Exception:
            result[key] = None
    try:
        result["CollisionEnabled"] = str(component.get_collision_enabled())
    except Exception:
        result["CollisionEnabled"] = ""
    if isinstance(component, unreal.TextRenderComponent):
        result["Text"] = str(component.get_editor_property("text"))
    return result


def box_extent(actor):
    boxes = list(actor.get_components_by_class(unreal.BoxComponent) or [])
    return [{
        "Name": box.get_name(),
        "Extent": [float(value) for value in box.get_unscaled_box_extent().to_tuple()],
        "CollisionProfile": str(box.get_collision_profile_name()),
        "CollisionEnabled": str(box.get_collision_enabled()),
    } for box in boxes]


def decode_hit(hit):
    if hit is None:
        return None
    if isinstance(hit, tuple) and len(hit) == 2 and isinstance(hit[0], bool):
        if not hit[0]:
            return None
        hit = hit[1]
    try:
        values = hit.to_tuple()
    except Exception:
        return None
    if len(values) < 18:
        return None
    return {
        "Blocking": bool(values[0]),
        "Impact": [float(values[5].x), float(values[5].y), float(values[5].z)],
        "Actor": actor_label(values[9]),
        "Component": values[10].get_name() if values[10] else "",
    }


def vertical_hits(world, location):
    start = unreal.Vector(float(location.x), float(location.y), float(location.z) + 3000.0)
    end = unreal.Vector(float(location.x), float(location.y), float(location.z) - 10000.0)
    raw_hits = unreal.SystemLibrary.line_trace_multi(
        world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
        unreal.DrawDebugTrace.NONE, True) or []
    return [decoded for decoded in (decode_hit(hit) for hit in raw_hits) if decoded]


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world_path = world.get_path_name().split(":", 1)[0] if world else ""
    if not world or not (world_path == TARGET_MAP or world_path.startswith(TARGET_MAP + ".")):
        raise RuntimeError("Lvl_Village_Main did not load")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    by_label = {}
    for actor in actors:
        by_label.setdefault(actor_label(actor), []).append(actor)

    all_transforms = {actor.get_path_name(): {
        "Label": actor_label(actor),
        "Class": actor.get_class().get_name(),
        "Transform": transform(actor),
    } for actor in actors}

    q5_actor_rows = []
    trigger_rows = []
    for actor in actors:
        label = actor_label(actor)
        if label.startswith("HBCT_SM_") or label.startswith("VHV_Q5_"):
            q5_actor_rows.append({
                "Path": actor.get_path_name(), "Label": label,
                "Class": actor.get_class().get_name(), "Transform": transform(actor),
                "ActorHiddenInGame": bool(actor.get_editor_property("hidden")),
                "ActorCollisionEnabled": bool(actor.get_actor_enable_collision()),
            })
        boxes = box_extent(actor)
        if boxes and (label.startswith("HBCT_SM_") or label.startswith("VHV_Q5_")):
            trigger_rows.append({
                "Path": actor.get_path_name(), "Label": label,
                "Class": actor.get_class().get_name(), "Transform": transform(actor),
                "Boxes": boxes,
            })

    sequence_rows = []
    for actor in actors:
        if actor.get_class().get_name() != "VHVTimeSkipSequence":
            continue
        stages = []
        for stage in list(actor.get_editor_property("stages") or []):
            arrival = stage.get_editor_property("player_arrival_target")
            support = stage.get_editor_property("player_arrival_support_actor")
            scene = stage.get_editor_property("observation_scene")
            trigger = stage.get_editor_property("legacy_trigger")
            stages.append({
                "StageID": str(stage.get_editor_property("stage_id")),
                "ObjectiveID": str(stage.get_editor_property("objective_id")),
                "TransitionText": str(stage.get_editor_property("transition_text")),
                "ObservationScene": actor_label(scene),
                "StageActors": [actor_label(item) for item in list(stage.get_editor_property("stage_actors") or [])],
                "ArrivalTarget": actor_label(arrival),
                "ArrivalTransform": transform(arrival) if arrival else None,
                "ArrivalSupport": actor_label(support),
                "ArrivalVerticalHits": vertical_hits(world, arrival.get_actor_location()) if arrival else [],
                "LegacyTrigger": actor_label(trigger),
                "AutoStart": bool(stage.get_editor_property("auto_start_observation_after_transition")),
                "PostDelay": float(stage.get_editor_property("post_transition_observation_delay")),
                "PresentationOnly": bool(stage.get_editor_property("presentation_only")),
            })
        sequence_rows.append({
            "Path": actor.get_path_name(), "Label": actor_label(actor),
            "QuestID": str(actor.get_editor_property("required_quest_id")),
            "RequiredObjectiveID": str(actor.get_editor_property("required_objective_id")),
            "AdvanceOnObjectiveCompletion": bool(actor.get_editor_property("advance_on_objective_completion")),
            "TransitionOnNextObjectiveActivation": bool(actor.get_editor_property("transition_on_next_objective_activation")),
            "RequireNavigationProjection": bool(actor.get_editor_property("require_navigation_projection")),
            "FadeIn": float(actor.get_editor_property("fade_in_duration")),
            "Hold": float(actor.get_editor_property("text_hold_duration")),
            "FadeOut": float(actor.get_editor_property("fade_out_duration")),
            "Stages": stages,
        })

    display_rows = []
    for actor in actors:
        if actor.get_class().get_name() != "VHVQ5DisplayVisibilityController":
            continue
        mapping = {}
        for group in list(actor.get_editor_property("objective_displays") or []):
            mapping[str(group.get_editor_property("objective_id"))] = [
                actor_label(item) for item in list(group.get_editor_property("display_actors") or [])]
        display_rows.append({
            "Path": actor.get_path_name(), "Label": actor_label(actor),
            "QuestID": str(actor.get_editor_property("required_quest_id")),
            "Mapping": mapping,
            "PrimarySequence": actor_label(actor.get_editor_property("time_skip_sequence")),
            "AdditionalSequences": [actor_label(item) for item in list(
                actor.get_editor_property("additional_time_skip_sequences") or [])],
        })

    display_details = {}
    for label in ("HBCT_SM_Display_BreakfastRecord", "HBCT_SM_Display_AfternoonRecord", "HBCT_SM_Display_CompleteDay"):
        matches = by_label.get(label, [])
        if len(matches) == 1:
            actor = matches[0]
            primitives = list(actor.get_components_by_class(unreal.PrimitiveComponent) or [])
            display_details[label] = {
                "Path": actor.get_path_name(), "Transform": transform(actor),
                "ActorHiddenInGame": bool(actor.get_editor_property("hidden")),
                "ActorCollisionEnabled": bool(actor.get_actor_enable_collision()),
                "Components": [component_state(component) for component in primitives],
            }

    arc = unreal.EditorAssetLibrary.load_asset(QUEST_ARC)
    quests = list(arc.get_editor_property("quests") or []) if arc else []
    quest = next((item for item in quests if str(item.get_editor_property("quest_id")) == QUEST_ID), None)
    objective_ids = [str(item.get_editor_property("objective_id"))
                     for item in list(quest.get_editor_property("objectives") or [])] if quest else []

    project_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    main_file = os.path.join(project_dir, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
    blockout_file = os.path.join(project_dir, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Blockout.umap")
    report = {
        "Status": "PASS",
        "GeneratedAtLocal": datetime.datetime.now().isoformat(),
        "Map": TARGET_MAP,
        "ActorCount": len(actors),
        "AllActorTransforms": all_transforms,
        "Q5Actors": q5_actor_rows,
        "Q5TriggerSnapshots": trigger_rows,
        "TimeSkipSequences": sequence_rows,
        "DisplayControllers": display_rows,
        "RelevantDisplays": display_details,
        "Q5ObjectiveIDs": objective_ids,
        "MainMapHash": sha256(main_file),
        "BlockoutHash": sha256(blockout_file),
        "SavedAnything": False,
    }
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    path = os.path.join(OUTPUT_DIR, "q5_urgent_prechange_snapshot.json")
    with open(path, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, sort_keys=True)
    unreal.log("Q5_URGENT_DIAG=" + json.dumps({
        "Report": path, "Sequences": len(sequence_rows),
        "Q5Actors": len(q5_actor_rows), "Triggers": len(trigger_rows),
        "SavedAnything": False,
    }, sort_keys=True))


main()
