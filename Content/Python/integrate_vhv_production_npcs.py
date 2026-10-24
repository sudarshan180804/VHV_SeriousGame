"""Production HBCT NPC visual/animation integration for Lvl_Village_Main.

This script is intentionally label-driven and refuses to create, delete, move, rotate,
or scale level actors. It creates skeleton-specific AnimBP facades and lightweight
per-character profiles, then changes only the visual/animation configuration of the
existing AVHVNPCCharacter instances (plus child capsule/mesh alignment).
"""

import json
import os
import traceback

import unreal


MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
COMMON = "/Game/Character/MixamoCommon"
PROFILE_DIR = COMMON + "/Profiles"
OUTPUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "Diagnostics", "vhv_npc_integration_result.json")
PROTECTED_PARTICIPANTS = {"Instructor"}


CHARACTERS = {
    "chai": dict(mesh="/Game/Character/chai/CHAI", idle="/Game/Character/chai/Idle",
                 walk="/Game/Character/chai/Walking", sit="/Game/Character/chai/Sitting_Idle",
                 profile="DA_VHV_Chai_Animations", abp="ABP_VHV_Mixamo_NPC_ChaiPrasert"),
    "prassart": dict(mesh="/Game/Character/prassart/CHAI", idle="/Game/Character/prassart/Idle",
                     walk="/Game/Character/prassart/Walking", sit="/Game/Character/prassart/Sitting_Idle",
                     profile="DA_VHV_Prassart_Animations", abp="ABP_VHV_Mixamo_NPC_ChaiPrasert"),
    "ChaiWife": dict(mesh="/Game/Character/ChaiWife/CHAIWIFE__1_",
                     profile_path=COMMON + "/DA_VHV_ChaiWife_Animations",
                     abp_path=COMMON + "/ABP_VHV_Mixamo_NPC_ChaiWife"),
    "npcA": dict(mesh="/Game/Character/npcA/CHAIWIFE__1_", idle="/Game/Character/npcA/Standing_Idle",
                 walk="/Game/Character/npcA/Walking", sit="/Game/Character/npcA/Sitting_Idle",
                 profile="DA_VHV_NPCA_Animations", abp_path=COMMON + "/ABP_VHV_Mixamo_NPC_ChaiWife"),
    "ChaiDaughter": dict(mesh="/Game/Character/ChaiDaughter/ChaiDaughter_Mixamo_Base",
                         profile_path=COMMON + "/DA_VHV_ChaiDaughter_Animations",
                         abp_path=COMMON + "/ABP_VHV_Mixamo_NPC"),
    "npcD": dict(mesh="/Game/Character/npcD/ChaiDaughter_Mixamo_Base",
                 idle="/Game/Character/npcD/Standing_Idle", walk="/Game/Character/npcD/Walk",
                 sit="/Game/Character/npcD/Sitting_Idle", profile="DA_VHV_NPCD_Animations",
                 abp_path=COMMON + "/ABP_VHV_Mixamo_NPC"),
    "athlete": dict(mesh="/Game/Character/athlete/ATHLETE", idle="/Game/Character/athlete/Breathing_Idle",
                    walk="/Game/Character/athlete/Walking", special="/Game/Character/athlete/Burpee",
                    dialogue_action=True, loop_action=False, profile="DA_VHV_Athlete_Animations",
                    abp="ABP_VHV_Mixamo_NPC_Athlete"),
    "athlete2": dict(mesh="/Game/Character/athlete2/ATHLETE", idle="/Game/Character/athlete2/Breathing_Idle",
                     walk="/Game/Character/athlete2/Walking", special="/Game/Character/athlete2/Burpee",
                     dialogue_action=False, loop_action=False, profile="DA_VHV_Athlete2_Animations",
                     abp="ABP_VHV_Mixamo_NPC_Athlete"),
    "athlete2Workout": dict(mesh="/Game/Character/athlete2/ATHLETE",
                            idle="/Game/Character/athlete2/Breathing_Idle",
                            walk="/Game/Character/athlete2/Walking",
                            special="/Game/Character/athlete2/Burpee",
                            dialogue_action=True, loop_action=False,
                            profile="DA_VHV_Athlete2_Workout_Animations",
                            abp="ABP_VHV_Mixamo_NPC_Athlete"),
    "kid1": dict(mesh="/Game/Character/kid1/KID1", idle="/Game/Character/kid1/Breathing_Idle",
                 walk="/Game/Character/kid1/Walking", sit="/Game/Character/kid1/Sitting_Idle",
                 profile="DA_VHV_Kid1_Animations", abp="ABP_VHV_Mixamo_NPC_Kid"),
    "kid2": dict(mesh="/Game/Character/kid2/KID1", idle="/Game/Character/kid2/Breathing_Idle",
                 walk="/Game/Character/kid2/Walking", sit="/Game/Character/kid2/Sitting_Idle",
                 profile="DA_VHV_Kid2_Animations", abp="ABP_VHV_Mixamo_NPC_Kid"),
    "npcB": dict(mesh="/Game/Character/npcB/NPCc", idle="/Game/Character/npcB/Breathing_Idle",
                 walk="/Game/Character/npcB/Walking", sit="/Game/Character/npcB/Sitting_Idle",
                 profile="DA_VHV_NPCB_Animations", abp="ABP_VHV_Mixamo_NPC_GeneralMale"),
    "npcC": dict(mesh="/Game/Character/npcC/NPCc", idle="/Game/Character/npcC/Breathing_Idle",
                 walk="/Game/Character/npcC/Walking", sit="/Game/Character/npcC/Sitting_Idle",
                 profile="DA_VHV_NPCC_Animations", abp="ABP_VHV_Mixamo_NPC_GeneralMale"),
    "nurse": dict(mesh="/Game/Character/nurse/nurse", idle="/Game/Character/nurse/Standing_Idle",
                  walk="/Game/Character/nurse/Walk", sit="/Game/Character/nurse/Sitting",
                  profile="DA_VHV_Nurse_Animations", abp="ABP_VHV_Mixamo_NPC_Nurse"),
    "saeng": dict(mesh="/Game/Character/saeng/Saeng", idle="/Game/Character/saeng/Breathing_Idle",
                  walk="/Game/Character/saeng/Walk", sit="/Game/Character/saeng/Sitting_Idle",
                  profile="DA_VHV_Saeng_Animations", abp="ABP_VHV_Mixamo_NPC_Saeng"),
    "somchai": dict(mesh="/Game/Character/somchai/SOMCHAI", idle="/Game/Character/somchai/Breathing_Idle_",
                    sit="/Game/Character/somchai/Sitting_Rubbing_Arm", initial_sitting=True,
                    profile="DA_VHV_Somchai_Animations", abp="ABP_VHV_Mixamo_NPC_Somchai"),
    "vendor": dict(mesh="/Game/Character/vendor/VENDOR", idle="/Game/Character/vendor/Standing_Idle",
                   walk="/Game/Character/vendor/Walk", special="/Game/Character/vendor/Talking",
                   dialogue_action=True, loop_action=True, profile="DA_VHV_Vendor_Animations",
                   abp="ABP_VHV_Mixamo_NPC_Vendor"),
    "villegeHead": dict(mesh="/Game/Character/villegeHead/villegeHead",
                        idle="/Game/Character/villegeHead/Breathing_Idle",
                        walk="/Game/Character/villegeHead/Walking",
                        sit="/Game/Character/villegeHead/Sitting_Idle",
                        profile="DA_VHV_VillegeHead_Animations", abp="ABP_VHV_Mixamo_NPC_VillegeHead"),
    "Walkingpartner": dict(mesh="/Game/Character/Walkingpartner/WALKING_PARTNER",
                           idle="/Game/Character/Walkingpartner/Breathing_Idle",
                           walk="/Game/Character/Walkingpartner/Walking",
                           profile="DA_VHV_WalkingPartner_Animations",
                           abp="ABP_VHV_Mixamo_NPC_WalkingPartner"),
}


