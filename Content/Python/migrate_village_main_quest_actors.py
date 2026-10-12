"""Report and apply the Quest 0-6 spatial migration in Lvl_Village_Main.

Run REPORT_ONLY (the default) from Unreal's Python console:

    import migrate_village_main_quest_actors as migration
    report = migration.run(migration.REPORT_ONLY)

Review the JSON written below Saved/VHV_QuestMigration.  APPLY deliberately
requires the report id from an accepted, clean REPORT_ONLY result:

    migration.run(migration.APPLY, accepted_report_id="<ReportId>")

This tool never loads a map.  It operates only on an already-loaded
Lvl_Village_Main, never creates/deletes actors, never edits actor properties
other than transforms, and saves only after the complete APPLY verification.
"""

from __future__ import print_function

import collections
import datetime
import hashlib
import json
import math
import os
import re
import sys
import traceback

import unreal


REPORT_ONLY = "REPORT_ONLY"
APPLY = "APPLY"
DIAGNOSTIC_ONLY = "DIAGNOSTIC_ONLY"
BLOCKER_DIAGNOSTIC = "BLOCKER_DIAGNOSTIC"
AFTERNOON_LOCAL_DIAGNOSTIC = "AFTERNOON_LOCAL_DIAGNOSTIC"
Q4_ROUTE_DIAGNOSTIC = "Q4_ROUTE_DIAGNOSTIC"
TARGET_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
PROTECTED_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout"
REPORT_DIRECTORY = os.path.join(unreal.Paths.project_saved_dir(), "VHV_QuestMigration")
LOG = "[VHV Quest Migration]"

QUEST_IDS = (
    "Q_HBCT_00_INTRO",
    "Q_HBCT_01_MOTIVATION",
    "Q_HBCT_02_GOAL_SETTING",
    "Q_HBCT_03_ROLE_MODEL",
    "Q_HBCT_04_SOCIAL_SUPPORT",
    "Q_HBCT_05_SELF_MONITORING",
    "Q_HBCT_06_POSITIVE_REINFORCEMENT",
)

# Screenshot registry is comparison-only.  It is never used to position actors.
SCREENSHOT_REGISTRY = {
    "PLAYER_START": ((0.0, 0.0, 240.0), 139.99),
    "P4_MAIN_MARKET": ((-2349.0, 2610.0, 62.786), 90.0),
    "H2_PRASERT": ((612.0, 4960.0, 81.587), 90.0),
    "H3_SAENG": ((-2069.0, 4893.9, 74.497), -79.99),
    "INSTRUCTOR_SHELTER": ((312.67, 6896.7, 360.38), 89.999),
    "P1_HEALTH_POST": ((-4600.0, 9694.6, 69.894), -99.99),
    "HEALTH_GATE": ((-1387.0, 8999.9, 491.91), 264.99),
    "GENERIC_H7": ((-1841.1, 11985.0, 110.0), -84.99),
    "P3_EXERCISE_PARK": ((1552.0, 13813.3, 1199.0), -139.9),
    "H8_FAMILY": ((-3545.1, 15620.0, 98.0), -54.9),
    "P2_PAVILION": ((-7360.0, 16950.0, 99.0), 0.0),
    "H7_NEIGHBOR": ((-8881.1, 19565.0, 116.0), 180.0),
}

LANDMARK_SPECS = collections.OrderedDict([
    ("PLAYER_START", dict(labels=("PlayerStart",), classes=("PlayerStart",), critical=True)),
    ("P4_MAIN_MARKET", dict(labels=("VM_P4_MainMarket",), meshes=("/Game/3D_Models/P4",), critical=True)),
    ("H2_PRASERT", dict(labels=("VM_H2_PrasertHouse",), meshes=("/Game/3D_Models/H2",), critical=True)),
    # Level-design authority: this exact actor is Saeng's residence regardless
    # of its assigned Static Mesh.  The separate generic H3 is decoration and
    # must never be considered for quest placement.
    ("H3_SAENG", dict(labels=("VM_H3_SaengHouse",), critical=True,
                       story_tokens=("saeng",))),
    ("H1_MALI", dict(labels=("VM_H1_MaliHouse", "H1"), meshes=("/Game/3D_Models/H1",), critical=True,
                      story_tokens=("mali",))),
    ("H6_CHAI", dict(labels=("VM_H6_ChaiHouse", "H6"), meshes=("/Game/3D_Models/H6",), critical=True,
                      story_tokens=("chai",))),
    ("H7_NEIGHBOR", dict(labels=("VM_H7_NeighborHouse",), meshes=("/Game/3D_Models/H7",), critical=True,
                          story_tokens=("neighbor",))),
    ("GENERIC_H7", dict(labels=("H7",), meshes=("/Game/3D_Models/H7",), critical=False)),
    ("H8_FAMILY", dict(labels=("VM_H8_FamilyHouse", "H8"), meshes=("/Game/3D_Models/H8",), critical=True)),
    ("P1_HEALTH_POST", dict(labels=("VM_P1_HealthPost",), meshes=("/Game/3D_Models/P1",), critical=True)),
    ("HEALTH_GATE", dict(labels=("VHV_Gate_HealthCenter_01",),
                         meshes=("/Game/3D_Models/VHV_Gate_HealthCenter_01",), critical=False)),
    ("P2_PAVILION", dict(labels=("VM_P2_CommunityPavilion",), meshes=("/Game/3D_Models/P2",), critical=True)),
    ("P3_EXERCISE_PARK", dict(labels=("VM_P3_ExercisePark",), meshes=("/Game/3D_Models/P3",), critical=True)),
    ("P5_KIOSK", dict(labels=("VM_P5_MarketKiosk",), meshes=("/Game/3D_Models/P5",), critical=True)),
    ("P6_DRINK_STALL", dict(labels=("VM_P6_DrinkStall",), meshes=("/Game/3D_Models/P6",), critical=True)),
    ("P7_SHOP_ROW", dict(labels=("VM_P7_ShopRow",), meshes=("/Game/3D_Models/P7",), critical=True)),
    ("VILLAGE_ENTRANCE", dict(labels=("VHV_Gate_VillageEntrance_01", "VM_Gate_VillageEntrance"),
                              meshes=("/Game/3D_Models/VHV_Gate_VillageEntrance_01",), critical=True)),
    ("INSTRUCTOR_SHELTER", dict(labels=("VHV_Shelter_Instructor_01",),
                                meshes=("/Game/3D_Models/VHV_Shelter_Instructor_01",), critical=True)),
])

# Stable station indices create separated clusters around large landmarks.  House
# objectives deliberately reuse a single porch/yard cluster as requested.
STATION_INDEX = {
    "entrance_intro": 0, "entrance_debrief": 1,
    "market_p6_vendor": 0, "market_p6_drink": 1, "market_p7_smoker": 0,
    "market_p5_junk": 0, "market_roadside": 2, "market_quiet": 3,
    "prasert_yard": 0, "motivation_bad": 0, "motivation_good": 1,
    "motivation_reflection": 2, "saeng_yard": 0, "mali_yard": 0,
    "clinic_initial": 0, "clinic_lesson": 1, "clinic_clear": 2,
    "clinic_nogoal": 3, "clinic_self": 4, "clinic_joint": 5,
    "clinic_evaluate": 6, "clinic_feedback": 7,
    "park_exercise": 0, "park_similar": 1, "park_family": 2,
    "park_symbolic": 3, "park_reflection": 4, "park_candidates": 5,
    "park_timeline_before": 6, "park_timeline_model": 7,
    "park_timeline_attempt": 8, "park_timeline_repeat": 9,
    "park_timeline_routine": 10, "park_demo": 11, "park_debrief": 12,
    "pavilion_emotional": 0, "pavilion_information": 1,
    "pavilion_instrumental": 2, "pavilion_appraisal": 3,
    "pavilion_board": 4, "pavilion_reflection": 5, "pavilion_group": 6,
    "pavilion_target": 7, "pavilion_process": 8, "pavilion_method": 9,
    "pavilion_pattern": 10, "pavilion_graph": 11,
    "fair_social": 0, "fair_material": 1, "fair_activity": 2,
    "fair_token": 3, "fair_feedback": 4, "fair_preferences": 5,
    "fair_timing": 6, "chai_home": 0, "neighbor_home": 0,
    "family_meal": 0,
}

# Designer-reviewed overrides are deliberately limited to records that failed
# the 2026-10-11 physical preflight.  Values are populated only after the
# bounded live-world candidate diagnostic has passed.
PHYSICAL_PLACEMENT_OVERRIDES = {
    "HBCT_Market_SugarDrinkShopper": {
        "XY": (-3210.061, 3166.419), "Yaw": -17.5,
        "Reason": "Faces SweetTea participant from clear ground beside P6; outside P4 collision."},
    "HBCT_Market_Vendor": {
        "XY": (-3746.579, 2842.833), "Yaw": -33.0,
        "Reason": "Clear west side of the /Game/3D_Models/p8 stand; faces the stand."},
    "HBCT_Trigger_SaengAppreciation": {
        "XY": (-2789.820, 4065.454), "Yaw": 83.01,
        "Reason": "Open approach-side yard ground associated with VM_H3_SaengHouse."},
    "HBCT_Location_RoleModel_WalkMid_Saeng": {
        "XY": (-2757.702, 15470.802), "Yaw": -19.392,
        "Reason": "East-side detour around H8 on the P3-to-community walking corridor."},
    "HBCT_Location_RoleModel_WalkMid_Somchai": {
        "XY": (-2557.702, 15470.802), "Yaw": -19.392,
        "Reason": "Second midpoint 200 cm east of Saeng, preserving separate movement stages."},
    "HBCT_SM_Location_AdjustedWalkEnd": {
        "XY": (-7278.775, 17608.577), "Yaw": 72.561,
        "Reason": "North forecourt/route side of P2, clear of pavilion geometry."},
    "HBCT_SM_Trigger_Afternoon": {
        "XY": (-2474.128, 3443.168), "Yaw": 121.209,
        "Reason": "25 cm west shift clears P6 while retaining the full 280 cm box and both smaller trigger centers."},
    "HBCT_SM_Trigger_RecordAfternoon": {
        "XY": (-2449.128, 3443.168), "Yaw": 121.209,
        "Reason": "Shares the gated afternoon-drink interaction area without solid overlap."},
    "HBCT_SM_Trigger_TargetSweetDrink": {
        "XY": (-2449.128, 3443.168), "Yaw": 121.209,
        "Reason": "Shares the gated P6 target-behavior area without solid overlap."},
    # Q4 O08 moves Chai and the neighbor through paired start/mid/end stages.
    # These positions follow the already-approved Chai route at a 160 cm
    # parallel offset.  The neighbor's earlier SSPlanNeighbor post remains at H7.
    "HBCT_SS_Location_NeighborWalkStart": {
        "XY": (-5444.229, 19697.501), "Yaw": -39.52,
        "Reason": "O07 completion meeting point, parallel to ChaiWalkStart at 160 cm."},
    "HBCT_SS_Location_NeighborWalkMid": {
        "XY": (-2143.029, 16973.381), "Yaw": -39.52,
        "Reason": "O08 paired midpoint, parallel to ChaiWalkMid at 160 cm."},
    "HBCT_SS_Location_NeighborWalkEnd": {
        "XY": (843.771, 14508.701), "Yaw": -39.52,
        "Reason": "O08 paired endpoint, parallel to ChaiWalkEnd at 160 cm."},
    # Q4 O10 defines start then end with no return stage.  The pair uses the
    # distinct H6-to-P2 short branch and separate side-by-side anchors.
    "HBCT_SS_Location_ChaiBackupStart": {
        "XY": (-6416.181, 19688.043), "Yaw": -110.493,
        "Reason": "First O10 move stage on the short H6-to-P2 branch; 160 cm from Wife."},
    "HBCT_SS_Location_WifeBackupStart": {
        "XY": (-6566.055, 19744.057), "Yaw": -110.493,
        "Reason": "First O10 companion stage, side-by-side with Chai."},
    "HBCT_SS_Location_ChaiBackupEnd": {
        "XY": (-7203.838, 17580.570), "Yaw": -110.493,
        "Reason": "Second/final O10 move stage near the community-side endpoint."},
    "HBCT_SS_Location_WifeBackupEnd": {
        "XY": (-7353.712, 17636.584), "Yaw": -110.493,
        "Reason": "Final O10 companion stage, side-by-side with Chai at 160 cm."},
}


def _now():
    return datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")


def _norm(value):
    return re.sub(r"[^a-z0-9]+", "", str(value).lower())


def _label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def _class_name(actor):
    try:
        return actor.get_class().get_name()
    except Exception:
        return actor.__class__.__name__


def _folder(actor):
    try:
        return str(actor.get_folder_path())
    except Exception:
        return ""


def _path(actor):
    try:
        return actor.get_path_name()
    except Exception:
        return _label(actor)


def _is_valid_object(obj):
    if obj is None:
        return False
    try:
        return bool(unreal.SystemLibrary.is_valid(obj))
    except Exception:
        # EditorActorSubsystem returns live UObject wrappers; UE 5.7 does not
        # expose UObject.is_valid() as an instance method.
        return True


def _v3(v):
    return [round(float(v.x), 3), round(float(v.y), 3), round(float(v.z), 3)]


def _r3(r):
    return [round(float(r.pitch), 3), round(float(r.yaw), 3), round(float(r.roll), 3)]


def _rotator_from_report(values):
    """Convert report-format [Pitch, Yaw, Roll] to an Unreal Rotator."""
    if values is None or len(values) != 3:
        raise ValueError("Report rotation must contain exactly [Pitch, Yaw, Roll].")
    return unreal.Rotator(
        pitch=float(values[0]),
        yaw=float(values[1]),
        roll=float(values[2]),
    )


def _transform_json(actor):
    return {
        "Location": _v3(actor.get_actor_location()),
        "Rotation": _r3(actor.get_actor_rotation()),
        "Scale": _v3(actor.get_actor_scale3d()),
    }


def _transform_from_values(location, rotation, scale):
    return {
        "Location": [round(float(x), 3) for x in location],
        "Rotation": [round(float(x), 3) for x in rotation],
        "Scale": [round(float(x), 3) for x in scale],
    }


def _distance(a, b):
    return math.sqrt((a.x-b.x)**2 + (a.y-b.y)**2 + (a.z-b.z)**2)


def _xy_distance(a, b):
    return math.sqrt((a.x-b.x)**2 + (a.y-b.y)**2)


def _angle_delta(a, b):
    return abs((float(a) - float(b) + 180.0) % 360.0 - 180.0)


