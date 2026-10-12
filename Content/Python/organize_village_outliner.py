"""Organize HBCT actors in Lvl_Village_Main without saving the level."""

import re
import traceback

import unreal


TARGET_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
FORBIDDEN_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout"
LOG_PREFIX = "[VHV HBCT Outliner]"

QUEST_FOLDERS = {
    "prologue": "01_Prologue",
    "motivation": "02_Motivation",
    "goalsetting": "03_GoalSetting",
    "rolemodel": "04_RoleModel",
    "socialsupport": "05_SocialSupport",
    "selfmonitoring": "06_SelfMonitoring",
    "positivereinforcement": "07_PositiveReinforcement",
    "shared": "08_SharedHBCT",
}

SUBFOLDERS = {
    "npc": "01_NPCs",
    "scene": "02_Scenes",
    "location": "03_Locations",
    "trigger": "04_Triggers",
    "interactable": "05_Interactables",
    "walking": "06_WalkingRoutes",
    "other": "99_Other",
}


def _package_path(obj):
    return obj.get_path_name().split(".", 1)[0] if obj else ""


def _current_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return subsystem.get_editor_world() if subsystem else None


def _folder(actor):
    return str(actor.get_folder_path()) if hasattr(actor, "get_folder_path") else ""


def _normalized(value):
    return re.sub(r"[^a-z0-9]+", "", str(value).lower())


def _tokens(value):
    return [token for token in re.split(r"[^a-z0-9]+", str(value).lower()) if token]


def _is_hbct(actor):
    return bool(re.match(r"^hbct(?:_|$)", actor.get_actor_label(), re.IGNORECASE)
                or re.match(r"^hbct(?:_|$)", actor.get_name(), re.IGNORECASE))


def _quest_from_folder(folder):
    segments = [segment for segment in str(folder).replace("\\", "/").split("/") if segment]
    matches = [key for key, value in QUEST_FOLDERS.items()
               if any(_normalized(segment) == _normalized(value) for segment in segments)]
    return matches[0] if len(matches) == 1 else None


def _quest_candidates(actor):
    label_tokens = _tokens(actor.get_actor_label())
    name_tokens = _tokens(actor.get_name())
    combined = "".join(label_tokens + name_tokens)
    candidates = set()
    phrases = {
        "prologue": ("prologue",),
        "motivation": ("motivation",),
        "goalsetting": ("goalsetting", "goalset", "goalplan"),
        "rolemodel": ("rolemodel",),
        "socialsupport": ("socialsupport",),
        "selfmonitoring": ("selfmonitoring", "selfmonitor"),
        "positivereinforcement": ("positivereinforcement", "positivefeedback"),
        "shared": ("sharedhbct", "shared", "commonhbct", "globalhbct"),
    }
    for quest, identifiers in phrases.items():
        if any(identifier in combined for identifier in identifiers):
            candidates.add(quest)

    # In this project, PR is an established prefix for Positive Reinforcement.
    for source_tokens in (label_tokens, name_tokens):
        if len(source_tokens) > 1 and source_tokens[0] == "hbct":
            prefix_quests = {
                "gs": "goalsetting", "rm": "rolemodel", "ss": "socialsupport",
                "sm": "selfmonitoring", "pr": "positivereinforcement",
            }
            if source_tokens[1] in prefix_quests:
                candidates.add(prefix_quests[source_tokens[1]])
    return candidates


def _subfolder(actor):
    label_tokens = set(_tokens(actor.get_actor_label()))
    name_tokens = set(_tokens(actor.get_name()))
    class_text = _normalized(actor.get_class().get_name())
    combined = _normalized(actor.get_actor_label() + "_" + actor.get_name())
    tokens = label_tokens | name_tokens

    if tokens & {"trigger", "triggers"} or "trigger" in combined:
        return "trigger"
    if tokens & {"location", "locations", "semanticlocation"} or "location" in combined:
        return "location"
    if (tokens & {"walking", "walk", "route", "routes", "waypoint", "waypoints", "path"}
            or any(value in combined for value in ("walkingroute", "walkroute", "waypoint"))):
        return "walking"
    if (tokens & {"scene", "scenes", "sequence", "cinematic"}
            or "scene" in combined or "levelsequenceactor" in class_text):
        return "scene"
    if (tokens & {"interactable", "interaction", "interact", "pickup"}
            or "interact" in combined):
        return "interactable"
    if tokens & {"npc", "character"} or "vhvnpc" in class_text:
        return "npc"
    return "other"