CAST = {
    # Q0 Prologue market observations
    "HBCT_Market_JunkFoodVillager": "npcB", "HBCT_Market_Motorbike": "athlete2",
    "HBCT_Market_Smoker": "npcC", "HBCT_Market_SugarDrinkShopper": "npcD",
    "HBCT_Market_SweetTeaVillager": "npcA", "HBCT_Market_Vendor": "vendor",
    # Q1 Motivation
    "HBCT_Motivation_DirectivePatient": "npcC", "HBCT_Motivation_DirectiveVHV": "npcB",
    "HBCT_Motivation_Mali": "npcA", "HBCT_Motivation_MotivationalPatient": "npcD",
    "HBCT_Motivation_MotivationalVHV": "nurse", "HBCT_Motivation_Prasert": "prassart",
    "HBCT_Motivation_Saeng": "saeng", "HBCT_Motivation_SaengVHV": "npcB",
    # Q2 Goal Setting (the two Nuan actors deliberately share one appearance)
    "HBCT_GoalSetting_AuntieNuan": "npcD", "HBCT_GoalSetting_Nuan_ClearGoal": "npcD",
    "HBCT_GoalSetting_JointPatient": "npcC", "HBCT_GoalSetting_JointVHV": "nurse",
    "HBCT_GoalSetting_ObservationVHV": "npcB", "HBCT_GoalSetting_RoadVillager": "npcC",
    "HBCT_GoalSetting_SelfSetPatient": "npcB", "HBCT_GoalSetting_SelfSetVHV": "nurse",
    "HBCT_GoalSetting_UncleChai": "chai",
    # Q3 Role Model
    "HBCT_RoleModel_ExperienceWoman": "npcA", "HBCT_RoleModel_FitVillager": "athlete2Workout",
    "HBCT_RoleModel_ListeningWoman": "npcD", "HBCT_RoleModel_Parent": "npcB",
    "HBCT_RoleModel_RoadVillager": "npcC", "HBCT_RoleModel_SymbolicCandidateObserver": "kid1",
    "HBCT_RoleModel_SymbolicObserver": "npcB", "HBCT_RoleModel_TimelineBefore_Target": "npcC",
    "HBCT_RoleModel_TimelineFindsModel_Model": "athlete2",
    "HBCT_RoleModel_TimelineFindsModel_Target": "npcC",
    "HBCT_RoleModel_TimelineFirstAttempt_Target": "npcC",
    "HBCT_RoleModel_TimelineNewRoutine_Target": "npcC",
    "HBCT_RoleModel_TimelineRepetition_Model": "athlete2",
    "HBCT_RoleModel_TimelineRepetition_Target": "npcC",
    "HBCT_RoleModel_UncleSomchai": "somchai", "HBCT_RoleModel_YoungAthlete": "athlete",
    "HBCT_RoleModel_YoungerFamilyMember": "kid2", "HBCT_RoleModel_YoungObserver": "kid1",
    # Q4 Social Support
    "HBCT_SS_NPC_ChaiClinicNurse": "nurse", "HBCT_SS_NPC_ChaiDaughter": "ChaiDaughter",
    "HBCT_SS_NPC_ChaiWalkingNeighbor": "Walkingpartner", "HBCT_SS_NPC_ChaiWife": "ChaiWife",
    "HBCT_SS_NPC_EncouragingFriend": "npcD", "HBCT_SS_NPC_GuidanceNurse": "nurse",
    "HBCT_SS_NPC_GuidancePatient": "npcB", "HBCT_SS_NPC_MealPatient": "npcC",
    "HBCT_SS_NPC_MealSpouse": "npcA", "HBCT_SS_NPC_ObservationWalker": "npcB",
    "HBCT_SS_NPC_ObservationWalkingPartner": "npcC",
    "HBCT_SS_NPC_RoomAppraisalPatient": "npcC", "HBCT_SS_NPC_RoomAppraisalVHV": "nurse",
    "HBCT_SS_NPC_RoomEmotionalFamily": "npcD", "HBCT_SS_NPC_RoomEmotionalPatient": "npcB",
    "HBCT_SS_NPC_RoomInformationalPatient": "npcC", "HBCT_SS_NPC_RoomInformationalVHV": "nurse",
    "HBCT_SS_NPC_RoomInstrumentalFamily": "npcA", "HBCT_SS_NPC_RoomInstrumentalPatient": "npcB",
    "HBCT_SS_NPC_TiredVillager": "npcC",
    # Q5 Self-Monitoring
    "HBCT_SM_NPC_SelfMonitoringFriend": "npcB",
    # Q6 Positive Reinforcement
    "HBCT_PR_NPC_SuccessWalker": "athlete2", "HBCT_PR_NPC_SuccessWalkerFriend": "npcD",
    "HBCT_PR_NPC_WaterChild": "kid1", "HBCT_PR_NPC_WaterParent": "npcA",
    "HBCT_PR_NPC_ActivityVillager": "npcD", "HBCT_PR_NPC_FeedbackVillager": "npcB",
    "HBCT_PR_NPC_HealthFairExerciseVillager": "athlete2Workout", "HBCT_PR_NPC_HealthFairVHV": "nurse",
    "HBCT_PR_NPC_MaterialVillager": "npcC", "HBCT_PR_NPC_TokenVillager": "npcB",
    "HBCT_PR_NPC_PreferenceActivity": "npcC", "HBCT_PR_NPC_PreferencePraise": "npcA",
    "HBCT_PR_NPC_PreferenceToken": "npcB", "HBCT_PR_NPC_DemoVillager": "npcD",
    "HBCT_PR_NPC_VillageHead": "villegeHead", "HBCT_PR_NPC_HealthNurse": "nurse",
}