def _safe_property(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def _tag_text(value):
    if value is None:
        return ""
    try:
        return str(value.get_editor_property("tag_name"))
    except Exception:
        return str(value)


def _actor_text(actor):
    pieces = [_label(actor), _class_name(actor), _folder(actor)]
    for prop in ("location_tag", "location_id", "target_tag", "target_id",
                 "required_active_quest_id", "required_active_objective_id",
                 "world_action_receiver_tag", "world_action_tag"):
        pieces.append(_tag_text(_safe_property(actor, prop, "")))
    return " ".join(pieces).lower()


def _static_mesh_path(actor):
    components = []
    direct = _safe_property(actor, "static_mesh_component")
    if direct:
        components.append(direct)
    try:
        components.extend(actor.get_components_by_class(unreal.StaticMeshComponent))
    except Exception:
        pass
    seen = set()
    for component in components:
        if not component or component in seen:
            continue
        seen.add(component)
        mesh = _safe_property(component, "static_mesh")
        if mesh:
            path = mesh.get_path_name()
            return path.split(".")[0]
    return ""


def _mesh_local_bounds(actor):
    mesh_path = _static_mesh_path(actor)
    result = {"Asset": mesh_path, "Origin": None, "Extent": None}
    if not mesh_path:
        return result
    mesh = unreal.load_asset(mesh_path)
    if not mesh:
        return result
    try:
        box = mesh.get_bounding_box()
        minimum = box.min
        maximum = box.max
        result["Origin"] = [round((minimum.x+maximum.x)*0.5, 3),
                            round((minimum.y+maximum.y)*0.5, 3),
                            round((minimum.z+maximum.z)*0.5, 3)]
        result["Extent"] = [round((maximum.x-minimum.x)*0.5, 3),
                            round((maximum.y-minimum.y)*0.5, 3),
                            round((maximum.z-minimum.z)*0.5, 3)]
    except Exception as exc:
        result["Error"] = str(exc)
    return result


def _bounds(actor):
    try:
        origin, extent = actor.get_actor_bounds(False, True)
    except TypeError:
        origin, extent = actor.get_actor_bounds(False)
    return origin, extent


def _world_map_path(world):
    outer = world.get_outer()
    package = outer.get_path_name() if outer else world.get_path_name()
    return package.split(".")[0]


def _assert_target_world():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        raise RuntimeError("No editor world is loaded. Open Lvl_Village_Main first.")
    path = _world_map_path(world)
    if path == PROTECTED_MAP or "Lvl_Village_Blockout" in path:
        raise RuntimeError("Protected level is loaded; refusing all work: {}".format(path))
    if path != TARGET_MAP and not path.endswith("/Lvl_Village_Main"):
        raise RuntimeError("Lvl_Village_Main must already be loaded; current world is {}".format(path))
    return world, path


def _all_actors():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return [a for a in subsystem.get_all_level_actors() if _is_valid_object(a)]


def _protected_file_state():
    path = os.path.join(unreal.Paths.project_content_dir(), "VHV_Stuff", "Maps", "Lvl_Village_Blockout.umap")
    if not os.path.isfile(path):
        return {"Path": path, "Exists": False}
    stat = os.stat(path)
    return {"Path": path, "Exists": True, "Size": stat.st_size, "MTimeNs": getattr(stat, "st_mtime_ns", int(stat.st_mtime*1e9))}


def _landmark_candidate_rows(key, spec, actors):
    exact_labels = {_norm(x): i for i, x in enumerate(spec.get("labels", ())) }
    mesh_paths = {x.lower() for x in spec.get("meshes", ())}
    class_names = {_norm(x) for x in spec.get("classes", ())}
    rows = []
    for actor in actors:
        label = _label(actor)
        mesh = _static_mesh_path(actor)
        cls = _class_name(actor)
        evidence = []
        score = 0
        if _norm(label) in exact_labels:
            score += 1000 - exact_labels[_norm(label)] * 20
            evidence.append("ExactLabel")
        if mesh.lower() in mesh_paths:
            score += 400
            evidence.append("StaticMesh")
        if _norm(cls) in class_names:
            score += 350
            evidence.append("ActorClass")
        if key in ("H1_MALI", "H6_CHAI", "H3_SAENG", "H7_NEIGHBOR"):
            story = " ".join(spec.get("story_tokens", ()))
            combined = (label + " " + _folder(actor)).lower()
            if story and any(token in combined for token in spec.get("story_tokens", ())):
                score += 180
                evidence.append("StoryLabelOrFolder")
        if score:
            rows.append(dict(Actor=actor, Score=score, Evidence=evidence,
                             Label=label, Class=cls, Mesh=mesh, Folder=_folder(actor)))
    rows.sort(key=lambda row: (-row["Score"], row["Label"].lower(), _path(row["Actor"])))
    return rows


def _near_story_evidence(candidate, actors, tokens):
    if not tokens:
        return []
    location = candidate.get_actor_location()
    evidence = []
    for actor in actors:
        if actor == candidate:
            continue
        text = _actor_text(actor)
        if any(token.lower() in text for token in tokens):
            distance = _xy_distance(location, actor.get_actor_location())
            if distance <= 1800.0:
                evidence.append({"ActorLabel": _label(actor), "DistanceCm": round(distance, 1)})
    evidence.sort(key=lambda item: item["DistanceCm"])
    return evidence[:8]


def _resolve_landmarks(actors):
    resolved = collections.OrderedDict()
    unresolved = []
    claimed = set()
    for key, spec in LANDMARK_SPECS.items():
        rows = _landmark_candidate_rows(key, spec, actors)
        usable = [row for row in rows if _path(row["Actor"]) not in claimed]
        status = "Resolved"
        chosen = usable[0] if usable else None
        reason = ""
        if not chosen:
            status = "Missing"
            reason = "No actor matched exact labels, mesh asset path, or actor class."
        elif len(usable) > 1 and usable[0]["Score"] == usable[1]["Score"]:
            status = "Ambiguous"
            reason = "Top candidates have equal identity evidence."
            chosen = None
        elif key in ("H3_SAENG", "H1_MALI", "H6_CHAI", "H7_NEIGHBOR"):
            story_evidence = _near_story_evidence(chosen["Actor"], actors, spec.get("story_tokens", ()))
            chosen["StoryEvidence"] = story_evidence
            has_semantic_label = any(token.lower() in _label(chosen["Actor"]).lower()
                                     for token in spec.get("story_tokens", ()))
            if key in ("H1_MALI", "H6_CHAI") and not has_semantic_label and not story_evidence:
                status = "Ambiguous"
                reason = "Generic house instance lacks nearby story-association evidence."
                chosen = None
        record = {
            "Status": status,
            "Critical": bool(spec.get("critical")),
            "Reason": reason,
            "Candidates": [{k: v for k, v in row.items() if k != "Actor"}
                           for row in rows[:12]],
        }
        if chosen:
            actor = chosen["Actor"]
            claimed.add(_path(actor))
            origin, extent = _bounds(actor)
            record.update({
                "ActorLabel": _label(actor), "ActorPath": _path(actor),
                "ActorClass": _class_name(actor), "Folder": _folder(actor),
                "StaticMesh": _static_mesh_path(actor),
                "Transform": _transform_json(actor),
                "WorldBounds": {"Origin": _v3(origin), "Extent": _v3(extent)},
                "MeshLocalBounds": _mesh_local_bounds(actor),
                "Evidence": chosen["Evidence"],
                "StoryEvidence": chosen.get("StoryEvidence", []),
            })
            if key in SCREENSHOT_REGISTRY:
                expected_location, expected_yaw = SCREENSHOT_REGISTRY[key]
                current = actor.get_actor_location()
                record["ScreenshotDifference"] = {
                    "DeltaLocationCm": [round(current.x-expected_location[0], 3),
                                        round(current.y-expected_location[1], 3),
                                        round(current.z-expected_location[2], 3)],
                    "DistanceCm": round(math.sqrt(sum((a-b)**2 for a, b in zip(_v3(current), expected_location))), 3),
                    "DeltaYawDeg": round(_angle_delta(actor.get_actor_rotation().yaw, expected_yaw), 3),
                    "CurrentLevelUsed": True,
                }
        else:
            unresolved.append({"Type": "Landmark", "Landmark": key, "Critical": bool(spec.get("critical")),
                               "Status": status, "Reason": reason,
                               "Candidates": record["Candidates"]})
        resolved[key] = record

    # All additional shelters are inventoried, but the exact Instructor shelter remains distinct.
    shelters = []
    for actor in actors:
        text = (_label(actor) + " " + _static_mesh_path(actor) + " " + _class_name(actor)).lower()
        if "shelter" in text:
            origin, extent = _bounds(actor)
            shelters.append({"ActorLabel": _label(actor), "ActorPath": _path(actor),
                             "Transform": _transform_json(actor),
                             "WorldBounds": {"Origin": _v3(origin), "Extent": _v3(extent)},
                             "StaticMesh": _static_mesh_path(actor)})
    resolved["ADDITIONAL_SHELTERS"] = {"Status": "Inventory", "Actors": shelters}
    return resolved, unresolved


def _resolved_actor(landmarks, actors_by_path, key):
    path = landmarks.get(key, {}).get("ActorPath")
    return actors_by_path.get(path)


def _quest_id(actor):
    text = _actor_text(actor)
    explicit = str(_safe_property(actor, "required_active_quest_id", ""))
    if explicit in QUEST_IDS:
        return explicit
    rules = (
        ("Q_HBCT_06_POSITIVE_REINFORCEMENT", ("hbct_pr_", "positivereinforcement", "positive reinforcement")),
        ("Q_HBCT_05_SELF_MONITORING", ("hbct_sm_", "selfmonitoring", "self monitoring")),
        ("Q_HBCT_04_SOCIAL_SUPPORT", ("hbct_ss_", "socialsupport", "social support")),
        ("Q_HBCT_03_ROLE_MODEL", ("rolemodel", "role model", "hbct_location_exercisepark")),
        ("Q_HBCT_02_GOAL_SETTING", ("goalsetting", "goal setting", "hbct_location_healthpost")),
        ("Q_HBCT_01_MOTIVATION", ("motivation", "prasert", "saengappreciation", "hbct_location_malihouse")),
        ("Q_HBCT_00_INTRO", ("hbct_intro", "hbct_market", "villageentrance", "openingcamerahook",
                             "hbct_location_market")),
    )
    for quest, tokens in rules:
        if any(token in text for token in tokens):
            return quest
    return ""


def _participant_tag(actor):
    for method_name in ("get_quest_participant_component",):
        try:
            component = getattr(actor, method_name)()
            return _tag_text(_safe_property(component, "participant_tag", ""))
        except Exception:
            pass
    try:
        for component in actor.get_components_by_class(unreal.ActorComponent):
            value = _tag_text(_safe_property(component, "participant_tag", ""))
            if value:
                return value
    except Exception:
        pass
    return ""


def _actor_kind(actor):
    cls = _class_name(actor).lower()
    label = _label(actor).lower()
    if "questlocationvolume" in cls:
        objective_name = str(_safe_property(actor, "required_active_objective_id", ""))
        objective_gated = bool(objective_name and objective_name.lower() != "none")
        is_trigger = bool(_safe_property(actor, "trigger_world_action", False)
                          or _safe_property(actor, "activate_current_objective", False)
                          or objective_gated
                          or "trigger" in label)
        if bool(_safe_property(actor, "require_player_interaction", False)):
            return "InteractionPoint"
        if is_trigger:
            return "ObjectiveTrigger"
        route_text = _actor_text(actor)
        if "walk" in route_text and any(token in route_text for token in ("start", "mid", "end", "exit")):
            return "MovementAnchor"
        return "SemanticLocation"
    if "behaviortarget" in cls or "targetpoint" in cls or "anchor" in label or "waypoint" in label:
        return "MovementAnchor" if not ("camera" in label) else "CameraTarget"
    if "ambientconversation" in cls or "scene" in label or "director" in cls or "controller" in cls:
        return "SceneActor"
    if _participant_tag(actor) or "npc" in cls or "character" in cls:
        return "NPC"
    if "camera" in cls or "camera" in label:
        return "CameraTarget"
    if label.startswith("hbct_"):
        return "InteractionPoint" if ("board" in label or "tracker" in label) else "SceneActor"
    return ""


def _is_relevant(actor):
    label = _label(actor).lower()
    folder = _folder(actor).lower()
    text = _actor_text(actor)
    if label.startswith("hbct_") or "hbct" in folder:
        return True
    if any(folder.startswith(prefix) for prefix in ("01_", "02_", "03_", "04_", "05_", "06_", "07_")):
        return bool(_actor_kind(actor))
    if any(q.lower() in text for q in QUEST_IDS):
        return True
    participant = _participant_tag(actor).lower()
    return any(name in participant for name in ("instructor", "unclechai", "auntmali", "auntsaeng"))


def _objective_id(actor):
    value = str(_safe_property(actor, "required_active_objective_id", ""))
    return value if value and value != "None" else ""


def _inventory(actors):
    inventory = []
    for actor in actors:
        if not _is_relevant(actor):
            continue
        kind = _actor_kind(actor)
        if not kind:
            continue
        inventory.append({
            "Actor": actor, "ActorPath": _path(actor), "ActorLabel": _label(actor),
            "ActorClass": _class_name(actor), "ExistingFolder": _folder(actor),
            "QuestId": _quest_id(actor), "ObjectiveId": _objective_id(actor),
            "Kind": kind, "ParticipantTag": _participant_tag(actor),
            "Transform": _transform_json(actor), "SemanticText": _actor_text(actor),
        })
    inventory.sort(key=lambda row: (row["QuestId"], row["Kind"], row["ActorLabel"].lower()))
    return inventory


def _target_rule(row):
    """Return landmark, station, purpose, and route role from existing identity."""
    q = row["QuestId"]
    text = row["SemanticText"]
    label = row["ActorLabel"].lower()
    participant = row["ParticipantTag"].lower()
    route = ""

    if q == "Q_HBCT_00_INTRO":
        if any(x in text for x in ("junk", "junkfood")):
            return "P5_KIOSK", "market_p5_junk", "Junk-food observation at kiosk", route
        if any(x in text for x in ("smoker", "shoprow")):
            return "P7_SHOP_ROW", "market_p7_smoker", "Smoker observation at shop row", route
        if any(x in text for x in ("buyer", "vendor")):
            return "P6_DRINK_STALL", "market_p6_vendor", "Vendor/buyer stall scene", route
        if any(x in text for x in ("sweettea", "sugardrink", "sweetdrink")):
            return "P6_DRINK_STALL", "market_p6_drink", "Sweet-drink observation", route
        if any(x in text for x in ("rider", "motorbike", "market")):
            return "P4_MAIN_MARKET", "market_roadside", "Market circulation/roadside observation", route
        if any(x in text for x in ("debrief", "lesson")):
            return "P4_MAIN_MARKET", "market_quiet", "Quiet market-edge teaching", route
        return "VILLAGE_ENTRANCE", "entrance_intro", "Opening entrance staging", route

    if q == "Q_HBCT_01_MOTIVATION":
        if "prasert" in text:
            return "H2_PRASERT", "prasert_yard", "Prasert yard/porch observation", route
        if "saeng" in text:
            return "H3_SAENG", "saeng_yard", "Saeng appreciation scene", route
        if "mali" in text:
            return "H1_MALI", "mali_yard", "Mali recurring yard scene", route
        if any(x in text for x in ("badgood", "directive")):
            station = "motivation_bad" if ("directive" in text or "bad" in text) else "motivation_good"
            return "INSTRUCTOR_SHELTER", station, "Motivation demonstration pair", route
        if any(x in text for x in ("motivational", "reflection", "learnmotivation")):
            return "INSTRUCTOR_SHELTER", "motivation_reflection", "Motivation teaching/reflection", route
        if "debrief" in text or "instructor" in participant:
            return "VILLAGE_ENTRANCE", "entrance_debrief", "Motivation final entrance debrief", route

    if q == "Q_HBCT_02_GOAL_SETTING":
        if "mali" in text:
            return "H1_MALI", "mali_yard", "Mali goal-plan revisit", route
        if "road" in text:
            return "P4_MAIN_MARKET", "market_roadside", "Safe roadside practice near market route", route
        if "final" in text:
            return "P1_HEALTH_POST", "clinic_evaluate", "Clinic/community-side final teaching", route
        station = "clinic_lesson"
        for token, value in (("initial", "clinic_initial"), ("clear", "clinic_clear"),
                             ("nogoal", "clinic_nogoal"), ("no goal", "clinic_nogoal"),
                             ("selfset", "clinic_self"), ("joint", "clinic_joint"),
                             ("evaluat", "clinic_evaluate"), ("goodgoal", "clinic_evaluate")):
            if token in text:
                station = value
                break
        return "P1_HEALTH_POST", station, "Goal-setting clinic forecourt station", route

    if q == "Q_HBCT_03_ROLE_MODEL":
        if "saenghouseexit" in text:
            return "H3_SAENG", "saeng_yard", "Saeng residence route-exit anchor", route
        if "walk" in text:
            route = "Q3_MAIN"
            return "P3_EXERCISE_PARK", "park_demo", "Role-model walking route anchor", route
        station = "park_candidates"
        for token, value in (("exercise", "park_exercise"), ("similar", "park_similar"),
                             ("experience", "park_similar"), ("family", "park_family"),
                             ("symbolic", "park_symbolic"), ("reflection", "park_reflection"),
                             ("before", "park_timeline_before"), ("findsmodel", "park_timeline_model"),
                             ("firstattempt", "park_timeline_attempt"), ("repetition", "park_timeline_repeat"),
                             ("newroutine", "park_timeline_routine"), ("demo", "park_demo"),
                             ("debrief", "park_debrief")):
            if token in text:
                station = value
                break
        return "P3_EXERCISE_PARK", station, "Role-model park station", route

    if q == "Q_HBCT_04_SOCIAL_SUPPORT":
        if any(x in text for x in ("guidance", "clinicnurse", "chainurse")):
            return "P1_HEALTH_POST", "clinic_feedback", "Health guidance at clinic", route
        if "meal" in text:
            return "H8_FAMILY", "family_meal", "Healthy meal home-yard observation", route
        if "neighbor" in text:
            return "H7_NEIGHBOR", "neighbor_home", "Neighbor near H6 route exit", route
        if "walk" in text:
            route = "Q4_BACKUP" if "backup" in text else "Q4_MAIN"
            return "H6_CHAI", "chai_home", "Social-support walking route anchor", route
        if any(x in text for x in ("chai", "wife", "daughter", "disruption", "monthlater", "one month", "backup")):
            return "H6_CHAI", "chai_home", "Chai family support scene", route
        station = "pavilion_group"
        for token, value in (("emotional", "pavilion_emotional"), ("informational", "pavilion_information"),
                             ("information", "pavilion_information"), ("instrumental", "pavilion_instrumental"),
                             ("appraisal", "pavilion_appraisal"), ("network", "pavilion_board"),
                             ("board", "pavilion_board"), ("reflection", "pavilion_reflection"),
                             ("encourag", "pavilion_reflection"), ("final", "pavilion_group")):
            if token in text:
                station = value
                break
        return "P2_PAVILION", station, "Community support learning station", route

    if q == "Q_HBCT_05_SELF_MONITORING":
        if any(x in text for x in ("afternoon", "sweetdrink")):
            return "P6_DRINK_STALL", "market_p6_drink", "Afternoon drink observation/recording", route
        if any(x in text for x in ("walk", "route")):
            route = "Q5_ADJUSTED" if "adjust" in text else "Q5_MAIN"
            return "H6_CHAI", "chai_home", "Self-monitoring walking route anchor", route
        if any(x in text for x in ("checkin", "chaihome", "tracker", "breakfast", "firstday", "oneweek",
                                   "reminder", "selfmonitorpayoff")):
            return "H6_CHAI", "chai_home", "Chai home self-monitoring scene", route
        station = "pavilion_process"
        for token, value in (("target", "pavilion_target"), ("process", "pavilion_process"),
                             ("method", "pavilion_method"), ("pattern", "pavilion_pattern"),
                             ("graph", "pavilion_graph"), ("final", "pavilion_reflection")):
            if token in text:
                station = value
                break
        return "P2_PAVILION", station, "Community self-monitoring teaching station", route

    if q == "Q_HBCT_06_POSITIVE_REINFORCEMENT":
        if any(x in text for x in ("healthpost", "healthnurse", "feedbackresult")):
            return "P1_HEALTH_POST", "clinic_feedback", "Health feedback scene", route
        if "finaldebrief" in text:
            return "VILLAGE_ENTRANCE", "entrance_debrief", "Q6 final entrance debrief", route
        if "challengewalk" in text or "walkingchallenge" in text:
            route = "Q6_CHALLENGE"
            return "H6_CHAI", "chai_home", "Reinforcement walking challenge route", route
        if any(x in text for x in ("chaihome", "checkin", "exercise", "selfreward")):
            return "H6_CHAI", "chai_home", "Chai reinforcement review", route
        if any(x in text for x in ("healthpark", "successwalker", "successchild", "waterchild", "waterparent",
                                   "notice", "instructorintro", "instructorlesson")):
            return "P3_EXERCISE_PARK", "park_exercise", "Exercise-park success vignette", route
        station = "fair_preferences"
        for token, value in (("social", "fair_social"), ("material", "fair_material"),
                             ("activity", "fair_activity"), ("token", "fair_token"),
                             ("feedback", "fair_feedback"), ("timing", "fair_timing"),
                             ("preference", "fair_preferences"), ("healthfair", "pavilion_group")):
            if token in text:
                station = value
                break
        return "P2_PAVILION", station, "Community health-fair reinforcement station", route

    # Shared persistent NPCs have a base post; runtime semantic stages remain intact.
    if "instructor" in participant:
        return "VILLAGE_ENTRANCE", "entrance_intro", "Persistent Instructor base post", route
    if "unclechai" in participant:
        return "H6_CHAI", "chai_home", "Persistent Uncle Chai home post", route
    if "auntmali" in participant:
        return "H1_MALI", "mali_yard", "Persistent Mali home post", route
    if "auntsaeng" in participant:
        return "H3_SAENG", "saeng_yard", "Persistent Saeng home post", route
    return None, None, None, None


def _yaw_vectors(yaw):
    rad = math.radians(float(yaw))
    forward = unreal.Vector(math.cos(rad), math.sin(rad), 0.0)
    right = unreal.Vector(-math.sin(rad), math.cos(rad), 0.0)
    return forward, right


def _normalized_xy(vector):
    length = math.sqrt(vector.x*vector.x + vector.y*vector.y)
    if length < 1.0:
        return unreal.Vector(1.0, 0.0, 0.0)
    return unreal.Vector(vector.x/length, vector.y/length, 0.0)


def _approach_reference(key, landmarks, actors_by_path):
    refs = {
        "VILLAGE_ENTRANCE": "PLAYER_START", "P4_MAIN_MARKET": "PLAYER_START",
        "P5_KIOSK": "P4_MAIN_MARKET", "P6_DRINK_STALL": "P4_MAIN_MARKET", "P7_SHOP_ROW": "P4_MAIN_MARKET",
        "H2_PRASERT": "P4_MAIN_MARKET", "H3_SAENG": "P4_MAIN_MARKET", "H1_MALI": "P1_HEALTH_POST",
        "INSTRUCTOR_SHELTER": "H2_PRASERT", "P1_HEALTH_POST": "HEALTH_GATE",
        "P2_PAVILION": "H6_CHAI", "P3_EXERCISE_PARK": "P2_PAVILION",
        "H6_CHAI": "P2_PAVILION", "H7_NEIGHBOR": "H6_CHAI", "H8_FAMILY": "P2_PAVILION",
    }
    ref = _resolved_actor(landmarks, actors_by_path, refs.get(key, "PLAYER_START"))
    return ref.get_actor_location() if ref else None


def _station_basis(landmark, key, landmarks, actors_by_path):
    origin, extent = _bounds(landmark)
    approach = _approach_reference(key, landmarks, actors_by_path)
    if approach:
        outward = _normalized_xy(approach - origin)
        facing_basis = "Closest bounds side to approach reference; not asserted as a doorway"
    else:
        forward, unused = _yaw_vectors(landmark.get_actor_rotation().yaw)
        outward = forward * -1.0
        facing_basis = "Actor yaw fallback; doorway not detected"
    tangent = unreal.Vector(-outward.y, outward.x, 0.0)
    radial_extent = abs(outward.x)*extent.x + abs(outward.y)*extent.y
    base = origin + outward * (radial_extent + 180.0)
    return base, outward, tangent, extent, facing_basis


def _station_point(landmark, key, station, landmarks, actors_by_path):
    base, outward, tangent, extent, facing_basis = _station_basis(landmark, key, landmarks, actors_by_path)
    index = STATION_INDEX.get(station, 0)
    # Five stations per row.  Large public sites use wider spacing.
    public = key in ("P1_HEALTH_POST", "P2_PAVILION", "P3_EXERCISE_PARK")
    spacing = 650.0 if public else 430.0
    row_spacing = 520.0 if public else 380.0
    column = index % 5
    row = index // 5
    point = base + tangent * ((column - 2.0) * spacing) + outward * (row * row_spacing)
    return point, outward, tangent, facing_basis


def _decode_hit_result(hit):
    """Decode the UE 5.7 HitResult UStruct using the project's proven pattern."""
    if hit is None:
        return None
    # Some generated wrappers expose a (return_value, out_hit) pair.  Unwrap
    # that without mistaking the HitResult's own 18-field tuple for the pair.
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
    (blocking, initial_overlap, time_value, distance, location, impact_point,
     normal, impact_normal, physical_material, actor, component,
     hit_bone_name, bone_name, hit_item, element_index, face_index,
     trace_start, trace_end) = values[:18]
    return {
        "BlockingHit": bool(blocking),
        "InitialOverlap": bool(initial_overlap),
        "Time": float(time_value),
        "Distance": float(distance),
        "LocationObject": location,
        "ImpactPointObject": impact_point,
        "NormalObject": normal,
        "ImpactNormalObject": impact_normal,
        "ActorObject": actor,
        "ComponentObject": component,
        "TraceStartObject": trace_start,
        "TraceEndObject": trace_end,
    }


def _is_landscape_surface(actor, component):
    actor_class = _class_name(actor).lower() if actor else ""
    component_class = _class_name(component).lower() if component else ""
    return (actor_class in ("landscape", "landscapeproxy", "landscapestreamingproxy")
            or "landscapeheightfieldcollisioncomponent" in component_class
            or "landscapemeshcollisioncomponent" in component_class)


def _hit_json(hit):
    decoded = _decode_hit_result(hit)
    if not decoded:
        return {"Decoded": False, "PythonType": type(hit).__name__}
    actor = decoded["ActorObject"]
    component = decoded["ComponentObject"]
    return {
        "Decoded": True,
        "BlockingHit": decoded["BlockingHit"],
        "InitialOverlap": decoded["InitialOverlap"],
        "Distance": round(decoded["Distance"], 3),
        "ActorName": _label(actor) if actor else "",
        "ActorClass": _class_name(actor) if actor else "",
        "ComponentName": component.get_name() if component else "",
        "ComponentClass": _class_name(component) if component else "",
        "ImpactLocation": _v3(decoded["ImpactPointObject"]),
        "ImpactNormal": _v3(decoded["ImpactNormalObject"]),
        "ValidLandscapeSurface": _is_landscape_surface(actor, component),
    }


def _component_collision_json(component):
    row = {"Name": component.get_name(), "Class": _class_name(component)}
    for label, method in (("CollisionEnabled", "get_collision_enabled"),
                          ("CollisionProfile", "get_collision_profile_name"),
                          ("GenerateOverlapEvents", "get_generate_overlap_events")):
        try:
            row[label] = str(getattr(component, method)())
        except Exception as exc:
            row[label] = "Unavailable: {}".format(exc)
    responses = {}
    try:
        enum_type = unreal.CollisionChannel
        for channel_name in ("ECC_VISIBILITY", "ECC_CAMERA", "ECC_WORLD_STATIC", "ECC_PAWN"):
            channel = getattr(enum_type, channel_name, None)
            if channel is not None:
                responses[channel_name] = str(component.get_collision_response_to_channel(channel))
    except Exception as exc:
        responses["Error"] = str(exc)
    row["Responses"] = responses
    return row


def _landscape_diagnostic_inventory(actors):
    rows = []
    for actor in actors:
        actor_class = _class_name(actor).lower()
        if "landscape" not in actor_class:
            continue
        components = []
        try:
            components = list(actor.get_components_by_class(unreal.ActorComponent))
        except Exception:
            pass
        collision_components = [component for component in components
                                if "collision" in _class_name(component).lower()]
        rows.append({
            "ActorLabel": _label(actor), "ActorClass": _class_name(actor),
            "ActorPath": _path(actor), "Transform": _transform_json(actor),
            "CollisionComponents": [_component_collision_json(component)
                                    for component in collision_components],
        })
    return rows


def _diagnostic_trace(world, x, y, channel, channel_name):
    start = unreal.Vector(float(x), float(y), 20000.0)
    end = unreal.Vector(float(x), float(y), -10000.0)
    try:
        raw_single = unreal.SystemLibrary.line_trace_single(
            world, start, end, channel, False, [], unreal.DrawDebugTrace.NONE,
            True, unreal.LinearColor(), unreal.LinearColor(), 0.0)
        raw_single_type = type(raw_single).__name__
        raw_single_decoded = _hit_json(raw_single)
    except Exception as exc:
        raw_single_type = "Exception"
        raw_single_decoded = {"Decoded": False, "Error": str(exc)}
    try:
        hits = unreal.SystemLibrary.line_trace_multi(
            world, start, end, channel, False, [], unreal.DrawDebugTrace.NONE, True) or []
    except Exception as exc:
        return {"XY": [float(x), float(y)], "Start": _v3(start), "End": _v3(end),
                "TraceChannel": channel_name, "Error": str(exc), "HitCount": 0,
                "Hits": [], "ValidLandscapeHit": False}
    hit_rows = [_hit_json(hit) for hit in hits]
    landscapes = [row for row in hit_rows
                  if row.get("Decoded") and row.get("BlockingHit")
                  and row.get("ValidLandscapeSurface")]
    return {
        "XY": [float(x), float(y)], "Start": _v3(start), "End": _v3(end),
        "TraceChannel": channel_name, "TraceComplex": False,
        "IgnoredActors": [], "HitCount": len(hits), "Hits": hit_rows,
        "SingleReturnPythonType": raw_single_type,
        "SingleReturnDecoded": raw_single_decoded,
        "ValidLandscapeHit": bool(landscapes),
        "LandscapeImpact": landscapes[0].get("ImpactLocation") if landscapes else None,
    }


def diagnose_ground_traces():
    """Read-only five-site diagnosis; never builds the 385-placement plan."""
    world, map_path = _assert_target_world()
    actors = _all_actors()
    sites = collections.OrderedDict([
        ("VillageEntrance", (0.0, 0.0)),
        ("Market", (-2350.0, 2610.0)),
        ("HealthPost", (-4600.0, 9695.0)),
        ("CommunityPavilion", (-7360.0, 16950.0)),
        ("ExercisePark", (1552.0, 13813.0)),
    ])
    offsets = ((0.0, 0.0), (600.0, 0.0), (-600.0, 0.0),
               (0.0, 600.0), (0.0, -600.0), (1000.0, 0.0),
               (-1000.0, 0.0), (0.0, 1000.0), (0.0, -1000.0))
    site_rows = []
    for name, xy in sites.items():
        attempts = []
        selected = None
        for dx, dy in offsets:
            by_channel = [
                _diagnostic_trace(world, xy[0]+dx, xy[1]+dy,
                                  unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, "Visibility / TraceTypeQuery1"),
                _diagnostic_trace(world, xy[0]+dx, xy[1]+dy,
                                  unreal.TraceTypeQuery.TRACE_TYPE_QUERY2, "Camera / TraceTypeQuery2"),
            ]
            attempt = {"XY": [xy[0]+dx, xy[1]+dy], "Channels": by_channel}
            attempts.append(attempt)
            visibility = by_channel[0]
            if visibility.get("ValidLandscapeHit"):
                selected = {"XY": attempt["XY"], "Channel": visibility["TraceChannel"],
                            "Impact": visibility["LandscapeImpact"]}
                shared_point, shared_result = _trace_ground(
                    world, unreal.Vector(attempt["XY"][0], attempt["XY"][1], 0.0), [])
                selected["SharedRoutinePassed"] = bool(
                    shared_point and shared_result.get("Supported")
                    and shared_result.get("ValidLandscapeSurface"))
                selected["SharedRoutineResult"] = shared_result
                break
        site_rows.append({"Site": name, "ReferenceXY": list(xy),
                          "SelectedOpenGround": selected, "Attempts": attempts})
    result = {
        "SchemaVersion": 1, "GeneratedAtLocal": _now(), "Mode": DIAGNOSTIC_ONLY,
        "TargetMap": map_path,
        "World": {"Path": world.get_path_name(), "Class": _class_name(world),
                  "Outer": world.get_outer().get_path_name() if world.get_outer() else "",
                  "WorldType": str(_safe_property(world, "world_type", "Unavailable"))},
        "LandscapeInventory": _landscape_diagnostic_inventory(actors),
        "Sites": site_rows,
        "ProtectedMapFileBefore": _protected_file_state(),
        "ProtectedMapLoadedOrSaved": False,
        "PlacementPlanGenerated": False,
        "ActorsModified": False,
    }
    result["FiveSiteSummary"] = {
        "SitesPassed": sum(1 for row in site_rows
                           if (row.get("SelectedOpenGround") or {}).get("SharedRoutinePassed")),
        "SitesTested": len(site_rows),
        "AllSharedRoutineTracesPassed": all(
            (row.get("SelectedOpenGround") or {}).get("SharedRoutinePassed") for row in site_rows),
    }
    result["ProtectedMapFileAfter"] = _protected_file_state()
    if not os.path.isdir(REPORT_DIRECTORY):
        os.makedirs(REPORT_DIRECTORY)
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    path = os.path.join(REPORT_DIRECTORY, "ground_trace_diagnostic_{}.json".format(stamp))
    with open(path, "w") as stream:
        json.dump(result, stream, indent=2)
    unreal.log("{} five-site ground diagnostic: {}".format(LOG, path))
    return result


def _trace_ground(world, desired, ignore):
    # Use an absolute vertical range: building pivots and proposed actor Z values
    # are not terrain elevations, and the northern park is substantially raised.
    start = unreal.Vector(desired.x, desired.y, 20000.0)
    end = unreal.Vector(desired.x, desired.y, -10000.0)
    try:
        hits = unreal.SystemLibrary.line_trace_multi(
            world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
            unreal.DrawDebugTrace.NONE, True) or []
    except Exception as exc:
        return None, {"Supported": False, "Error": "Ground trace failed: {}".format(exc),
                      "TraceStart": _v3(start), "TraceEnd": _v3(end),
                      "TraceChannel": "Visibility / TraceTypeQuery1"}
    hit_rows = [_hit_json(hit) for hit in hits]
    landscape_hits = []
    for hit in hits:
        decoded = _decode_hit_result(hit)
        if (decoded and decoded["BlockingHit"]
                and _is_landscape_surface(decoded["ActorObject"], decoded["ComponentObject"])):
            landscape_hits.append(decoded)
    if not landscape_hits:
        obstruction = next((row for row in hit_rows
                            if row.get("Decoded") and row.get("BlockingHit")), None)
        error = "No blocking Landscape collision hit"
        if obstruction:
            error += "; first blocker is {} ({})".format(
                obstruction.get("ActorName") or "unknown",
                obstruction.get("ActorClass") or "unknown")
        return None, {"Supported": False, "Error": error,
                      "TraceStart": _v3(start), "TraceEnd": _v3(end),
                      "TraceChannel": "Visibility / TraceTypeQuery1",
                      "HitCount": len(hits), "Hits": hit_rows}
    decoded = landscape_hits[0]
    impact = decoded["ImpactPointObject"]
    normal = decoded["ImpactNormalObject"]
    hit_actor = decoded["ActorObject"]
    hit_component = decoded["ComponentObject"]
    slope = math.degrees(math.acos(max(-1.0, min(1.0, float(normal.z)))))
    result = {"Supported": True, "GroundPoint": _v3(impact), "Normal": _v3(normal),
              "SlopeDegrees": round(slope, 2), "HitActor": _label(hit_actor) if hit_actor else "",
              "HitActorClass": _class_name(hit_actor) if hit_actor else "",
              "HitComponent": hit_component.get_name() if hit_component else "",
              "HitComponentClass": _class_name(hit_component) if hit_component else "",
              "ValidLandscapeSurface": True,
              "TraceStart": _v3(start), "TraceEnd": _v3(end),
              "TraceChannel": "Visibility / TraceTypeQuery1",
              "HitCount": len(hits), "Hits": hit_rows}
    if slope > 28.0:
        result["Supported"] = False
        result["Error"] = "Ground slope exceeds 28 degrees"
    return impact, result


def _capsule_clear(world, point, ignore, radius=45.0, half_height=96.0):
    center = unreal.Vector(point.x, point.y, point.z + half_height + 3.0)
    end = unreal.Vector(center.x, center.y, center.z + 1.0)
    try:
        hit = unreal.SystemLibrary.capsule_trace_single(
            world, center, end, radius, half_height,
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
            unreal.DrawDebugTrace.NONE, False, unreal.LinearColor(), unreal.LinearColor(), 0.0)
    except Exception as exc:
        return {"Clear": False, "Error": "Capsule trace failed: {}".format(exc)}
    # UE 5.7 returns None (rather than a default HitResult) when a single
    # capsule sweep has no hit.  That is the expected clear-space result.
    if hit is None or (isinstance(hit, tuple) and len(hit) == 2
                       and isinstance(hit[0], bool) and not hit[0]):
        return {"Clear": True, "BlockingActor": "", "NoHit": True}
    decoded = _decode_hit_result(hit)
    if not decoded:
        return {"Clear": False, "Error": "Capsule HitResult could not be decoded",
                "PythonType": type(hit).__name__}
    blocking = decoded["BlockingHit"]
    hit_actor = decoded["ActorObject"]
    hit_component = decoded["ComponentObject"]
    return {"Clear": not blocking,
            "BlockingActor": _label(hit_actor) if hit_actor else "",
            "BlockingActorClass": _class_name(hit_actor) if hit_actor else "",
            "BlockingComponent": hit_component.get_name() if hit_component else "",
            "BlockingComponentClass": _class_name(hit_component) if hit_component else "",
            "InitialOverlap": decoded["InitialOverlap"],
            "ImpactLocation": _v3(decoded["ImpactPointObject"])}


def _box_clear(world, point, ignore, extent, yaw=0.0):
    """Check a live trigger box above its grounded pivot against solid Visibility blockers."""
    center = unreal.Vector(point.x, point.y, point.z + float(extent.z) + 3.0)
    end = unreal.Vector(center.x, center.y, center.z + 1.0)
    rotation = unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(yaw))
    try:
        hit = unreal.SystemLibrary.box_trace_single(
            world, center, end, extent, rotation,
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
            unreal.DrawDebugTrace.NONE, False, unreal.LinearColor(), unreal.LinearColor(), 0.0)
    except Exception as exc:
        return {"Clear": False, "Error": "Box trace failed: {}".format(exc)}
    if hit is None or (isinstance(hit, tuple) and len(hit) == 2
                       and isinstance(hit[0], bool) and not hit[0]):
        return {"Clear": True, "BlockingActor": "", "NoHit": True,
                "Shape": "BoxComponent", "Extent": _v3(extent)}
    decoded = _decode_hit_result(hit)
    if not decoded:
        return {"Clear": False, "Error": "Box HitResult could not be decoded",
                "PythonType": type(hit).__name__}
    hit_actor = decoded["ActorObject"]
    hit_component = decoded["ComponentObject"]
    return {"Clear": not decoded["BlockingHit"],
            "BlockingActor": _label(hit_actor) if hit_actor else "",
            "BlockingActorClass": _class_name(hit_actor) if hit_actor else "",
            "BlockingComponent": hit_component.get_name() if hit_component else "",
            "BlockingComponentClass": _class_name(hit_component) if hit_component else "",
            "InitialOverlap": decoded["InitialOverlap"],
            "ImpactLocation": _v3(decoded["ImpactPointObject"]),
            "Shape": "BoxComponent", "Extent": _v3(extent)}


