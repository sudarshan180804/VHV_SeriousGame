"""Apply the SS1-SS17 actor-scoped visual replacements and SS8 burpee profile."""

import json
import os
import traceback

import unreal


MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
BACKUP_DIR = "/Game/VHV_Backups/TargetedNPC17_20261023"
BACKUP_MAP = BACKUP_DIR + "/Lvl_Village_Main_PreTargetedNPC17"
PROFILE_DIR = "/Game/Character/MixamoCommon/Profiles"
OUTPUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "Diagnostics", "targeted_npc17_result.json")


TARGETS = [
    ("SS1", "HBCT_Market_Motorbike", "prassart",
     "Motorbike observation actor in the Prologue market trigger."),
    ("SS2", "HBCT_GoalSetting_ObservationVHV", "nurse",
     "VHV slot in the initial health-post conversation with Auntie Nuan."),
    ("SS3", "HBCT_GoalSetting_RoadVillager", "ChaiWife",
     "GoalSettingRoadVillager participant used by RoadsidePractice."),
    ("SS4", "HBCT_RoleModel_FitVillager", "genericAdult",
     "Adult Model paired with HBCT_RoleModel_YoungObserver in ExerciseObservation."),
    ("SS5", "HBCT_RoleModel_Parent", "somchaiStanding",
     "Parent slot in the water-choice Family observation."),
    ("SS6", "HBCT_RoleModel_ListeningWoman", "vendor",
     "Listener slot in the SimilarExperience observation."),
    ("SS7", "HBCT_RoleModel_SymbolicObserver", "kid1",
     "Young Villager observer bound to the motivational-poster scene."),
    ("SS8", "HBCT_RoleModel_YoungAthlete", "youngAthleteBurpees",
     "YoungAthlete participant with actor-specific outside-dialogue Burpee loop."),
    ("SS9", "HBCT_RoleModel_RoadVillager", "Walkingpartner",
     "RoleModelRoadVillager participant asking whether a competitive runner is useful."),
    ("SS10", "HBCT_SS_NPC_ObservationWalker", "npcE",
     "Left/near-trigger Villager slot in the two-man roadside walking scene."),
    ("SS11", "HBCT_SS_NPC_EncouragingFriend", "athlete2",
     "Friend speaker of 'That's better than last week'."),
    ("SS12", "HBCT_SS_NPC_RoomInstrumentalPatient", "ChaiDaughter",
     "Male Patient in RoomInstrumental, paired with the vegetable-preparing Family slot."),
    ("SS13", "HBCT_SS_NPC_RoomAppraisalPatient", "Walkingpartner",
     "Patient speaker of 'I didn't realise I had improved that much'."),
    ("SS14", "HBCT_PR_NPC_PreferencePraise", "saeng",
     "NPC A / praise-preference villager at the pavilion preference sequence."),
    ("SS15", "HBCT_PR_NPC_PreferenceActivity", "vendor",
     "NPC B / fishing-and-quiet-time preference participant."),
    ("SS16", "HBCT_PR_NPC_PreferenceToken", "ChaiDaughter",
     "NPC C / points-and-milestone preference participant."),
    ("SS17", "HBCT_PR_NPC_DemoVillager", "prassart",
     "Villager bound to the O06 reinforcement-timing demonstration."),
]