def load(path, expected=None):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset or (expected and not isinstance(asset, expected)):
        raise RuntimeError("Missing or incorrect asset type: " + path)
    return asset


def path_name(value):
    return value.get_path_name() if value else ""


def snapshot(actor):
    return (actor.get_actor_location(), actor.get_actor_rotation(), actor.get_actor_scale3d())


def same_vector(a, b, tolerance=0.001):
    return abs(a.x-b.x) <= tolerance and abs(a.y-b.y) <= tolerance and abs(a.z-b.z) <= tolerance


def same_rotator(a, b, tolerance=0.001):
    return abs(a.pitch-b.pitch) <= tolerance and abs(a.yaw-b.yaw) <= tolerance and abs(a.roll-b.roll) <= tolerance


def same_transform(a, b):
    return same_vector(a[0], b[0]) and same_rotator(a[1], b[1]) and same_vector(a[2], b[2])


def actor_participant_id(actor):
    if not hasattr(actor, "get_quest_participant_component"):
        return ""
    component = actor.get_quest_participant_component()
    return str(component.get_effective_participant_id()) if component else ""


def ensure_anim_blueprint(name, skeleton):
    asset_path = COMMON + "/" + name
    existing = unreal.EditorAssetLibrary.load_asset(asset_path)
    if existing:
        if not isinstance(existing, unreal.AnimBlueprint):
            raise RuntimeError(asset_path + " exists but is not an AnimBlueprint")
        if existing.get_editor_property("target_skeleton") != skeleton:
            raise RuntimeError(asset_path + " targets the wrong skeleton")
        return existing
    factory = unreal.AnimBlueprintFactory()
    factory.set_editor_properties({
        "target_skeleton": skeleton,
        "parent_class": unreal.VHVMixamoNPCAnimInstance.static_class(),
    })
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, COMMON, unreal.AnimBlueprint, factory)
    if not asset:
        raise RuntimeError("Could not create " + asset_path)
    if asset.get_editor_property("target_skeleton") != skeleton:
        raise RuntimeError("Created AnimBP has the wrong skeleton: " + asset_path)
    return asset