def _actor_clearance_shape(actor):
    """Return the actor's live capsule, or a conservative bounds-derived volume."""
    try:
        capsules = list(actor.get_components_by_class(unreal.CapsuleComponent))
    except Exception:
        capsules = []
    for component in capsules:
        try:
            radius = float(component.get_scaled_capsule_radius())
            half_height = float(component.get_scaled_capsule_half_height())
            return {"Source": "CapsuleComponent", "Component": component.get_name(),
                    "Radius": round(radius, 3), "HalfHeight": round(half_height, 3)}
        except Exception:
            pass
    box = _safe_property(actor, "box_component")
    if box:
        try:
            extent = box.get_scaled_box_extent()
        except Exception:
            extent = _safe_property(box, "box_extent")
            if extent:
                scale = actor.get_actor_scale3d()
                extent = unreal.Vector(abs(extent.x*scale.x), abs(extent.y*scale.y),
                                       abs(extent.z*scale.z))
        if extent:
            # The capsule is a conservative broad-phase approximation of the
            # live query-only box. Intended trigger-trigger overlaps are ignored;
            # Visibility blockers still represent solid scenery.
            return {"Source": "BoxComponent", "Component": box.get_name(),
                    "Extent": _v3(extent),
                    "Radius": round(max(float(extent.x), float(extent.y)), 3),
                    "HalfHeight": round(float(extent.z), 3)}
    origin, extent = _bounds(actor)
    radius = max(25.0, min(400.0, max(float(extent.x), float(extent.y))))
    half_height = max(25.0, min(400.0, float(extent.z)))
    return {"Source": "ActorWorldBounds", "Origin": _v3(origin), "Extent": _v3(extent),
            "Radius": round(radius, 3), "HalfHeight": round(half_height, 3)}