CHARACTERS = {
    "prassart": dict(
        mesh="/Game/Character/prassart/CHAI",
        profile=PROFILE_DIR + "/DA_VHV_Prassart_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_ChaiPrasert",
        z=-96.0),
    "nurse": dict(
        mesh="/Game/Character/nurse/nurse",
        profile=PROFILE_DIR + "/DA_VHV_Nurse_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_Nurse",
        z=-96.0),
    "ChaiWife": dict(
        mesh="/Game/Character/ChaiWife/CHAIWIFE__1_",
        profile="/Game/Character/MixamoCommon/DA_VHV_ChaiWife_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_ChaiWife",
        z=-96.0),
    "genericAdult": dict(
        mesh="/Game/Character/npcB/renewed/NPCb",
        profile=PROFILE_DIR + "/DA_VHV_NPCB_Renewed_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_RenewedBC",
        z=-96.0),
    "somchaiStanding": dict(
        mesh="/Game/Character/somchai/SOMCHAI",
        profile=PROFILE_DIR + "/DA_VHV_Somchai_Standing_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_Somchai",
        z=-96.0),
    "vendor": dict(
        mesh="/Game/Character/vendor/VENDOR",
        profile=PROFILE_DIR + "/DA_VHV_Vendor_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_Vendor",
        z=-96.0),
    "kid1": dict(
        mesh="/Game/Character/kid1/KID1",
        profile=PROFILE_DIR + "/DA_VHV_Kid1_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_Kid",
        z=-70.0),
    "youngAthleteBurpees": dict(
        mesh="/Game/Character/athlete/ATHLETE",
        profile=PROFILE_DIR + "/DA_VHV_YoungAthlete_Burpees",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_Athlete",
        z=-96.0),
    "Walkingpartner": dict(
        mesh="/Game/Character/Walkingpartner/WALKING_PARTNER",
        profile=PROFILE_DIR + "/DA_VHV_WalkingPartner_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_WalkingPartner",
        z=-96.0),
    "npcE": dict(
        mesh="/Game/Character/npcE/WALKING_PARTNER",
        profile=PROFILE_DIR + "/DA_VHV_NPCE_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_NPCE",
        z=-96.0),
    "athlete2": dict(
        mesh="/Game/Character/athlete2/ATHLETE",
        profile=PROFILE_DIR + "/DA_VHV_Athlete2_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_Athlete",
        z=-96.0),
    "ChaiDaughter": dict(
        mesh="/Game/Character/ChaiDaughter/ChaiDaughter_Mixamo_Base",
        profile="/Game/Character/MixamoCommon/DA_VHV_ChaiDaughter_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC",
        z=-96.0),
    "saeng": dict(
        mesh="/Game/Character/saeng/Saeng",
        profile=PROFILE_DIR + "/DA_VHV_Saeng_Animations",
        abp="/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_Saeng",
        z=-96.0),
}


def path_name(value):
    return value.get_path_name() if value else ""


def load(path, expected=None):
    value = unreal.EditorAssetLibrary.load_asset(path)
    if not value or (expected and not isinstance(value, expected)):
        raise RuntimeError("Missing or wrong-class asset: " + path)
    return value


def vector(value):
    return [round(value.x, 4), round(value.y, 4), round(value.z, 4)]


def rotator(value):
    return [round(value.pitch, 4), round(value.yaw, 4), round(value.roll, 4)]


def transform(actor):
    return {
        "location": vector(actor.get_actor_location()),
        "rotation": rotator(actor.get_actor_rotation()),
        "scale": vector(actor.get_actor_scale3d()),
    }


def participant_id(actor):
    component = actor.get_quest_participant_component()
    return str(component.get_effective_participant_id()) if component else ""


def gameplay_identity(actor):
    participant = actor.get_quest_participant_component()
    dialogue = actor.get_dialogue_component()
    interaction = actor.get_npc_interaction_component()
    return {
        "class": path_name(actor.get_class()),
        "actor_tags": sorted(str(tag) for tag in actor.tags),
        "participant_id": participant_id(actor),
        "quest_participation_enabled": bool(
            participant.get_editor_property("quest_participation_enabled")) if participant else False,
        "default_conversation": path_name(
            dialogue.get_editor_property("default_conversation")) if dialogue else "",
        "interaction_enabled": bool(
            interaction.get_editor_property("interaction_enabled")) if interaction else False,
        "interaction_prompt": str(
            interaction.get_editor_property("default_interaction_prompt")) if interaction else "",
    }


def visual(actor):
    component = actor.get_editor_property("mesh")
    mesh = component.get_skeletal_mesh_asset()
    return {
        "mesh": path_name(mesh),
        "skeleton": path_name(mesh.get_editor_property("skeleton")) if mesh else "",
        "materials": [path_name(material) for material in component.get_materials()],
        "anim_class": path_name(component.get_editor_property("anim_class")),
        "profile": path_name(actor.get_animation_profile()),
        "mesh_relative_location": vector(component.get_editor_property("relative_location")),
        "mesh_relative_rotation": rotator(component.get_editor_property("relative_rotation")),
        "mesh_relative_scale": vector(component.get_editor_property("relative_scale3d")),
    }