def ensure_profile(key, spec, mesh):
    if spec.get("profile_path"):
        profile = load(spec["profile_path"], unreal.VHVNPCAnimationProfile)
        if not profile.is_compatible_with_skeleton(mesh.get_editor_property("skeleton")):
            raise RuntimeError("Existing profile is incompatible: " + spec["profile_path"])
        return profile
    asset_path = PROFILE_DIR + "/" + spec["profile"]
    profile = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not profile:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VHVNPCAnimationProfile)
        profile = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            spec["profile"], PROFILE_DIR, unreal.VHVNPCAnimationProfile, factory)
    if not profile:
        raise RuntimeError("Could not create profile " + asset_path)
    properties = {
        "standing_idle_animation": load(spec["idle"], unreal.AnimSequence),
        "walking_animation": load(spec["walk"], unreal.AnimSequence) if spec.get("walk") else None,
        "sitting_idle_animation": load(spec["sit"], unreal.AnimSequence) if spec.get("sit") else None,
        "special_action_animation": load(spec["special"], unreal.AnimSequence) if spec.get("special") else None,
        "use_special_action_during_dialogue": bool(spec.get("dialogue_action", False)),
        "loop_special_action": bool(spec.get("loop_action", False)),
        "special_action_playback_speed": 1.0,
        "idle_walk_threshold": 5.0,
        "walk_playback_speed": 1.0,
        "transition_blend_time": 0.2,
        "initial_posture": unreal.VHVNPCPosture.SITTING if spec.get("initial_sitting") else unreal.VHVNPCPosture.STANDING,
    }
    profile.set_editor_properties(properties)
    skeleton = mesh.get_editor_property("skeleton")
    if not profile.has_all_required_animations() or not profile.is_compatible_with_skeleton(skeleton):
        raise RuntimeError("Profile validation failed: " + asset_path)
    return profile