def _candidate_surface(world, actor, x, y, shape):
    desired = unreal.Vector(float(x), float(y), 0.0)
    ground, terrain = _trace_ground(world, desired, [actor])
    result = {"XY": [round(float(x), 3), round(float(y), 3)],
              "Terrain": terrain, "Grounded": bool(ground)}
    if not ground:
        result.update({"Accepted": False, "RejectReason": terrain.get("Error", "No Landscape ground")})
        return result
    footprint = min(200.0, max(45.0, float(shape["Radius"])))
    samples = []
    stable = True
    for dx, dy in ((footprint, 0.0), (-footprint, 0.0),
                   (0.0, footprint), (0.0, -footprint)):
        edge, edge_terrain = _trace_ground(
            world, unreal.Vector(float(x)+dx, float(y)+dy, ground.z), [actor])
        samples.append({"XY": [round(float(x)+dx, 3), round(float(y)+dy, 3)],
                        "Grounded": bool(edge), "GroundZ": round(float(edge.z), 3) if edge else None,
                        "SlopeDegrees": edge_terrain.get("SlopeDegrees"),
                        "Error": edge_terrain.get("Error", "")})
        if not edge or abs(float(edge.z)-float(ground.z)) > 60.0:
            stable = False
    clearance = _capsule_clear(world, ground, [actor],
                               radius=float(shape["Radius"]),
                               half_height=float(shape["HalfHeight"]))
    accepted = stable and bool(clearance.get("Clear"))
    reason = ""
    if not stable:
        reason = "Footprint lacks stable Landscape support"
    elif not clearance.get("Clear"):
        reason = "Actor-sized clearance blocked by {} ({})".format(
            clearance.get("BlockingActor") or "unknown",
            clearance.get("BlockingActorClass") or "unknown")
    result.update({"GroundZ": round(float(ground.z), 3), "FootprintRadius": round(footprint, 3),
                   "FootprintSamples": samples, "StableSurface": stable,
                   "Clearance": clearance, "Accepted": accepted,
                   "RejectReason": reason})
    return result


def _latest_report_only_path():
    candidates = []
    if os.path.isdir(REPORT_DIRECTORY):
        for name in os.listdir(REPORT_DIRECTORY):
            if name.startswith("village_quest_migration_report_only_") and name.endswith(".json"):
                path = os.path.join(REPORT_DIRECTORY, name)
                candidates.append((os.path.getmtime(path), path))
    if not candidates:
        raise RuntimeError("No prior REPORT_ONLY JSON exists under {}".format(REPORT_DIRECTORY))
    return max(candidates)[1]


def diagnose_physical_blockers():
    """Search bounded alternatives for only the latest report's physical failures."""
    world, map_path = _assert_target_world()
    actors = _all_actors()
    actors_by_path = {_path(actor): actor for actor in actors}
    actors_by_label = collections.defaultdict(list)
    for actor in actors:
        actors_by_label[_label(actor)].append(actor)
    source_path = _latest_report_only_path()
    with open(source_path, "r") as stream:
        source = json.load(stream)
    failed = [row for row in source.get("ProposedChanges", [])
              if row.get("PhysicalValidationStatus") == "Failed"]
    if len(failed) != 9:
        raise RuntimeError("Expected exactly 9 physical failures in latest report; found {}".format(len(failed)))
    directions = ((1.0, 0.0, "E"), (-1.0, 0.0, "W"), (0.0, 1.0, "N"),
                  (0.0, -1.0, "S"), (0.70710678, 0.70710678, "NE"),
                  (-0.70710678, 0.70710678, "NW"),
                  (0.70710678, -0.70710678, "SE"),
                  (-0.70710678, -0.70710678, "SW"))
    distances = (150.0, 250.0, 400.0, 600.0)
    rows = []
    blocker_labels = set()
    for previous in failed:
        actor = actors_by_path.get(previous.get("ActorPath"))
        if not actor:
            raise RuntimeError("Failed actor is missing from live level: {}".format(previous.get("ActorLabel")))
        landmark_record = source["ResolvedLandmarks"].get(previous["TargetLandmark"], {})
        landmark = actors_by_path.get(landmark_record.get("ActorPath"))
        if not landmark:
            raise RuntimeError("Target landmark is missing from live level: {}".format(previous["TargetLandmark"]))
        shape = _actor_clearance_shape(actor)
        origin = previous["NewTransform"]["Location"]
        candidates = []
        for distance in distances:
            for dx, dy, direction in directions:
                candidate = _candidate_surface(world, actor,
                                               origin[0] + dx*distance,
                                               origin[1] + dy*distance, shape)
                candidate.update({"OffsetDistance": distance, "Direction": direction,
                                  "DistanceToLandmarkCm": round(_xy_distance(
                                      unreal.Vector(candidate["XY"][0], candidate["XY"][1], 0.0),
                                      landmark.get_actor_location()), 3)})
                candidates.append(candidate)
                blocker = candidate.get("Clearance", {}).get("BlockingActor")
                if blocker:
                    blocker_labels.add(blocker)
        participants = [row["ActorLabel"] for row in source.get("ProposedChanges", [])
                        if row.get("QuestId") == previous.get("QuestId")
                        and row.get("TargetLandmark") == previous.get("TargetLandmark")
                        and row.get("PlacementStation") == previous.get("PlacementStation")]
        rows.append({
            "ActorLabel": previous["ActorLabel"], "ActorPath": previous["ActorPath"],
            "ActorClass": previous["ActorClass"], "ActorKind": previous["ActorKind"],
            "QuestId": previous["QuestId"], "ObjectiveId": previous["ObjectiveId"],
            "TargetLandmark": previous["TargetLandmark"],
            "PlacementStation": previous["PlacementStation"],
            "PlacementPurpose": previous["PlacementPurpose"],
            "OriginalFailedTransform": previous["NewTransform"],
            "OriginalGroundingIssues": previous.get("GroundingIssues", []),
            "OriginalCollisionIssues": previous.get("CollisionIssues", []),
            "RuntimeSensitive": previous.get("RuntimeRelocationPreserved", False),
            "SceneParticipants": participants,
            "IntendedPlayerApproach": previous.get("StagingWarnings", []),
            "TargetLandmarkTransform": _transform_json(landmark),
            "TargetLandmarkBounds": {"Origin": _v3(_bounds(landmark)[0]),
                                     "Extent": _v3(_bounds(landmark)[1])},
            "ClearanceShape": shape, "Candidates": candidates,
            "AcceptedCandidateCount": sum(1 for candidate in candidates if candidate["Accepted"]),
        })
        for issue in previous.get("GroundingIssues", []) + previous.get("CollisionIssues", []):
            match = re.search(r"(?:first blocker is|blocked by)\s+([^\s(]+)", issue)
            if match:
                blocker_labels.add(match.group(1))
    blocker_rows = []
    for label in sorted(blocker_labels):
        for actor in actors_by_label.get(label, []):
            origin, extent = _bounds(actor)
            blocker_rows.append({"ActorLabel": label, "ActorClass": _class_name(actor),
                                 "ActorPath": _path(actor), "StaticMesh": _static_mesh_path(actor),
                                 "Transform": _transform_json(actor),
                                 "WorldBounds": {"Origin": _v3(origin), "Extent": _v3(extent)}})
    result = {"SchemaVersion": 1, "GeneratedAtLocal": _now(), "Mode": BLOCKER_DIAGNOSTIC,
              "TargetMap": map_path, "SourceReport": source_path,
              "FailureCount": len(rows), "Records": rows,
              "BlockingGeometry": blocker_rows,
              "ActorsModified": False, "LevelSaved": False,
              "ProtectedMapFileBefore": _protected_file_state()}
    result["ProtectedMapFileAfter"] = _protected_file_state()
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    path = os.path.join(REPORT_DIRECTORY, "physical_blocker_candidates_{}.json".format(stamp))
    with open(path, "w") as stream:
        json.dump(result, stream, indent=2)
    unreal.log("{} targeted physical-blocker diagnostic: {}".format(LOG, path))
    return result