def create_or_load_profile():
    path = PROFILE_DIR + "/DA_VHV_YoungAthlete_Burpees"
    profile = unreal.EditorAssetLibrary.load_asset(path)
    if not profile:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        profile = tools.create_asset(
            "DA_VHV_YoungAthlete_Burpees", PROFILE_DIR,
            unreal.VHVNPCAnimationProfile, unreal.DataAssetFactory())
    if not profile:
        raise RuntimeError("Could not create " + path)
    profile.modify()
    profile.set_editor_properties({
        "standing_idle_animation": load("/Game/Character/athlete/Breathing_Idle", unreal.AnimSequence),
        "walking_animation": load("/Game/Character/athlete/Walking", unreal.AnimSequence),
        "running_animation": None,
        "sitting_idle_animation": None,
        "special_action_animation": load("/Game/Character/athlete/Burpee", unreal.AnimSequence),
        "use_special_action_during_dialogue": False,
        "use_special_action_outside_dialogue": True,
        "loop_special_action": True,
        "special_action_playback_speed": 1.0,
        "idle_walk_threshold": 5.0,
        "walk_playback_speed": 1.0,
        "transition_blend_time": 0.25,
        "initial_posture": unreal.VHVNPCPosture.STANDING,
    })
    return profile


def prepare_assets():
    unreal.EditorAssetLibrary.make_directory(PROFILE_DIR)
    burpee_profile = create_or_load_profile()
    prepared = {}
    for key, spec in CHARACTERS.items():
        mesh = load(spec["mesh"], unreal.SkeletalMesh)
        profile = burpee_profile if key == "youngAthleteBurpees" else load(
            spec["profile"], unreal.VHVNPCAnimationProfile)
        abp = load(spec["abp"], unreal.AnimBlueprint)
        skeleton = mesh.get_editor_property("skeleton")
        if not profile.is_compatible_with_skeleton(skeleton):
            raise RuntimeError("Profile {} is incompatible with {}".format(
                path_name(profile), path_name(mesh)))
        if abp.get_editor_property("target_skeleton") != skeleton:
            raise RuntimeError("AnimBP {} targets the wrong skeleton for {}".format(
                path_name(abp), path_name(mesh)))
        prepared[key] = dict(
            mesh=mesh, profile=profile, abp=abp,
            anim_class=abp.generated_class(), z=spec["z"])
    return prepared


def find_npcs():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    npcs = [actor for actor in actors if isinstance(actor, unreal.VHVNPCCharacter)]
    labels = {}
    for actor in npcs:
        labels.setdefault(actor.get_actor_label(), []).append(actor)
    return npcs, labels


def configure_actor(actor, assets, activate_special=False):
    actor.modify()
    actor.set_animation_profile(None)
    component = actor.get_editor_property("mesh")
    component.modify()
    component.set_anim_instance_class(None)
    component.set_skinned_asset_and_update(assets["mesh"])
    component.set_editor_property("override_materials", [])
    component.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
    component.set_anim_instance_class(assets["anim_class"])
    component.set_relative_location(unreal.Vector(0.0, 0.0, assets["z"]), False, False)
    component.set_relative_rotation(
        unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0), False, False)
    component.set_relative_scale3d(unreal.Vector(1.0, 1.0, 1.0))
    actor.set_animation_profile(assets["profile"])
    actor.set_npc_posture(unreal.VHVNPCPosture.STANDING)
    actor.set_special_animation_action_active(activate_special)