def _market_classification(actor):
    class_name = actor.get_class().get_name()
    class_text = _normalized(class_name)
    label_text = _normalized(actor.get_actor_label())
    is_npc = isinstance(actor, unreal.Pawn) or "vhvnpc" in class_text or "npc" in class_text
    is_scene = (isinstance(actor, unreal.LevelSequenceActor)
                or "levelsequence" in class_text or "scene" in class_text)
    if is_npc and is_scene:
        return None, "Market actor class is ambiguous between NPC and scene: {}".format(class_name)
    if is_npc:
        return "{}/{}".format(QUEST_FOLDERS["prologue"], SUBFOLDERS["npc"]), ""
    if is_scene or ("scene" in label_text and not is_npc):
        return "{}/{}".format(QUEST_FOLDERS["prologue"], SUBFOLDERS["scene"]), ""
    return None, "Market actor is neither a confirmed NPC nor scene: {}".format(class_name)


def _classify(actor):
    label = actor.get_actor_label()
    label_lower = label.lower()

    if label_lower == "hbct_intro_instructor":
        return "{}/{}".format(QUEST_FOLDERS["shared"], SUBFOLDERS["npc"]), ""
    if label_lower.startswith("hbct_location_"):
        return "{}/{}".format(QUEST_FOLDERS["shared"], SUBFOLDERS["location"]), ""
    if label_lower.startswith("hbct_market_"):
        return _market_classification(actor)
    if label_lower == "hbct_openingcamerahook":
        return "{}/{}".format(QUEST_FOLDERS["prologue"], SUBFOLDERS["other"]), ""
    if label_lower in {
            "hbct_trigger_badgooddemo",
            "hbct_trigger_prasertobservation",
            "hbct_trigger_saengappreciation"}:
        return "{}/{}".format(QUEST_FOLDERS["motivation"], SUBFOLDERS["trigger"]), ""

    folder_quest = _quest_from_folder(_folder(actor))
    named_candidates = _quest_candidates(actor)

    if len(named_candidates) > 1:
        return None, "multiple quest identifiers: {}".format(", ".join(sorted(named_candidates)))
    named_quest = next(iter(named_candidates)) if named_candidates else None
    if named_quest and folder_quest and named_quest != folder_quest:
        return None, "name identifies {} but existing folder identifies {}".format(
            QUEST_FOLDERS[named_quest], QUEST_FOLDERS[folder_quest])
    quest = named_quest or folder_quest
    if not quest:
        return None, "no unambiguous quest identifier in label, object name, or existing folder"

    subtype = _subfolder(actor)
    return "{}/{}".format(QUEST_FOLDERS[quest], SUBFOLDERS[subtype]), ""


def _parent_path(actor):
    try:
        parent = actor.get_attach_parent_actor()
    except Exception:
        parent = None
    return parent.get_path_name() if parent else ""