def _point_inside_rotated_box_xy(point_xy, center_xy, extent_xy, yaw):
    dx = float(point_xy[0]) - float(center_xy[0])
    dy = float(point_xy[1]) - float(center_xy[1])
    radians = math.radians(float(yaw))
    cosine, sine = math.cos(radians), math.sin(radians)
    local_x = cosine*dx + sine*dy
    local_y = -sine*dx + cosine*dy
    return (abs(local_x) <= float(extent_xy[0])
            and abs(local_y) <= float(extent_xy[1]))


def diagnose_afternoon_trigger_locally():
    """Bounded P6-only search using the live trigger's actual rotated box."""
    world, map_path = _assert_target_world()
    actors = _all_actors()
    by_label = {_label(actor): actor for actor in actors}
    large = by_label.get("HBCT_SM_Trigger_Afternoon")
    smaller_labels = ("HBCT_SM_Trigger_RecordAfternoon", "HBCT_SM_Trigger_TargetSweetDrink")
    smaller = [by_label.get(label) for label in smaller_labels]
    if not large or not all(smaller):
        raise RuntimeError("All three Q5 afternoon triggers must exist in the live level.")
    box = _safe_property(large, "box_component")
    if not box:
        raise RuntimeError("HBCT_SM_Trigger_Afternoon has no BoxComponent.")
    extent = box.get_scaled_box_extent()
    override = PHYSICAL_PLACEMENT_OVERRIDES["HBCT_SM_Trigger_Afternoon"]
    base_xy = [float(override["XY"][0]), float(override["XY"][1])]
    yaw = float(override["Yaw"])
    smaller_center = list(base_xy)
    directions = ((1.0, 0.0, "E"), (-1.0, 0.0, "W"), (0.0, 1.0, "N"),
                  (0.0, -1.0, "S"), (0.70710678, 0.70710678, "NE"),
                  (-0.70710678, 0.70710678, "NW"),
                  (0.70710678, -0.70710678, "SE"),
                  (-0.70710678, -0.70710678, "SW"))
    candidates = []
    for distance in (25.0, 50.0, 75.0, 100.0, 125.0, 150.0, 200.0):
        for dx, dy, direction in directions:
            x, y = base_xy[0]+dx*distance, base_xy[1]+dy*distance
            ground, terrain = _trace_ground(world, unreal.Vector(x, y, 0.0), [large])
            clearance = (_box_clear(world, ground, [large], extent, yaw)
                         if ground else {"Clear": False, "Error": "No Landscape ground"})
            covers_smaller = _point_inside_rotated_box_xy(
                smaller_center, (x, y), (extent.x, extent.y), yaw)
            candidates.append({"Type": "PositionOnly", "OffsetDistance": distance,
                               "Direction": direction, "XY": [round(x, 3), round(y, 3)],
                               "GroundZ": round(float(ground.z), 3) if ground else None,
                               "Grounded": bool(ground), "Terrain": terrain,
                               "Extent": _v3(extent), "Yaw": yaw,
                               "Clearance": clearance,
                               "CoversBothSmallerTriggerCenters": covers_smaller,
                               "Accepted": bool(ground and clearance.get("Clear") and covers_smaller)})
    for reduced_xy in (270.0, 260.0, 250.0, 240.0, 220.0):
        test_extent = unreal.Vector(reduced_xy, reduced_xy, extent.z)
        ground, terrain = _trace_ground(
            world, unreal.Vector(base_xy[0], base_xy[1], 0.0), [large])
        clearance = (_box_clear(world, ground, [large], test_extent, yaw)
                     if ground else {"Clear": False, "Error": "No Landscape ground"})
        candidates.append({"Type": "ExtentOnly", "OffsetDistance": 0.0,
                           "Direction": "None", "XY": [round(base_xy[0], 3), round(base_xy[1], 3)],
                           "GroundZ": round(float(ground.z), 3) if ground else None,
                           "Grounded": bool(ground), "Terrain": terrain,
                           "Extent": _v3(test_extent), "Yaw": yaw,
                           "Clearance": clearance,
                           "CoversBothSmallerTriggerCenters": True,
                           "Accepted": bool(ground and clearance.get("Clear"))})
    geometry = []
    for label in ("P6", "VM_P6_DrinkStall") + ("HBCT_SM_Trigger_Afternoon",) + smaller_labels:
        actor = by_label.get(label)
        if not actor:
            continue
        origin, actor_extent = _bounds(actor)
        actor_box = _safe_property(actor, "box_component")
        geometry.append({"ActorLabel": label, "ActorPath": _path(actor),
                         "ActorClass": _class_name(actor), "StaticMesh": _static_mesh_path(actor),
                         "Transform": _transform_json(actor),
                         "WorldBounds": {"Origin": _v3(origin), "Extent": _v3(actor_extent)},
                         "BoxExtent": (_v3(actor_box.get_scaled_box_extent()) if actor_box else None),
                         "BoxCollision": (_component_collision_json(actor_box) if actor_box else None)})
    accepted = [candidate for candidate in candidates if candidate["Accepted"]]
    result = {"SchemaVersion": 1, "GeneratedAtLocal": _now(),
              "Mode": AFTERNOON_LOCAL_DIAGNOSTIC, "TargetMap": map_path,
              "BaseProposedXY": base_xy, "Yaw": yaw, "ActualExtent": _v3(extent),
              "Geometry": geometry, "Candidates": candidates,
              "AcceptedCandidateCount": len(accepted),
              "SmallestAcceptedCandidate": accepted[0] if accepted else None,
              "ActorsModified": False, "LevelSaved": False,
              "ProtectedMapFileBefore": _protected_file_state()}
    result["ProtectedMapFileAfter"] = _protected_file_state()
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    path = os.path.join(REPORT_DIRECTORY, "afternoon_trigger_local_{}.json".format(stamp))
    with open(path, "w") as stream:
        json.dump(result, stream, indent=2)
    unreal.log("{} local Q5 afternoon-trigger diagnostic: {}".format(LOG, path))
    return result


def _nav_bounds(actors):
    result = []
    for actor in actors:
        if "navmeshboundsvolume" in _class_name(actor).lower():
            origin, extent = _bounds(actor)
            result.append({"Actor": actor, "ActorLabel": _label(actor), "Origin": origin, "Extent": extent})
    return result


def _inside_nav(point, nav_bounds):
    for row in nav_bounds:
        origin, extent = row["Origin"], row["Extent"]
        if (abs(point.x-origin.x) <= extent.x and abs(point.y-origin.y) <= extent.y
                and abs(point.z-origin.z) <= extent.z):
            return True, row["ActorLabel"]
    return False, ""


def _recommended_nav_box(name, keys, landmarks, xy_margin=1200.0, z_margin=1500.0):
    points = []
    missing = []
    for key in keys:
        record = landmarks.get(key, {})
        location = record.get("Transform", {}).get("Location")
        if not location:
            missing.append(key)
            continue
        points.append(location)
    if not points:
        return {"Name": name, "Landmarks": list(keys), "Missing": missing,
                "Status": "Unavailable"}
    minima = [min(point[index] for point in points) for index in range(3)]
    maxima = [max(point[index] for point in points) for index in range(3)]
    center = [(minima[index] + maxima[index]) * 0.5 for index in range(3)]
    extent = [(maxima[0]-minima[0])*0.5 + xy_margin,
              (maxima[1]-minima[1])*0.5 + xy_margin,
              max((maxima[2]-minima[2])*0.5 + z_margin, z_margin)]
    return {"Name": name, "Landmarks": list(keys), "Missing": missing,
            "Status": "Recommended", "Center": [round(value, 3) for value in center],
            "Extent": [round(value, 3) for value in extent],
            "Purpose": "Candidate NavMeshBoundsVolume coverage; validate terrain connectivity before placement."}


def _navigation_recommendations(landmarks, nav_bounds):
    groups = [
        ("EntranceAndMarket", ("PLAYER_START", "VILLAGE_ENTRANCE", "P4_MAIN_MARKET",
                               "P5_KIOSK", "P6_DRINK_STALL", "P7_SHOP_ROW")),
        ("HealthPost", ("P1_HEALTH_POST", "HEALTH_GATE")),
        ("MaliPrasertSaengHomes", ("H1_MALI", "H2_PRASERT", "H3_SAENG")),
        ("NorthernCommunity", ("P3_EXERCISE_PARK", "P2_PAVILION", "H6_CHAI")),
    ]
    boxes = [_recommended_nav_box(name, keys, landmarks) for name, keys in groups]
    boxes.extend([
        _recommended_nav_box("MainWalkingCorridor_H6_to_P3",
                             ("H6_CHAI", "P3_EXERCISE_PARK"), landmarks, 900.0, 1800.0),
        _recommended_nav_box("BackupWalkingCorridor_H6_to_P2",
                             ("H6_CHAI", "P2_PAVILION"), landmarks, 900.0, 1800.0),
    ])
    landmark_coverage = []
    required_keys = ("PLAYER_START", "VILLAGE_ENTRANCE", "P4_MAIN_MARKET", "P1_HEALTH_POST",
                     "H1_MALI", "H2_PRASERT", "H3_SAENG", "P3_EXERCISE_PARK",
                     "P2_PAVILION", "H6_CHAI")
    for key in required_keys:
        location = landmarks.get(key, {}).get("Transform", {}).get("Location")
        if not location:
            landmark_coverage.append({"Landmark": key, "Covered": False,
                                      "Reason": "Resolved transform unavailable"})
            continue
        covered, volume = _inside_nav(unreal.Vector(*location), nav_bounds)
        landmark_coverage.append({"Landmark": key, "Location": location,
                                  "Covered": covered, "Volume": volume})
    return {"RequiredLandmarkCoverage": landmark_coverage,
            "RecommendedBounds": boxes,
            "MajorConnectivityConcerns": [
                "Existing Recast data was not rebuilt or used to claim reachability.",
                "P3 and northern village elevation/path slopes require manual Recast path inspection.",
                "Main and backup routes need distinct connected corridors; the report's linear samples are not proof of walkability.",
                "Recommended boxes describe minimum spatial coverage only and may need splitting to follow roads without covering cliffs or buildings.",
            ]}


def _nav_project(world, location, extent=None):
    """Project a proposed destination to the live editor world's generated nav data."""
    extent = extent or unreal.Vector(100.0, 100.0, 500.0)
    source = unreal.Vector(float(location[0]), float(location[1]), float(location[2]))
    try:
        projected = unreal.NavigationSystemV1.project_point_to_navigation(
            world, source, None, None, extent)
        if projected is None:
            return {"Projected": False, "Source": _v3(source),
                    "QueryExtent": _v3(extent), "Reason": "No generated navigation point found"}
        return {"Projected": True, "Source": _v3(source),
                "ProjectedLocation": _v3(projected), "QueryExtent": _v3(extent),
                "ProjectionDistanceCm": round(_distance(source, projected), 3)}
    except Exception as exc:
        return {"Projected": False, "Source": _v3(source),
                "QueryExtent": _v3(extent), "Reason": "Navigation projection API failed: {}".format(exc)}


def _nav_path(world, start_location, end_location):
    """Test a non-trivial synchronous path against existing Recast data; never rebuild nav."""
    start_projection = _nav_project(world, start_location)
    end_projection = _nav_project(world, end_location)
    result = {"StartProjection": start_projection, "EndProjection": end_projection,
              "Connected": False, "Verified": False, "Partial": None,
              "PathPointCount": 0, "PathLengthCm": None, "PathCost": None}
    if not start_projection["Projected"] or not end_projection["Projected"]:
        result["Reason"] = "One or both endpoints are absent from generated navigation data"
        return result
    start = unreal.Vector(*start_projection["ProjectedLocation"])
    end = unreal.Vector(*end_projection["ProjectedLocation"])
    direct_distance = _distance(start, end)
    result["ProjectedEndpointDistanceCm"] = round(direct_distance, 3)
    if direct_distance < 50.0:
        result["Verified"] = True
        result["Degenerate"] = True
        result["Reason"] = "Stage endpoints are effectively identical; no ordered walk is represented"
        return result
    try:
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(
            world, start, end, None, None)
        if path is None:
            result["Reason"] = "Navigation API returned no path object"
            return result
        valid = bool(path.is_valid())
        partial = bool(path.is_partial())
        points = list(path.path_points or [])
        result.update({"Verified": True, "Valid": valid, "Partial": partial,
                       "PathPointCount": len(points),
                       "PathPoints": [_v3(point) for point in points],
                       "Connected": bool(valid and not partial and len(points) >= 2)})
        try:
            result["PathLengthCm"] = round(float(path.get_path_length()), 3)
        except Exception:
            pass
        try:
            result["PathCost"] = round(float(path.get_path_cost()), 3)
        except Exception:
            pass
        if not result["Connected"]:
            result["Reason"] = "Path is invalid, partial, or lacks a traversable segment"
        return result
    except Exception as exc:
        result["Reason"] = "Navigation path API failed: {}".format(exc)
        return result


def _route_review(world, changes_by_label, name, labels):
    rows = []
    missing = []
    locations = []
    for label in labels:
        row = changes_by_label.get(label)
        if not row:
            missing.append(label)
            continue
        locations.append((label, row["NewTransform"]["Location"]))
    if missing:
        return {"Route": name, "StageLabels": list(labels), "Status": "UNVERIFIED",
                "Connected": False, "MissingStages": missing,
                "Reason": "Required proposed movement stages are missing"}
    for index in range(len(locations)-1):
        start_label, start = locations[index]
        end_label, end = locations[index+1]
        test = _nav_path(world, start, end)
        test.update({"From": start_label, "To": end_label, "SequenceIndex": index})
        rows.append(test)
    connected = bool(rows) and all(row.get("Connected") for row in rows)
    verified = bool(rows) and all(row.get("Verified") for row in rows)
    return {"Route": name, "StageLabels": list(labels), "SegmentCount": len(rows),
            "Segments": rows, "Connected": connected,
            "Status": "PASS" if connected else ("FAIL" if verified else "UNVERIFIED")}