def prepare_assets():
    unreal.EditorAssetLibrary.make_directory(PROFILE_DIR)
    prepared = {}
    for key, spec in CHARACTERS.items():
        mesh = load(spec["mesh"], unreal.SkeletalMesh)
        skeleton = mesh.get_editor_property("skeleton")
        if not skeleton:
            raise RuntimeError("Mesh has no skeleton: " + spec["mesh"])
        profile = ensure_profile(key, spec, mesh)
        if spec.get("abp_path"):
            abp = load(spec["abp_path"], unreal.AnimBlueprint)
            if abp.get_editor_property("target_skeleton") != skeleton:
                raise RuntimeError("Existing AnimBP incompatible with " + spec["mesh"])
        else:
            abp = ensure_anim_blueprint(spec["abp"], skeleton)
        generated_class = abp.generated_class()
        if not generated_class:
            raise RuntimeError("AnimBP has no generated class: " + path_name(abp))
        prepared[key] = dict(mesh=mesh, profile=profile, abp=abp, anim_class=generated_class)
    return prepared


def run():
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load " + MAP_PATH)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_npcs = [actor for actor in subsystem.get_all_level_actors()
                if hasattr(actor, "get_quest_participant_component")]
    labels = {actor.get_actor_label(): actor for actor in all_npcs}
    duplicates = [label for label in CAST if sum(1 for actor in all_npcs if actor.get_actor_label() == label) != 1]
    if duplicates:
        raise RuntimeError("Missing or duplicate cast labels: " + ", ".join(duplicates))
    eligible = [actor for actor in all_npcs if actor_participant_id(actor) not in PROTECTED_PARTICIPANTS]
    unmapped = sorted(actor.get_actor_label() for actor in eligible if actor.get_actor_label() not in CAST)
    extras = sorted(label for label in CAST if label not in labels)
    if unmapped or extras or len(CAST) != len(eligible):
        raise RuntimeError("Cast coverage failure: eligible={} cast={} unmapped={} extras={}".format(
            len(eligible), len(CAST), unmapped, extras))

    before = {actor.get_actor_label(): snapshot(actor) for actor in all_npcs}
    instructor = next(actor for actor in all_npcs if actor_participant_id(actor) == "Instructor")
    instructor_visual = (path_name(instructor.get_editor_property("mesh").get_skeletal_mesh_asset()),
                         path_name(instructor.get_animation_profile()))
    gameplay = {actor.get_actor_label(): (
        actor.get_quest_participant_component(), actor.get_dialogue_component(),
        actor.get_behavior_component(), actor.get_npc_interaction_component(),
        actor.get_patrol_component(), actor.get_quest_command_component()) for actor in eligible}

    prepared = prepare_assets()
    changed = []
    for label, key in CAST.items():
        actor = labels[label]
        assets = prepared[key]
        actor.modify()
        component = actor.get_editor_property("mesh")
        component.modify()
        component.set_skinned_asset_and_update(assets["mesh"])
        component.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
        component.set_anim_instance_class(assets["anim_class"])
        actor.set_animation_profile(assets["profile"])
        posture = unreal.VHVNPCPosture.SITTING if CHARACTERS[key].get("initial_sitting") else unreal.VHVNPCPosture.STANDING
        actor.set_npc_posture(posture)
        actor.set_special_animation_action_active(False)

        is_child = key in ("kid1", "kid2")
        mesh_z = -70.0 if is_child else -96.0
        component.set_relative_location(unreal.Vector(0.0, 0.0, mesh_z), False, False)
        # Unreal's Python Rotator positional constructor is Roll, Pitch, Yaw.
        # The relative transform is computed explicitly so the Mixamo -90 yaw
        # correction is never misinterpreted as a -90 pitch.
        component.set_relative_rotation(
            unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0), False, False)
        component.set_relative_scale3d(unreal.Vector(1.0, 1.0, 1.0))
        if is_child:
            capsule = actor.get_editor_property("capsule_component")
            capsule.modify()
            capsule.set_capsule_size(30.0, 70.0, True)
        changed.append(label)

    failures = []
    actor_rows = []
    for label, key in CAST.items():
        actor = labels[label]
        assets = prepared[key]
        component = actor.get_editor_property("mesh")
        mesh = component.get_skeletal_mesh_asset()
        profile = actor.get_animation_profile()
        if not same_transform(before[label], snapshot(actor)):
            failures.append(label + ": actor transform changed")
        if gameplay[label] != (
                actor.get_quest_participant_component(), actor.get_dialogue_component(),
                actor.get_behavior_component(), actor.get_npc_interaction_component(),
                actor.get_patrol_component(), actor.get_quest_command_component()):
            failures.append(label + ": gameplay component identity changed")
        if mesh != assets["mesh"]:
            failures.append(label + ": mesh mismatch")
        if profile != assets["profile"] or not profile.is_compatible_with_skeleton(mesh.get_editor_property("skeleton")):
            failures.append(label + ": profile mismatch/incompatibility")
        if component.get_editor_property("anim_class") != assets["anim_class"]:
            failures.append(label + ": AnimBP mismatch")
        expected_materials = [slot.material_interface for slot in mesh.get_editor_property("materials")]
        if list(component.get_materials()) != expected_materials:
            failures.append(label + ": per-mesh material variation was not preserved")
        actor_rows.append({
            "label": label, "participant_id": actor_participant_id(actor), "character": key,
            "mesh": path_name(mesh), "materials": [path_name(value) for value in component.get_materials()],
            "skeleton": path_name(mesh.get_editor_property("skeleton")),
            "anim_blueprint": path_name(assets["abp"]), "profile": path_name(profile),
            "posture": str(actor.get_npc_posture()), "transform_preserved": same_transform(before[label], snapshot(actor)),
        })

    if not same_transform(before[instructor.get_actor_label()], snapshot(instructor)):
        failures.append("Instructor transform changed")
    if instructor_visual != (path_name(instructor.get_editor_property("mesh").get_skeletal_mesh_asset()),
                             path_name(instructor.get_animation_profile())):
        failures.append("Instructor visual/animation configuration changed")
    if failures:
        raise RuntimeError("Pre-save validation failed: " + "; ".join(failures))

    assets_to_save = set()
    for values in prepared.values():
        assets_to_save.add(path_name(values["profile"]).split(".", 1)[0])
        assets_to_save.add(path_name(values["abp"]).split(".", 1)[0])
    for asset_path in sorted(assets_to_save):
        if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
            raise RuntimeError("Failed to save asset " + asset_path)
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Failed to save Lvl_Village_Main")

    profiles = []
    seen_profiles = set()
    for key, values in prepared.items():
        profile = values["profile"]
        profile_path = path_name(profile)
        if profile_path in seen_profiles:
            continue
        seen_profiles.add(profile_path)
        profiles.append({
            "character": key, "profile": profile_path,
            "idle": path_name(profile.get_editor_property("standing_idle_animation")),
            "walk": path_name(profile.get_editor_property("walking_animation")),
            "sit": path_name(profile.get_editor_property("sitting_idle_animation")),
            "special": path_name(profile.get_editor_property("special_action_animation")),
            "dialogue_action": bool(profile.get_editor_property("use_special_action_during_dialogue")),
            "loop_special": bool(profile.get_editor_property("loop_special_action")),
        })
    report = {
        "map": MAP_PATH, "eligible_actor_count": len(eligible), "modified_actor_count": len(changed),
        "protected": [{"label": instructor.get_actor_label(), "participant_id": "Instructor",
                       "unchanged": True}],
        "actors": sorted(actor_rows, key=lambda row: row["label"]),
        "profiles": sorted(profiles, key=lambda row: row["character"].lower()),
        "anim_blueprints": sorted(set(path_name(values["abp"]) for values in prepared.values())),
        "known_clip_gaps": [
            {"character": "somchai", "missing": ["walking"],
             "behavior": "Standing idle fallback while moved; Sitting Rubbing Arm is used only in seated posture."},
            {"character": "Walkingpartner", "missing": ["sitting"],
             "behavior": "Standing/walking only; no foreign-skeleton sitting clip is assigned."},
            {"character": "vendor", "missing": ["sitting"],
             "behavior": "Talking is activated only while the vendor is the current authored speaker."},
            {"character": "athlete", "missing": ["sitting"],
             "behavior": "Burpee is a non-looping speaker-context action, never a locomotion fallback."},
            {"character": "athlete2", "missing": ["sitting"],
             "behavior": "Burpee is disabled in the general profile and enabled non-looping only in the fit/workout scene profile."},
        ],
        "validation": {
            "all_actor_transforms_preserved": True, "gameplay_components_preserved": True,
            "all_profiles_skeleton_compatible": True, "per_mesh_materials_preserved": True,
            "instructor_unchanged": True, "running_animations_used": False,
            "actors_created_or_deleted": False,
        },
    }
    with open(OUTPUT_PATH, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
    unreal.log_warning("VHV_NPC_INTEGRATION=" + OUTPUT_PATH)
    unreal.log_warning("VHV_NPC_INTEGRATION_COMPLETE modified={} profiles={} animbps={}".format(
        len(changed), len(profiles), len(report["anim_blueprints"])))
    return report


try:
    run()
except Exception:
    unreal.log_error(traceback.format_exc())
    raise