def _snapshot(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    try:
        tags = tuple(sorted(str(tag) for tag in actor.get_editor_property("tags")))
    except Exception:
        tags = ()
    return {
        "object_path": actor.get_path_name(),
        "label": actor.get_actor_label(),
        "class": actor.get_class().get_path_name(),
        "location": (float(location.x), float(location.y), float(location.z)),
        "rotation": (float(rotation.pitch), float(rotation.yaw), float(rotation.roll)),
        "scale": (float(scale.x), float(scale.y), float(scale.z)),
        "tags": tags,
        "attach_parent": _parent_path(actor),
    }


def _collect_hbct(world):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not subsystem:
        raise RuntimeError("EditorActorSubsystem is unavailable")
    actors = [actor for actor in subsystem.get_all_level_actors()
              if actor and actor.get_world() == world and _is_hbct(actor)]
    return subsystem, actors


def _is_root(actor):
    folder = _folder(actor).strip()
    return not folder or folder.lower() == "none"


def run():
    world = _current_world()
    world_path = _package_path(world)
    if not world or world_path != TARGET_MAP or world_path == FORBIDDEN_MAP:
        raise RuntimeError(
            "Loaded editor world must be {}; found {}".format(TARGET_MAP, world_path or "None"))

    actor_subsystem, actors = _collect_hbct(world)
    actors.sort(key=lambda actor: (actor.get_actor_label().lower(), actor.get_path_name().lower()))
    before = {actor.get_path_name(): _snapshot(actor) for actor in actors}
    if len(before) != len(actors):
        raise RuntimeError("Duplicate HBCT actor object identities detected")

    plan = []
    unresolved = []
    counts = {folder: 0 for folder in QUEST_FOLDERS.values()}
    for actor in actors:
        desired_folder, reason = _classify(actor)
        if not desired_folder:
            unresolved.append({
                "actor": actor.get_actor_label(),
                "object_path": actor.get_path_name(),
                "reason": reason,
            })
            continue
        quest_folder = desired_folder.split("/", 1)[0]
        counts[quest_folder] += 1
        plan.append({
            "actor": actor,
            "label": actor.get_actor_label(),
            "old_folder": _folder(actor),
            "new_folder": desired_folder,
        })

    # The complete dry-run plan exists before the first actor is modified.
    moves = [entry for entry in plan if entry["old_folder"] != entry["new_folder"]]
    unreal.log("{} Dry run: inspected={}, classified={}, moves={}, unresolved={}".format(
        LOG_PREFIX, len(actors), len(plan), len(moves), len(unresolved)))

    changed = []
    try:
        if moves:
            with unreal.ScopedEditorTransaction("Organize VHV HBCT World Outliner"):
                for entry in moves:
                    actor = entry["actor"]
                    actor.modify()
                    actor.set_folder_path(unreal.Name(entry["new_folder"]))
                    changed.append(entry)

        _, after_actors = _collect_hbct(world)
        after = {actor.get_path_name(): _snapshot(actor) for actor in after_actors}
        if len(after_actors) != len(actors) or set(after) != set(before):
            raise RuntimeError("HBCT actor count or identities changed during folder organization")
        for actor_path, original in before.items():
            if after[actor_path] != original:
                raise RuntimeError("Non-folder actor state changed: {}".format(actor_path))
        for entry in plan:
            if _folder(entry["actor"]) != entry["new_folder"]:
                raise RuntimeError("Folder verification failed for {}".format(entry["label"]))
        unresolved_paths = {item["object_path"] for item in unresolved}
        root_actors = [actor for actor in after_actors if _is_root(actor)]
        unexpected_root = [actor.get_actor_label() for actor in root_actors
                           if actor.get_path_name() not in unresolved_paths]
        if unexpected_root:
            raise RuntimeError("Classified HBCT actors remain at root: {}".format(
                ", ".join(sorted(unexpected_root))))
    except Exception:
        for entry in reversed(changed):
            entry["actor"].set_folder_path(unreal.Name(entry["old_folder"]))
        raise

    root_count = sum(1 for actor in after_actors if _is_root(actor))
    unreal.log("{} Complete: inspected={}, moved={}, already_correct={}, root_remaining={}, unresolved={}".format(
        LOG_PREFIX, len(actors), len(moves), len(plan) - len(moves), root_count, len(unresolved)))
    unreal.log("{} Quest counts: {}".format(
        LOG_PREFIX, ", ".join("{}={}".format(folder, counts[folder])
                             for folder in sorted(counts))))
    for item in unresolved:
        unreal.log_warning("{} Unresolved: {} -- {}".format(
            LOG_PREFIX, item["actor"], item["reason"]))
    unreal.log("{} Verification passed: actor identities, counts, labels, transforms, classes, tags, "
               "attachments, and gameplay state were unchanged; only folder paths changed.".format(
                   LOG_PREFIX))
    unreal.log("{} Level was not saved; review the dirty map and save manually if approved.".format(
        LOG_PREFIX))
    return {
        "inspected": len(actors),
        "moved": len(moves),
        "already_correct": len(plan) - len(moves),
        "root_remaining": root_count,
        "quest_counts": counts,
        "unresolved": unresolved,
        "saved": False,
    }


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG_PREFIX, traceback.format_exc()))
        raise