def _actual_navigation_review(world, actors, changes, landmarks, nav_bounds):
    """Focused pre-APPLY audit of generated nav data and approved proposed transforms."""
    changes_by_label = {row["ActorLabel"]: row for row in changes}
    projections = []
    for row in changes:
        projection = _nav_project(world, row["NewTransform"]["Location"])
        projections.append({"ActorLabel": row["ActorLabel"], "ActorPath": row["ActorPath"],
                            "QuestId": row["QuestId"], "RuntimeSensitive": row["RuntimeRelocationPreserved"],
                            "Projection": projection})

    route_specs = (
        ("Q3_Saeng_DemonstrationWalk", ("HBCT_Location_RoleModel_Demo_Saeng",
          "HBCT_Location_RoleModel_WalkMid_Saeng", "HBCT_Location_RoleModel_WalkEnd_Saeng")),
        ("Q3_Somchai_DemonstrationWalk", ("HBCT_Location_RoleModel_Demo_Somchai",
          "HBCT_Location_RoleModel_WalkMid_Somchai", "HBCT_Location_RoleModel_WalkEnd_Somchai")),
        ("Q4_Chai_MainWalk", ("HBCT_SS_Location_ChaiWalkStart", "HBCT_SS_Location_ChaiWalkMid",
          "HBCT_SS_Location_ChaiWalkEnd")),
        ("Q4_Neighbor_MeetChai", ("HBCT_SS_Location_PlanNeighbor",
          "HBCT_SS_Location_NeighborWalkStart")),
        ("Q4_Neighbor_MainWalk", ("HBCT_SS_Location_NeighborWalkStart", "HBCT_SS_Location_NeighborWalkMid",
          "HBCT_SS_Location_NeighborWalkEnd")),
        ("Q4_Chai_BackupWalk", ("HBCT_SS_Location_ChaiBackupStart", "HBCT_SS_Location_ChaiBackupEnd")),
        ("Q4_Wife_BackupWalk", ("HBCT_SS_Location_WifeBackupStart", "HBCT_SS_Location_WifeBackupEnd")),
        ("Q5_EveningWalk", ("HBCT_SM_Trigger_WalkStart", "HBCT_SM_Location_ChaiWalkMid",
          "HBCT_SM_Location_ChaiWalkEnd")),
        ("Q5_AdjustedWalk", ("HBCT_SM_Trigger_AdjustedWalkStart", "HBCT_SM_Location_AdjustedWalkMid",
          "HBCT_SM_Location_AdjustedWalkEnd")),
        ("Q6_Chai_WalkingChallenge", ("HBCT_PR_Location_PRChallengeWalkStart",
          "HBCT_PR_Location_PRChallengeWalkMid", "HBCT_PR_Location_PRChallengeWalkEnd")),
    )
    routes = [_route_review(world, changes_by_label, name, labels) for name, labels in route_specs]

    pair_specs = (
        ("Q4_MainPair", (("HBCT_SS_Location_ChaiWalkStart", "HBCT_SS_Location_NeighborWalkStart"),
                         ("HBCT_SS_Location_ChaiWalkMid", "HBCT_SS_Location_NeighborWalkMid"),
                         ("HBCT_SS_Location_ChaiWalkEnd", "HBCT_SS_Location_NeighborWalkEnd"))),
        ("Q4_BackupPair", (("HBCT_SS_Location_ChaiBackupStart", "HBCT_SS_Location_WifeBackupStart"),
                           ("HBCT_SS_Location_ChaiBackupEnd", "HBCT_SS_Location_WifeBackupEnd"))),
    )
    pair_spacing = []
    for pair_name, stages in pair_specs:
        stage_rows = []
        for left, right in stages:
            left_row, right_row = changes_by_label.get(left), changes_by_label.get(right)
            if not left_row or not right_row:
                stage_rows.append({"Actors": [left, right], "Pass": False, "Reason": "Missing stage anchor"})
                continue
            a = unreal.Vector(*left_row["NewTransform"]["Location"])
            b = unreal.Vector(*right_row["NewTransform"]["Location"])
            separation = _distance(a, b)
            stage_rows.append({"Actors": [left, right], "SeparationCm": round(separation, 3),
                               "Pass": separation >= 100.0,
                               "Reason": "" if separation >= 100.0 else "Paired NPC anchors overlap"})
        pair_spacing.append({"Pair": pair_name, "Stages": stage_rows,
                             "Pass": all(stage["Pass"] for stage in stage_rows)})

    actors_by_path = {_path(actor): actor for actor in actors}
    house_access = []
    for key in ("H1_MALI", "H2_PRASERT", "H3_SAENG", "H6_CHAI", "H7_NEIGHBOR", "H8_FAMILY"):
        house_rows = [row for row in changes if row["TargetLandmark"] == key]
        approach = _approach_reference(key, landmarks, actors_by_path)
        if not house_rows or approach is None:
            house_access.append({"House": key, "Status": "UNVERIFIED",
                                 "Reason": "No proposed staging rows or approach reference"})
            continue
        locations = [row["NewTransform"]["Location"] for row in house_rows]
        center = [sum(point[index] for point in locations)/len(locations) for index in range(3)]
        test = _nav_path(world, _v3(approach), center)
        house_access.append({"House": key, "ProposedPlacementCount": len(house_rows),
                             "ProposedGroupCenter": [round(value, 3) for value in center],
                             "ApproachReference": _v3(approach), "PathTest": test,
                             "Status": "PASS" if test.get("Connected") else (
                                 "FAIL" if test.get("Verified") else "UNVERIFIED")})

    nav_data = []
    for actor in actors:
        class_name = _class_name(actor)
        if "recastnavmesh" in class_name.lower() or "navigationdata" in class_name.lower():
            nav_data.append({"ActorLabel": _label(actor), "ActorClass": class_name,
                             "ActorPath": _path(actor), "Transform": _transform_json(actor)})
    runtime = [row for row in projections if row["RuntimeSensitive"]]
    destination_count = sum(1 for row in projections if row["Projection"].get("Projected"))
    route_blockers = [row["Route"] for row in routes if row["Status"] != "PASS"]
    pair_blockers = [row["Pair"] for row in pair_spacing if not row["Pass"]]
    house_blockers = [row["House"] for row in house_access if row["Status"] != "PASS"]
    return {
        "GeneratedNavDataActors": nav_data,
        "NavMeshBoundsVolumeCount": len(nav_bounds),
        "ProjectionQueryExtent": [100.0, 100.0, 500.0],
        "DestinationCount": len(projections), "DestinationsProjected": destination_count,
        "DestinationProjectionFailures": [row for row in projections if not row["Projection"].get("Projected")],
        "DestinationProjections": projections,
        "Routes": routes, "PairedStageSpacing": pair_spacing, "HouseRoadYardAccess": house_access,
        "RuntimeSensitiveExpected": 46, "RuntimeSensitiveReviewed": len(runtime),
        "RuntimeSensitiveProjected": sum(1 for row in runtime if row["Projection"].get("Projected")),
        "RuntimeSensitiveProjectionFailures": [row for row in runtime if not row["Projection"].get("Projected")],
        "ActualBlockers": {"Routes": route_blockers, "PairSpacing": pair_blockers,
                           "HouseAccess": house_blockers},
        "ActualNavigationPass": bool(destination_count == len(projections)
                                     and not route_blockers and not pair_blockers and not house_blockers
                                     and len(runtime) == 46),
        "NavigationWasRebuiltByTool": False,
    }


def diagnose_q4_routes():
    """Targeted read-only validation of only the seven corrected Q4 stage anchors."""
    world, map_path = _assert_target_world()
    actors = _all_actors()
    by_label = {_label(actor): actor for actor in actors}
    source_path = _latest_report_only_path()
    with open(source_path, "r") as stream:
        source = json.load(stream)
    source_changes = {row["ActorLabel"]: row for row in source.get("ProposedChanges", [])}
    labels = (
        "HBCT_SS_Location_NeighborWalkStart", "HBCT_SS_Location_NeighborWalkMid",
        "HBCT_SS_Location_NeighborWalkEnd", "HBCT_SS_Location_ChaiBackupStart",
        "HBCT_SS_Location_WifeBackupStart", "HBCT_SS_Location_ChaiBackupEnd",
        "HBCT_SS_Location_WifeBackupEnd",
    )
    candidates = {}
    for label in labels:
        actor = by_label.get(label)
        override = PHYSICAL_PLACEMENT_OVERRIDES.get(label)
        if not actor or not override:
            candidates[label] = {"Accepted": False, "Reason": "Actor or override missing"}
            continue
        shape = _actor_clearance_shape(actor)
        surface = _candidate_surface(world, actor, override["XY"][0], override["XY"][1], shape)
        ground = None
        if surface.get("Grounded"):
            ground = unreal.Vector(float(surface["XY"][0]), float(surface["XY"][1]),
                                   float(surface["GroundZ"]))
        if ground and shape["Source"] == "BoxComponent":
            clearance = _box_clear(world, ground, [actor], unreal.Vector(*shape["Extent"]),
                                   yaw=float(override["Yaw"]))
        elif ground:
            clearance = _capsule_clear(world, ground, [actor], radius=float(shape["Radius"]),
                                       half_height=float(shape["HalfHeight"]))
        else:
            clearance = {"Clear": False, "Error": "No grounded candidate"}
        accepted = bool(surface.get("StableSurface") and clearance.get("Clear"))
        candidates[label] = {"ActorLabel": label, "ActorPath": _path(actor),
                             "ActorClass": _class_name(actor), "Shape": shape,
                             "ProposedXY": list(override["XY"]), "Yaw": override["Yaw"],
                             "GroundZ": surface.get("GroundZ"), "Terrain": surface.get("Terrain"),
                             "FootprintSamples": surface.get("FootprintSamples"),
                             "Clearance": clearance, "Accepted": accepted,
                             "ProposedLocation": ([round(override["XY"][0], 3),
                                                   round(override["XY"][1], 3),
                                                   surface.get("GroundZ")] if ground else None)}

    def location(label):
        candidate = candidates.get(label, {})
        if candidate.get("Accepted"):
            return candidate["ProposedLocation"]
        prior = source_changes.get(label)
        return prior.get("NewTransform", {}).get("Location") if prior else None

    planned = {}
    for label in labels:
        loc = location(label)
        if loc:
            planned[label] = {"ActorLabel": label, "NewTransform": {"Location": loc}}
    for label in ("HBCT_SS_Location_PlanNeighbor", "HBCT_SS_Location_ChaiWalkStart",
                  "HBCT_SS_Location_ChaiWalkMid", "HBCT_SS_Location_ChaiWalkEnd"):
        prior = source_changes.get(label)
        if prior:
            planned[label] = prior

    routes = [
        _route_review(world, planned, "Q4_Neighbor_MeetChai",
                      ("HBCT_SS_Location_PlanNeighbor", "HBCT_SS_Location_NeighborWalkStart")),
        _route_review(world, planned, "Q4_Neighbor_MainWalk",
                      ("HBCT_SS_Location_NeighborWalkStart", "HBCT_SS_Location_NeighborWalkMid",
                       "HBCT_SS_Location_NeighborWalkEnd")),
        _route_review(world, planned, "Q4_Chai_BackupWalk",
                      ("HBCT_SS_Location_ChaiBackupStart", "HBCT_SS_Location_ChaiBackupEnd")),
        _route_review(world, planned, "Q4_Wife_BackupWalk",
                      ("HBCT_SS_Location_WifeBackupStart", "HBCT_SS_Location_WifeBackupEnd")),
    ]
    pair_specs = (
        ("MainStart", "HBCT_SS_Location_ChaiWalkStart", "HBCT_SS_Location_NeighborWalkStart"),
        ("MainMid", "HBCT_SS_Location_ChaiWalkMid", "HBCT_SS_Location_NeighborWalkMid"),
        ("MainEnd", "HBCT_SS_Location_ChaiWalkEnd", "HBCT_SS_Location_NeighborWalkEnd"),
        ("BackupStart", "HBCT_SS_Location_ChaiBackupStart", "HBCT_SS_Location_WifeBackupStart"),
        ("BackupEnd", "HBCT_SS_Location_ChaiBackupEnd", "HBCT_SS_Location_WifeBackupEnd"),
    )
    spacing = []
    for stage, left, right in pair_specs:
        a, b = location(left), location(right)
        if not a or not b:
            spacing.append({"Stage": stage, "Pass": False, "Reason": "Missing proposed coordinate"})
            continue
        distance = _distance(unreal.Vector(*a), unreal.Vector(*b))
        spacing.append({"Stage": stage, "Actors": [left, right],
                        "SeparationCm": round(distance, 3),
                        "Pass": 120.0 <= distance <= 200.0})
    passed = (all(row.get("Accepted") for row in candidates.values())
              and all(route.get("Status") == "PASS" for route in routes)
              and all(row.get("Pass") for row in spacing))
    result = {"SchemaVersion": 1, "GeneratedAtLocal": _now(), "Mode": Q4_ROUTE_DIAGNOSTIC,
              "TargetMap": map_path, "SourceReport": source_path,
              "Candidates": candidates, "Routes": routes, "PairSpacing": spacing,
              "Passed": passed, "ActorsModified": False, "LevelSaved": False,
              "ProtectedMapFileBefore": _protected_file_state()}
    result["ProtectedMapFileAfter"] = _protected_file_state()
    if not os.path.isdir(REPORT_DIRECTORY):
        os.makedirs(REPORT_DIRECTORY)
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    path = os.path.join(REPORT_DIRECTORY, "q4_route_diagnostic_{}.json".format(stamp))
    with open(path, "w") as stream:
        json.dump(result, stream, indent=2)
    unreal.log("{} targeted Q4 route diagnostic: {} pass={}".format(LOG, path, passed))
    return result


def _route_position(route, text, landmarks, actors_by_path):
    routes = {
        "Q3_MAIN": ("P3_EXERCISE_PARK", "P2_PAVILION"),
        "Q4_MAIN": ("H6_CHAI", "P3_EXERCISE_PARK"),
        "Q4_BACKUP": ("H6_CHAI", "P2_PAVILION"),
        "Q5_MAIN": ("H6_CHAI", "P3_EXERCISE_PARK"),
        "Q5_ADJUSTED": ("H6_CHAI", "P2_PAVILION"),
        "Q6_CHALLENGE": ("H6_CHAI", "P3_EXERCISE_PARK"),
    }
    start_key, end_key = routes[route]
    start = _resolved_actor(landmarks, actors_by_path, start_key)
    end = _resolved_actor(landmarks, actors_by_path, end_key)
    if not start or not end:
        return None, "Missing route endpoint landmark"
    a = start.get_actor_location()
    b = end.get_actor_location()
    if any(token in text for token in ("start", "home", "prepare")):
        t = 0.10
    elif any(token in text for token in ("end", "destination", "villagehead")):
        t = 0.90
    elif "mid" in text:
        t = 0.52
    else:
        t = 0.15
    # This is only a proposed corridor sample.  REPORT_ONLY marks route points for
    # navigation review; APPLY requires ground/collision validation but never
    # claims Recast reachability.
    return unreal.Vector(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t), "Linear corridor sample; manual/Recast path review required"


def _actor_slot_offset(row, group_index, group_count, outward, tangent):
    kind = row["Kind"]
    if kind == "NPC":
        centered = group_index - (group_count-1)*0.5
        return tangent * (centered * 190.0) + outward * 40.0
    if kind in ("ObjectiveTrigger", "InteractionPoint"):
        return outward * 260.0
    if kind == "CameraTarget":
        return outward * 420.0 + unreal.Vector(0.0, 0.0, 160.0)
    if kind == "MovementAnchor":
        return outward * 80.0
    return unreal.Vector(0.0, 0.0, 0.0)