def run():
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    unreal.EditorAssetLibrary.make_directory(BACKUP_DIR)
    backup_status = "existing"
    if not unreal.EditorAssetLibrary.does_asset_exist(BACKUP_MAP):
        if not unreal.EditorAssetLibrary.duplicate_asset(MAP_PATH, BACKUP_MAP):
            raise RuntimeError("Could not create map backup")
        backup_status = "created"

    if not unreal.EditorLoadingAndSavingUtils.load_map(BACKUP_MAP):
        raise RuntimeError("Could not load backup map")
    _, baseline_labels = find_npcs()
    required = [label for _, label, _, _ in TARGETS]
    missing_baseline = [label for label in required if len(baseline_labels.get(label, [])) != 1]
    if missing_baseline:
        raise RuntimeError("Missing/duplicate backup targets: " + ", ".join(missing_baseline))
    baseline = {
        label: {
            "transform": transform(baseline_labels[label][0]),
            "identity": gameplay_identity(baseline_labels[label][0]),
            "visual": visual(baseline_labels[label][0]),
        } for label in required
    }

    prepared = prepare_assets()
    for assets in prepared.values():
        unreal.BlueprintEditorLibrary.compile_blueprint(assets["abp"])
        if not assets["abp"].generated_class():
            raise RuntimeError("AnimBP failed to compile: " + path_name(assets["abp"]))

    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load production map")
    all_npcs, labels = find_npcs()
    missing = [label for label in required if len(labels.get(label, [])) != 1]
    if missing:
        raise RuntimeError("Missing/duplicate production targets: " + ", ".join(missing))

    player_or_instructor_before = {
        actor.get_actor_label(): (transform(actor), visual(actor), gameplay_identity(actor))
        for actor in all_npcs if participant_id(actor) == "Instructor"
    }
    actor_count_before = len(all_npcs)
    before_all_transforms = {actor.get_actor_label(): transform(actor) for actor in all_npcs}
    rows = []
    for ss, label, character, basis in TARGETS:
        actor = labels[label][0]
        before = baseline[label]
        configure_actor(actor, prepared[character], ss == "SS8")
        after_visual = visual(actor)
        if after_visual["mesh"] != path_name(prepared[character]["mesh"]):
            raise RuntimeError(ss + " mesh assignment failed")
        if after_visual["profile"] != path_name(prepared[character]["profile"]):
            raise RuntimeError(ss + " profile assignment failed")
        if after_visual["anim_class"] != path_name(prepared[character]["anim_class"]):
            raise RuntimeError(ss + " AnimBP assignment failed")
        if transform(actor) != before["transform"]:
            raise RuntimeError(ss + " actor world transform changed")
        if gameplay_identity(actor) != before["identity"]:
            raise RuntimeError(ss + " gameplay identity changed")
        rows.append({
            "screenshot": ss,
            "actor": label,
            "participant_id": participant_id(actor),
            "resolution_basis": basis,
            "before": before["visual"],
            "after": after_visual,
            "world_transform": transform(actor),
            "validated": True,
        })

    all_npcs_after, labels_after = find_npcs()
    if len(all_npcs_after) != actor_count_before:
        raise RuntimeError("NPC actor count changed")
    for actor in all_npcs_after:
        label = actor.get_actor_label()
        if before_all_transforms.get(label) != transform(actor):
            raise RuntimeError("Unexpected actor transform change: " + label)

    player_or_instructor_after = {
        actor.get_actor_label(): (transform(actor), visual(actor), gameplay_identity(actor))
        for actor in all_npcs_after if participant_id(actor) == "Instructor"
    }
    if player_or_instructor_before != player_or_instructor_after:
        raise RuntimeError("Instructor changed unexpectedly")

    profile = prepared["youngAthleteBurpees"]["profile"]
    ss8_checks = {
        "burpee": path_name(profile.get_editor_property("special_action_animation")),
        "standing_idle": path_name(profile.get_editor_property("standing_idle_animation")),
        "loop_special_action": bool(profile.get_editor_property("loop_special_action")),
        "outside_dialogue": bool(profile.get_editor_property("use_special_action_outside_dialogue")),
        "during_dialogue": bool(profile.get_editor_property("use_special_action_during_dialogue")),
        "running_animation": path_name(profile.get_editor_property("running_animation")),
    }
    if ss8_checks != {
            "burpee": "/Game/Character/athlete/Burpee.Burpee",
            "standing_idle": "/Game/Character/athlete/Breathing_Idle.Breathing_Idle",
            "loop_special_action": True,
            "outside_dialogue": True,
            "during_dialogue": False,
            "running_animation": ""}:
        raise RuntimeError("SS8 burpee profile readback failed: " + str(ss8_checks))

    assets_to_save = {PROFILE_DIR + "/DA_VHV_YoungAthlete_Burpees"}
    assets_to_save.update(path_name(values["abp"]).split(".", 1)[0] for values in prepared.values())
    for asset_path in sorted(assets_to_save):
        if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
            raise RuntimeError("Failed to save " + asset_path)
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Failed to save production map")

    report = {
        "map": MAP_PATH,
        "backup_map": BACKUP_MAP,
        "backup_status": backup_status,
        "target_count": len(rows),
        "actor_count_preserved": len(all_npcs_after),
        "rows": rows,
        "ss8_profile": path_name(profile),
        "ss8_checks": ss8_checks,
        "compiled_anim_blueprints": sorted(set(path_name(v["abp"]) for v in prepared.values())),
        "validation": {
            "all_target_world_transforms_preserved": True,
            "all_target_gameplay_identities_preserved": True,
            "all_level_npc_world_transforms_preserved": True,
            "npc_actor_count_preserved": True,
            "instructor_unchanged": True,
            "player_not_modified": True,
            "running_animations_assigned": False,
            "actors_created_deleted_or_renamed": False,
        },
    }
    with open(OUTPUT_PATH, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
    unreal.log_warning("VHV_TARGETED_NPC17=" + OUTPUT_PATH)
    unreal.log_warning("VHV_TARGETED_NPC17_COMPLETE targets=17")
    return report


try:
    run()
except Exception:
    unreal.log_error(traceback.format_exc())
    raise