def _plan(world, actors, landmarks, inventory):
    actors_by_path = {_path(actor): actor for actor in actors}
    nav = _nav_bounds(actors)
    unresolved = []
    assignments = []
    groups = collections.defaultdict(list)
    for row in inventory:
        target, station, purpose, route = _target_rule(row)
        if not target:
            unresolved.append({"Type": "QuestActorMapping", "ActorLabel": row["ActorLabel"],
                               "ActorClass": row["ActorClass"], "QuestId": row["QuestId"],
                               "Reason": "No identity-based scene assignment rule matched."})
            continue
        groups[(target, station, route)].append((row, purpose))

    for group_key in sorted(groups):
        target, station, route = group_key
        landmark = _resolved_actor(landmarks, actors_by_path, target)
        rows = sorted(groups[group_key], key=lambda pair: (pair[0]["Kind"] != "NPC", pair[0]["ActorLabel"].lower()))
        if not landmark:
            for row, purpose in rows:
                unresolved.append({"Type": "QuestActorMapping", "ActorLabel": row["ActorLabel"],
                                   "QuestId": row["QuestId"], "TargetLandmark": target,
                                   "Reason": "Target landmark is unresolved."})
            continue
        station_point, outward, tangent, facing_basis = _station_point(
            landmark, target, station, landmarks, actors_by_path)
        npc_rows = [pair for pair in rows if pair[0]["Kind"] == "NPC"]
        npc_indices = {pair[0]["ActorPath"]: i for i, pair in enumerate(npc_rows)}
        for row, purpose in rows:
            actor = row["Actor"]
            text = row["SemanticText"]
            route_note = ""
            desired = station_point
            if route:
                route_value, route_note = _route_position(route, text, landmarks, actors_by_path)
                if route_value:
                    desired = route_value
            npc_index = npc_indices.get(row["ActorPath"], 0)
            offset = _actor_slot_offset(row, npc_index, len(npc_rows), outward, tangent)
            desired = desired + offset
            physical_override = PHYSICAL_PLACEMENT_OVERRIDES.get(row["ActorLabel"])
            if physical_override:
                desired = unreal.Vector(float(physical_override["XY"][0]),
                                        float(physical_override["XY"][1]), desired.z)
            # Ignore only the actor being proposed.  The target building remains
            # collision-visible so a roof/interior point fails safely.
            ground, terrain = _trace_ground(world, desired, [actor])
            if ground:
                z_offset = 0.0
                if row["Kind"] == "CameraTarget":
                    z_offset = 160.0
                desired = unreal.Vector(ground.x, ground.y, ground.z + z_offset)
            if row["Kind"] == "NPC":
                shape = _actor_clearance_shape(actor)
                collision = _capsule_clear(world, desired, [actor],
                                           radius=shape["Radius"],
                                           half_height=shape["HalfHeight"])
                collision["Shape"] = shape
            elif physical_override:
                shape = _actor_clearance_shape(actor)
                if shape["Source"] == "BoxComponent":
                    extent_values = shape["Extent"]
                    collision = _box_clear(
                        world, desired, [actor], unreal.Vector(*extent_values),
                        yaw=physical_override.get("Yaw", actor.get_actor_rotation().yaw))
                else:
                    collision = _capsule_clear(world, desired, [actor],
                                               radius=shape["Radius"],
                                               half_height=shape["HalfHeight"])
                collision["Shape"] = shape
                collision["IntendedNonBlockingTriggerOverlapsIgnored"] = True
            else:
                collision = {"Clear": True, "NotApplicable": True}
            nav_covered, nav_actor = _inside_nav(desired, nav)
            yaw = math.degrees(math.atan2(-outward.y, -outward.x))
            if physical_override and "Yaw" in physical_override:
                yaw = float(physical_override["Yaw"])
            old = row["Transform"]
            rotation = list(old["Rotation"])
            rotation[1] = round(yaw, 3)
            new = _transform_from_values(_v3(desired), rotation, old["Scale"])
            validation = "Passed"
            confidence = "High"
            issues = []
            grounding_issues = []
            collision_issues = []
            navigation_warnings = []
            staging_warnings = []
            if not terrain.get("Supported"):
                validation = "Failed"
                confidence = "Low"
                grounding_issues.append(terrain.get("Error", "Unsupported terrain"))
            if not collision.get("Clear"):
                validation = "Failed"
                confidence = "Low"
                collision_issues.append("NPC capsule blocked by {}".format(
                    collision.get("BlockingActor") or "unknown"))
            if not nav_covered:
                navigation_warnings.append("Outside all NavMeshBoundsVolume bounds")
                if validation == "Passed":
                    validation = "ManualReview"
                    confidence = "Medium"
            if route:
                navigation_warnings.append(route_note)
                if validation == "Passed":
                    validation = "ManualReview"
                    confidence = "Medium"
            if "not asserted as a doorway" in facing_basis.lower() or "doorway not detected" in facing_basis.lower():
                staging_warnings.append(facing_basis)
                if target in ("H1_MALI", "H2_PRASERT", "H3_SAENG", "H6_CHAI", "H7_NEIGHBOR", "H8_FAMILY"):
                    if validation == "Passed":
                        validation = "ManualReview"
                        confidence = "Medium"
            issues.extend(grounding_issues)
            issues.extend(collision_issues)
            issues.extend(navigation_warnings)
            issues.extend(staging_warnings)
            assignments.append({
                "QuestId": row["QuestId"], "ObjectiveId": row["ObjectiveId"],
                "ActorLabel": row["ActorLabel"], "ActorPath": row["ActorPath"],
                "ActorClass": row["ActorClass"], "ActorKind": row["Kind"],
                "ExistingFolder": row["ExistingFolder"], "OldTransform": old,
                "NewTransform": new, "TargetLandmark": target,
                "PlacementStation": station, "PlacementPurpose": purpose,
                "Confidence": confidence, "ValidationStatus": validation,
                "ValidationIssues": issues, "TerrainValidation": terrain,
                "CollisionValidation": collision,
                "PhysicalValidationStatus": "Failed" if grounding_issues or collision_issues else "Passed",
                "GroundingStatus": "Passed" if terrain.get("Supported") else "Failed",
                "CollisionStatus": "Passed" if collision.get("Clear") else "Failed",
                "NavigationStatus": "ReviewRequired" if navigation_warnings else "BoundsCoveredOnly",
                "StagingReviewStatus": "ReviewRequired" if staging_warnings else "NoDoorwayWarning",
                "GroundingIssues": grounding_issues,
                "CollisionIssues": collision_issues,
                "NavigationWarnings": navigation_warnings,
                "StagingWarnings": staging_warnings,
                "PhysicalOverride": ({"Applied": True,
                                      "Reason": physical_override["Reason"]}
                                     if physical_override else {"Applied": False}),
                "NavBoundsCoverage": {"Covered": nav_covered, "Volume": nav_actor,
                                      "RecastPathTested": False},
                "RuntimeRelocationPreserved": bool(route or row["Kind"] == "MovementAnchor"
                                                    or "instructor" in row["ParticipantTag"].lower()
                                                    or "unclechai" in row["ParticipantTag"].lower()),
            })
    assignments.sort(key=lambda item: (item["QuestId"], item["TargetLandmark"],
                                       item["PlacementStation"], item["ActorLabel"].lower()))
    return assignments, unresolved, nav


def _summaries(inventory, changes):
    inventoried = collections.defaultdict(lambda: collections.Counter())
    proposed = collections.defaultdict(lambda: collections.Counter())
    for row in inventory:
        inventoried[row["QuestId"] or "SharedOrUnassigned"][row["Kind"]] += 1
    for row in changes:
        proposed[row["QuestId"] or "SharedOrUnassigned"][row["ActorKind"]] += 1
    def clean(source):
        return {quest: dict(sorted(counter.items())) for quest, counter in sorted(source.items())}
    return clean(inventoried), clean(proposed)


def _proposal_stability(changes):
    try:
        previous_path = _latest_report_only_path()
        with open(previous_path, "r") as stream:
            previous = json.load(stream)
    except Exception as exc:
        return {"Compared": False, "Error": str(exc)}
    current_by_path = {row["ActorPath"]: row for row in changes}
    prior_all = previous.get("ProposedChanges", [])
    prior_physically_valid = [row for row in previous.get("ProposedChanges", [])
                              if row.get("PhysicalValidationStatus") == "Passed"]
    changed_valid = []
    changed_all = []
    semantic_changes = []
    for old in prior_all:
        new = current_by_path.get(old.get("ActorPath"))
        if not new or new.get("NewTransform") != old.get("NewTransform"):
            changed_all.append(old.get("ActorLabel"))
        if new and any(new.get(field) != old.get(field)
                       for field in ("QuestId", "ObjectiveId", "TargetLandmark",
                                     "PlacementStation", "ActorKind")):
            semantic_changes.append(old.get("ActorLabel"))
    for old in prior_physically_valid:
        new = current_by_path.get(old.get("ActorPath"))
        if not new or new.get("NewTransform") != old.get("NewTransform"):
            changed_valid.append(old.get("ActorLabel"))
    return {"Compared": True, "SourceReport": previous_path,
            "PriorProposalCount": len(prior_all),
            "UnchangedAllProposalTransformCount": len(prior_all)-len(changed_all),
            "ChangedProposalTransforms": changed_all,
            "PriorPhysicallyValidCount": len(prior_physically_valid),
            "UnchangedPriorPhysicallyValidCount": len(prior_physically_valid)-len(changed_valid),
            "ChangedPriorPhysicallyValidActors": changed_valid,
            "SemanticChanges": sorted(set(semantic_changes)),
            "OverrideLabels": sorted(PHYSICAL_PLACEMENT_OVERRIDES),
            "OnlyOverrideLabelsChanged": not changed_valid and not semantic_changes}


def _house_staging_review(changes):
    house_keys = ("H1_MALI", "H2_PRASERT", "H3_SAENG", "H6_CHAI",
                  "H7_NEIGHBOR", "H8_FAMILY")
    result = []
    for key in house_keys:
        rows = [row for row in changes if row["TargetLandmark"] == key]
        categories = collections.defaultdict(list)
        independent = []
        for row in rows:
            text = (row["PlacementPurpose"] + " " + row["PlacementStation"]).lower()
            if row["ActorKind"] == "MovementAnchor" or "walking route" in text or "walk" in text:
                category = "WalkingRouteExit"
            elif any(token in text for token in ("observation", "appreciation", "family", "meal")):
                category = "YardObservation"
            elif any(token in text for token in ("roadside", "approach")):
                category = "RoadsideApproach"
            else:
                category = "PrimaryPorchConversation"
            categories[category].append(row)
            if row["ActorKind"] == "MovementAnchor" or row.get("PhysicalOverride", {}).get("Applied"):
                independent.append({"ActorLabel": row["ActorLabel"], "Category": category,
                                    "Reason": (row.get("PhysicalOverride", {}).get("Reason")
                                               or "Runtime walking-stage anchor requires route review")})
        group_rows = []
        for category in ("PrimaryPorchConversation", "YardObservation",
                         "RoadsideApproach", "WalkingRouteExit"):
            members = categories.get(category, [])
            if not members:
                group_rows.append({"Area": category, "Used": False, "ActorCount": 0})
                continue
            locations = [row["NewTransform"]["Location"] for row in members]
            center = [round(sum(location[index] for location in locations)/len(locations), 3)
                      for index in range(3)]
            group_rows.append({"Area": category, "Used": True, "ActorCount": len(members),
                               "ProposedGroupCenter": center,
                               "Scenes": sorted(set(row["PlacementPurpose"] for row in members))})
        result.append({"House": key, "PlacementCount": len(rows),
                       "SharedStagingAreas": group_rows,
                       "SpecialIndependentStaging": independent})
    return result


def _runtime_sensitive_review(changes):
    sensitive = [row for row in changes if row["RuntimeRelocationPreserved"]]
    grouped = collections.defaultdict(list)
    for row in sensitive:
        grouped[(row["QuestId"], row["TargetLandmark"], row["ActorKind"])].append(row)
    groups = []
    for (quest, target, kind), rows in sorted(grouped.items()):
        risks = []
        if any(row["PhysicalValidationStatus"] == "Failed" for row in rows):
            risks.append("Physical preflight failure")
        if any(row["NavigationStatus"] == "ReviewRequired" for row in rows):
            risks.append("Navigation/path review required")
        if kind == "MovementAnchor":
            risks.append("Preserve stage ID/order and verify Recast reachability after nav rebuild")
        groups.append({"QuestId": quest, "TargetLandmark": target, "ActorKind": kind,
                       "Count": len(rows), "Actors": [row["ActorLabel"] for row in rows],
                       "Risks": risks})
    return {"Count": len(sensitive), "RuntimeLogicChanged": False, "Groups": groups}


def _scene_component_transform(component):
    result = {}
    for key, method in (("Location", "get_world_location"),
                        ("Rotation", "get_world_rotation"),
                        ("Scale", "get_world_scale")):
        try:
            value = getattr(component, method)()
            result[key] = _r3(value) if key == "Rotation" else _v3(value)
        except Exception as exc:
            result[key] = "Unavailable: {}".format(exc)
    return result


def _json_property(actor, name):
    value = _safe_property(actor, name, None)
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    return _tag_text(value)


def _afternoon_trigger_validation(actors, changes, landmarks):
    labels = ("HBCT_SM_Trigger_Afternoon", "HBCT_SM_Trigger_RecordAfternoon",
              "HBCT_SM_Trigger_TargetSweetDrink")
    actors_by_label = {_label(actor): actor for actor in actors}
    changes_by_label = {row["ActorLabel"]: row for row in changes}
    rows = []
    for label in labels:
        actor = actors_by_label.get(label)
        change = changes_by_label.get(label)
        if not actor or not change:
            rows.append({"ActorLabel": label, "Resolved": False})
            continue
        box = _safe_property(actor, "box_component")
        extent = box.get_scaled_box_extent() if box else None
        location = change["NewTransform"]["Location"]
        gates = {name: _json_property(actor, name) for name in (
            "enabled", "require_player_interaction", "trigger_world_action",
            "trigger_world_action_once", "track_world_action_as_objective",
            "activate_current_objective", "activate_current_objective_once",
            "required_active_quest_id", "required_active_objective_id",
            "world_action_receiver_tag", "world_action_tag", "location_tag")}
        rows.append({
            "ActorLabel": label, "ActorPath": _path(actor), "ActorClass": _class_name(actor),
            "ActorKind": change["ActorKind"], "QuestId": change["QuestId"],
            "ObjectiveId": change["ObjectiveId"], "PlacementPurpose": change["PlacementPurpose"],
            "CurrentActorTransform": _transform_json(actor),
            "CurrentBoxWorldTransform": _scene_component_transform(box) if box else None,
            "BoxScaledExtent": _v3(extent) if extent else None,
            "BoxCollision": _component_collision_json(box) if box else None,
            "ActivationAndObjectiveGates": gates,
            "ProposedTransform": change["NewTransform"],
            "ProposedBoxVerticalRange": ([round(location[2]-extent.z, 3),
                                           round(location[2]+extent.z, 3)] if extent else None),
            "GroundValidation": change["TerrainValidation"],
            "SolidStructureClearance": change["CollisionValidation"],
            "PhysicalValidationStatus": change["PhysicalValidationStatus"],
            "NavBoundsCoverageOnly": change["NavBoundsCoverage"],
            "IntentionalLandscapeRelationship": (
                "Actor pivot is grounded and the query-only trigger box straddles terrain; "
                "solid clearance is tested with an equal box lifted so its bottom is 3 cm above ground."),
        })
    p6 = landmarks.get("P6_DRINK_STALL", {}).get("Transform", {}).get("Location")
    proposed = rows[0].get("ProposedTransform", {}).get("Location") if rows else None
    distance_to_p6 = None
    if p6 and proposed:
        distance_to_p6 = round(math.sqrt((p6[0]-proposed[0])**2 + (p6[1]-proposed[1])**2), 3)
    unique_paths = [row.get("ActorPath") for row in rows if row.get("ActorPath")]
    objective_gates = [str(row.get("ActivationAndObjectiveGates", {}).get(
        "required_active_objective_id") or "") for row in rows]
    quest_gates = [str(row.get("ActivationAndObjectiveGates", {}).get(
        "required_active_quest_id") or "") for row in rows]
    return {
        "RequestedCoordinate": [float(PHYSICAL_PLACEMENT_OVERRIDES["HBCT_SM_Trigger_Afternoon"]["XY"][0]),
                                float(PHYSICAL_PLACEMENT_OVERRIDES["HBCT_SM_Trigger_Afternoon"]["XY"][1]),
                                rows[0].get("ProposedTransform", {}).get("Location", [None, None, None])[2]],
        "Records": rows,
        "IdentityAndGateReview": {
            "UniqueActorIdentities": len(unique_paths) == len(labels) and len(set(unique_paths)) == len(labels),
            "RequiredQuestGates": quest_gates,
            "RequiredObjectiveGates": objective_gates,
            "AllQuestGatesAreQ5": all(value == "Q_HBCT_05_SELF_MONITORING" for value in quest_gates),
            "AllObjectiveGatesNonEmpty": all(bool(value) for value in objective_gates),
            "UnrelatedObjectiveActivationRiskDetected": not (
                all(value == "Q_HBCT_05_SELF_MONITORING" for value in quest_gates)
                and all(bool(value) for value in objective_gates)),
        },
        "ApproachReview": {
            "DistanceToP6ActorOriginCm": distance_to_p6,
            "GroundSupported": all(row.get("GroundValidation", {}).get("Supported") for row in rows),
            "SolidStructureClear": all(row.get("SolidStructureClearance", {}).get("Clear") for row in rows),
            "WithinCurrentNavBounds": all(row.get("NavBoundsCoverageOnly", {}).get("Covered") for row in rows),
            "PathReachabilityClaimed": False,
        },
    }


def _report_id(landmarks, changes):
    payload = {
        "landmarks": {key: {"status": value.get("Status"), "path": value.get("ActorPath"),
                            "transform": value.get("Transform")}
                      for key, value in landmarks.items() if key != "ADDITIONAL_SHELTERS"},
        "changes": [{"path": row["ActorPath"], "new": row["NewTransform"],
                     "validation": row["ValidationStatus"]} for row in changes],
    }
    return hashlib.sha256(json.dumps(payload, sort_keys=True).encode("utf-8")).hexdigest()[:20]


def _preflight(landmark_issues, changes, mapping_issues):
    blockers = []
    for issue in landmark_issues:
        if issue.get("Critical"):
            blockers.append("Critical landmark {} is {}: {}".format(
                issue.get("Landmark"), issue.get("Status"), issue.get("Reason")))
    if not changes:
        blockers.append("No quest actor transform proposals were produced.")
    for row in changes:
        if row["ValidationStatus"] == "Failed":
            blockers.append("{} failed physical validation: {}".format(
                row["ActorLabel"], "; ".join(row["ValidationIssues"])))
    # Unmapped quest actors are reported, but only gameplay spatial actors block APPLY.
    for issue in mapping_issues:
        if issue.get("Type") == "QuestActorMapping":
            blockers.append("Unmapped quest actor {}: {}".format(issue.get("ActorLabel"), issue.get("Reason")))
    # All route points require post-migration Recast/path review; they are concerns,
    # not claims of navigability. Unsupported/colliding points already block above.
    return blockers


def _build_report(mode):
    world, map_path = _assert_target_world()
    actors = _all_actors()
    protected_before = _protected_file_state()
    landmarks, landmark_issues = _resolve_landmarks(actors)
    inventory = _inventory(actors)
    changes, mapping_issues, nav_bounds = _plan(world, actors, landmarks, inventory)
    inventoried_summary, proposed_summary = _summaries(inventory, changes)
    blockers = _preflight(landmark_issues, changes, mapping_issues)
    report_id = _report_id(landmarks, changes)
    actual_navigation = _actual_navigation_review(world, actors, changes, landmarks, nav_bounds)
    validation_summary = {
        "ProposalCount": len(changes),
        "GroundingPassed": sum(1 for row in changes if row["GroundingStatus"] == "Passed"),
        "GroundingFailed": sum(1 for row in changes if row["GroundingStatus"] == "Failed"),
        "CollisionPassed": sum(1 for row in changes if row["CollisionStatus"] == "Passed"),
        "CollisionFailed": sum(1 for row in changes if row["CollisionStatus"] == "Failed"),
        "PhysicalValidationPassed": sum(1 for row in changes if row["PhysicalValidationStatus"] == "Passed"),
        "PhysicalValidationFailed": sum(1 for row in changes if row["PhysicalValidationStatus"] == "Failed"),
        "DoorwayOrStagingReviewRequired": sum(
            1 for row in changes if row["StagingReviewStatus"] == "ReviewRequired"),
        "NavigationReviewRequired": sum(
            1 for row in changes if row["NavigationStatus"] == "ReviewRequired"),
        "OutsideCurrentNavBounds": sum(
            1 for row in changes if not row["NavBoundsCoverage"]["Covered"]),
        "RuntimeSensitiveProtected": sum(
            1 for row in changes if row["RuntimeRelocationPreserved"]),
    }
    report = {
        "SchemaVersion": 2,
        "GeneratedAtLocal": _now(),
        "Mode": mode,
        "ReportId": report_id,
        "TargetMap": map_path,
        "ProtectedMap": PROTECTED_MAP,
        "ProtectedMapFileBefore": protected_before,
        "CurrentLevelAuthoritative": True,
        "ResolvedLandmarks": landmarks,
        "UnresolvedIssues": landmark_issues + mapping_issues,
        "QuestActorInventory": [{k: v for k, v in row.items() if k not in ("Actor", "SemanticText")}
                                for row in inventory],
        "ProposedChanges": changes,
        "InventoryCountsByQuestAndKind": inventoried_summary,
        "ProposedCountsByQuestAndKind": proposed_summary,
        "ValidationSummary": validation_summary,
        "ProposalStability": _proposal_stability(changes),
        "HouseStagingReview": _house_staging_review(changes),
        "RuntimeSensitiveReview": _runtime_sensitive_review(changes),
        "AfternoonTriggerValidation": _afternoon_trigger_validation(actors, changes, landmarks),
        "Navigation": {
            "NavMeshBoundsVolumes": [{"ActorLabel": row["ActorLabel"],
                                      "Origin": _v3(row["Origin"]), "Extent": _v3(row["Extent"])}
                                     for row in nav_bounds],
            "RecastDataRebuilt": False,
            "PathReachabilityClaimed": True,
            "ActualNavDataReview": actual_navigation,
            "CoverageAnalysis": _navigation_recommendations(landmarks, nav_bounds),
            "RequiredAfterMigration": [
                "Rebuild navigation manually after accepting placement.",
                "Inspect Q3, Q4, Q5, and Q6 start/mid/end routes in order.",
                "Run path tests after rebuild; this tool intentionally does not run PIE or rebuild Recast.",
            ],
        },
        "PreApplyReadiness": {
            "Status": "PASS" if not blockers and actual_navigation["ActualNavigationPass"] else "BLOCKED",
            "PhysicalPreflightPassed": not blockers,
            "ActualNavigationPassed": actual_navigation["ActualNavigationPass"],
            "AcceptedReportId": report_id,
            "CompatibleWithSavedLevelAfterNavChange": bool(
                not blockers and actual_navigation["ActualNavigationPass"]),
        },
        "Preflight": {"Passed": not blockers, "Blockers": blockers,
                      "ManualReviewCount": sum(1 for row in changes if row["ValidationStatus"] == "ManualReview")},
        "Apply": {"Attempted": False, "Succeeded": False, "Saved": False,
                  "ModifiedActorCount": 0, "RestoredAfterFailure": False},
        "Safety": {
            "EnvironmentTransformsModified": False,
            "ActorsCreatedOrDeleted": False,
            "QuestLogicModified": False,
            "ProtectedLevelLoadedOrSaved": False,
        },
    }
    return report, actors


def _report_path(report, suffix=""):
    if not os.path.isdir(REPORT_DIRECTORY):
        os.makedirs(REPORT_DIRECTORY)
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    tail = "_{}".format(suffix) if suffix else ""
    return os.path.join(REPORT_DIRECTORY, "village_quest_migration_{}_{}{}.json".format(
        report["Mode"].lower(), stamp, tail))


def _write_report(report, suffix=""):
    path = _report_path(report, suffix)
    with open(path, "w") as stream:
        json.dump(report, stream, indent=2, sort_keys=False)
    unreal.log("{} JSON report: {}".format(LOG, path))
    return path


def _same_transform(actual, expected, location_tolerance=0.5, rotation_tolerance=0.25, scale_tolerance=0.001):
    a_loc, e_loc = actual["Location"], expected["Location"]
    a_rot, e_rot = actual["Rotation"], expected["Rotation"]
    a_scale, e_scale = actual["Scale"], expected["Scale"]
    location_ok = math.sqrt(sum((a-b)**2 for a, b in zip(a_loc, e_loc))) <= location_tolerance
    rotation_ok = all(_angle_delta(a, b) <= rotation_tolerance for a, b in zip(a_rot, e_rot))
    scale_ok = all(abs(a-b) <= scale_tolerance for a, b in zip(a_scale, e_scale))
    return location_ok and rotation_ok and scale_ok


def _apply(report, actors, accepted_report_id):
    if accepted_report_id != report["ReportId"]:
        raise RuntimeError("APPLY requires the exact accepted REPORT_ONLY id {} (received {}).".format(
            report["ReportId"], accepted_report_id or "none"))
    if not report["Preflight"]["Passed"]:
        raise RuntimeError("Preflight failed; APPLY is forbidden. See report blockers.")
    manual = [row["ActorLabel"] for row in report["ProposedChanges"]
              if row["ValidationStatus"] == "ManualReview"]
    # An exact accepted report id is the designer's approval of non-blocking
    # doorway/staging review warnings.  They remain blocking unless the same
    # regenerated plan has passed the final physical/navigation readiness gate.
    if manual and report.get("PreApplyReadiness", {}).get("Status") != "PASS":
        raise RuntimeError("APPLY is blocked until ManualReview placements are accepted/resolved: {}".format(
            ", ".join(manual[:20]) + (" ..." if len(manual) > 20 else "")))

    actors_by_path = {_path(actor): actor for actor in actors}
    snapshots = {}
    environment = {path: _transform_json(actor) for path, actor in actors_by_path.items()
                   if not _is_relevant(actor)}
    for change in report["ProposedChanges"]:
        actor = actors_by_path.get(change["ActorPath"])
        if not _is_valid_object(actor):
            raise RuntimeError("Actor vanished before APPLY: {}".format(change["ActorPath"]))
        if _label(actor) != change["ActorLabel"] or _class_name(actor) != change["ActorClass"]:
            raise RuntimeError("Actor identity changed before APPLY: {}".format(change["ActorPath"]))
        snapshots[change["ActorPath"]] = _transform_json(actor)

    changed = []
    report["Apply"]["Attempted"] = True
    try:
        with unreal.ScopedEditorTransaction("VHV Quest 0-6 spatial migration"):
            for change in report["ProposedChanges"]:
                actor = actors_by_path[change["ActorPath"]]
                target = change["NewTransform"]
                actor.modify()
                actor.set_actor_location(unreal.Vector(*target["Location"]), False, True)
                actor.set_actor_rotation(_rotator_from_report(target["Rotation"]), False)
                # Scale is deliberately untouched.
                changed.append(actor)

        readback_failures = []
        for change in report["ProposedChanges"]:
            actor = actors_by_path.get(change["ActorPath"])
            if not _is_valid_object(actor):
                readback_failures.append("Missing after move: {}".format(change["ActorLabel"]))
                continue
            actual = _transform_json(actor)
            change["ReadBackTransform"] = actual
            if not _same_transform(actual, change["NewTransform"]):
                readback_failures.append("Transform mismatch: {}".format(change["ActorLabel"]))
        environment_changes = []
        for path, original in environment.items():
            actor = actors_by_path.get(path)
            if actor and not _same_transform(_transform_json(actor), original):
                environment_changes.append(_label(actor))
        if environment_changes:
            readback_failures.append("Environment actor transforms changed: {}".format(", ".join(environment_changes[:20])))
        if readback_failures:
            raise RuntimeError("; ".join(readback_failures))
        if _protected_file_state() != report["ProtectedMapFileBefore"]:
            raise RuntimeError("Protected level file state changed externally during APPLY; refusing to save.")
        world, current_path = _assert_target_world()
        if current_path != TARGET_MAP and not current_path.endswith("/Lvl_Village_Main"):
            raise RuntimeError("Current map changed before save: {}".format(current_path))
        if not unreal.EditorLevelLibrary.save_current_level():
            raise RuntimeError("Unreal failed to save Lvl_Village_Main.")
        report["Apply"].update({"Succeeded": True, "Saved": True,
                                "ModifiedActorCount": len(changed)})
        report["ProtectedMapFileAfter"] = _protected_file_state()
        report["Safety"]["ProtectedLevelLoadedOrSaved"] = False
    except Exception:
        restore_failures = []
        for path, snapshot in snapshots.items():
            actor = actors_by_path.get(path)
            if not _is_valid_object(actor):
                restore_failures.append("missing {}".format(path))
                continue
            try:
                actor.set_actor_location(unreal.Vector(*snapshot["Location"]), False, True)
                actor.set_actor_rotation(_rotator_from_report(snapshot["Rotation"]), False)
                actor.set_actor_scale3d(unreal.Vector(*snapshot["Scale"]))
            except Exception as exc:
                restore_failures.append("{}: {}".format(path, exc))
        report["Apply"].update({"Succeeded": False, "Saved": False,
                                "ModifiedActorCount": len(changed),
                                "RestoredAfterFailure": not restore_failures,
                                "RestoreFailures": restore_failures,
                                "Failure": traceback.format_exc()})
        _write_report(report, "failed_restored" if not restore_failures else "failed_restore_incomplete")
        raise


def run(mode=REPORT_ONLY, accepted_report_id=None):
    """Build a report, or apply the exact accepted clean report plan."""
    mode = str(mode).upper()
    if mode not in (REPORT_ONLY, APPLY):
        raise ValueError("mode must be REPORT_ONLY or APPLY")
    report, actors = _build_report(mode)
    if mode == REPORT_ONLY:
        report["ProtectedMapFileAfter"] = _protected_file_state()
        if report["ProtectedMapFileAfter"] != report["ProtectedMapFileBefore"]:
            report["UnresolvedIssues"].append({"Type": "Safety", "Critical": True,
                                                "Reason": "Protected map file state changed externally during report."})
            report["Preflight"]["Passed"] = False
            report["Preflight"]["Blockers"].append("Protected map file state changed during REPORT_ONLY.")
        path = _write_report(report)
        report["ReportPath"] = path
        unreal.log("{} REPORT_ONLY complete: {} landmarks resolved, {} actors inventoried, {} proposals, preflight={}.".format(
            LOG,
            sum(1 for key, value in report["ResolvedLandmarks"].items()
                if key != "ADDITIONAL_SHELTERS" and value.get("Status") == "Resolved"),
            len(report["QuestActorInventory"]), len(report["ProposedChanges"]),
            report["Preflight"]["Passed"]))
        return report
    try:
        _apply(report, actors, accepted_report_id)
    except Exception as exc:
        unreal.log_error("{} APPLY failed and was not saved: {}".format(LOG, exc))
        raise
    path = _write_report(report, "verified")
    report["ReportPath"] = path
    unreal.log("{} APPLY verified and Lvl_Village_Main saved. {} actors moved.".format(
        LOG, report["Apply"]["ModifiedActorCount"]))
    return report


def _command_line_options():
    mode = os.environ.get("VHV_QUEST_MIGRATION_MODE", REPORT_ONLY).upper()
    accepted = os.environ.get("VHV_QUEST_MIGRATION_ACCEPTED_REPORT_ID")
    for argument in sys.argv:
        if argument.startswith("--vhv-migration-mode="):
            mode = argument.split("=", 1)[1].upper()
        elif argument.startswith("--vhv-accept-report-id="):
            accepted = argument.split("=", 1)[1]
    return mode, accepted


if __name__ == "__main__":
    _mode, _accepted = _command_line_options()
    if _mode == DIAGNOSTIC_ONLY:
        diagnose_ground_traces()
    elif _mode == BLOCKER_DIAGNOSTIC:
        diagnose_physical_blockers()
    elif _mode == AFTERNOON_LOCAL_DIAGNOSTIC:
        diagnose_afternoon_trigger_locally()
    elif _mode == Q4_ROUTE_DIAGNOSTIC:
        diagnose_q4_routes()
    else:
        run(_mode, _accepted)
