"""Idempotent production character replacement for Lvl_Village_Main.

Only visual/animation configuration and mesh-component-relative alignment are
changed. Actor transforms, gameplay component identities, quest data, dialogue
assets, triggers, tags, and navigation destinations are verified unchanged.
"""

import json
import os
import traceback

import unreal


MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
PLAYER_BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
COMMON = "/Game/Character/MixamoCommon"
PROFILE_DIR = COMMON + "/Profiles"
BACKUP_DIR = "/Game/VHV_Backups/CharacterReplacement_20261023"
OUTPUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "Diagnostics", "character_replacement_result.json")


SPECS = {
    "player": dict(
        mesh="/Game/Character/Player/main1",
        idle="/Game/Character/Player/Idle",
        walk="/Game/Character/Player/Walking",
        run="/Game/Character/Player/Running",
        sit="/Game/Character/Player/Sitting_Idle",
        profile="DA_VHV_Player_Animations",
        abp="ABP_VHV_Mixamo_Player",
        run_threshold=350.0,
    ),
    "instructor": dict(
        mesh="/Game/Character/instructor/instructor",
        idle="/Game/Character/instructor/Standing_Idle",
        walk="/Game/Character/instructor/Walk",
        sit="/Game/Character/instructor/Sitting",
        special="/Game/Character/instructor/Talking",
        dialogue_action=True,
        loop_action=True,
        profile="DA_VHV_Instructor_Animations",
        abp="ABP_VHV_Mixamo_NPC_Instructor",
    ),
    "npcB_renewed": dict(
        mesh="/Game/Character/npcB/renewed/NPCb",
        idle="/Game/Character/npcB/renewed/Idle",
        walk="/Game/Character/npcB/renewed/Walking",
        sit="/Game/Character/npcB/renewed/Sitting_Idle",
        profile="DA_VHV_NPCB_Renewed_Animations",
        abp="ABP_VHV_Mixamo_NPC_RenewedBC",
    ),
    "npcC_renewed": dict(
        mesh="/Game/Character/npcC/renewed/NPCb",
        idle="/Game/Character/npcC/renewed/Idle",
        walk="/Game/Character/npcC/renewed/Walking",
        sit="/Game/Character/npcC/renewed/Sitting_Idle",
        profile="DA_VHV_NPCC_Renewed_Animations",
        abp="ABP_VHV_Mixamo_NPC_RenewedBC",
    ),
    "npcE": dict(
        mesh="/Game/Character/npcE/WALKING_PARTNER",
        idle="/Game/Character/npcE/Breathing_Idle",
        walk="/Game/Character/npcE/Walking",
        profile="DA_VHV_NPCE_Animations",
        abp="ABP_VHV_Mixamo_NPC_NPCE",
    ),
    "somchai_standing": dict(
        mesh="/Game/Character/somchai/SOMCHAI",
        idle="/Game/Character/somchai/Breathing_Idle_",
        profile="DA_VHV_Somchai_Standing_Animations",
        abp_path=COMMON + "/ABP_VHV_Mixamo_NPC_Somchai",
    ),
}


EXISTING = {
    "ChaiWife": dict(
        mesh="/Game/Character/ChaiWife/CHAIWIFE__1_",
        profile=COMMON + "/DA_VHV_ChaiWife_Animations",
        abp=COMMON + "/ABP_VHV_Mixamo_NPC_ChaiWife",
    ),
    "npcA": dict(
        mesh="/Game/Character/npcA/CHAIWIFE__1_",
        profile=PROFILE_DIR + "/DA_VHV_NPCA_Animations",
        abp=COMMON + "/ABP_VHV_Mixamo_NPC_ChaiWife",
    ),
    "prassart": dict(
        mesh="/Game/Character/prassart/CHAI",
        profile=PROFILE_DIR + "/DA_VHV_Prassart_Animations",
        abp=COMMON + "/ABP_VHV_Mixamo_NPC_ChaiPrasert",
    ),
}


SPECIFIC_REPLACEMENTS = {
    "HBCT_Market_SugarDrinkShopper": "ChaiWife",
    "HBCT_RoleModel_FitVillager": "somchai_standing",
    "HBCT_GoalSetting_AuntieNuan": "npcA",
    "HBCT_RoleModel_Parent": "prassart",
}


def path_name(value):
    return value.get_path_name() if value else ""


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


def load(path, expected=None):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset or (expected and not isinstance(asset, expected)):
        raise RuntimeError("Missing or incorrect asset type: " + path)
    return asset


def participant_id(actor):
    component = actor.get_quest_participant_component()
    return str(component.get_effective_participant_id()) if component else ""


def gameplay_identity(actor):
    return [path_name(value) for value in (
        actor.get_quest_participant_component(), actor.get_dialogue_component(),
        actor.get_behavior_component(), actor.get_npc_interaction_component(),
        actor.get_patrol_component(), actor.get_quest_command_component(),
        actor.get_presentation_component(), actor.get_interaction_component())]


def actor_visual(actor):
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


def ensure_backup(source, destination):
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        return "existing"
    duplicated = unreal.EditorAssetLibrary.duplicate_asset(source, destination)
    if not duplicated:
        raise RuntimeError("Could not back up {} to {}".format(source, destination))
    if not unreal.EditorAssetLibrary.save_asset(destination, only_if_is_dirty=False):
        raise RuntimeError("Could not save backup " + destination)
    return "created"


def ensure_anim_blueprint(name, skeleton):
    path = COMMON + "/" + name
    blueprint = (unreal.EditorAssetLibrary.load_asset(path)
                 if unreal.EditorAssetLibrary.does_asset_exist(path) else None)
    if not blueprint:
        factory = unreal.AnimBlueprintFactory()
        factory.set_editor_properties({
            "target_skeleton": skeleton,
            "parent_class": unreal.VHVMixamoNPCAnimInstance.static_class(),
        })
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, COMMON, unreal.AnimBlueprint, factory)
    if not isinstance(blueprint, unreal.AnimBlueprint):
        raise RuntimeError(path + " is not an AnimBlueprint")
    if blueprint.get_editor_property("target_skeleton") != skeleton:
        raise RuntimeError(path + " targets the wrong skeleton")
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    generated_class = blueprint.generated_class()
    if not generated_class:
        raise RuntimeError(path + " did not generate an animation class")
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + path)
    return blueprint


def ensure_profile(spec, mesh):
    path = PROFILE_DIR + "/" + spec["profile"]
    profile = (unreal.EditorAssetLibrary.load_asset(path)
               if unreal.EditorAssetLibrary.does_asset_exist(path) else None)
    if not profile:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VHVNPCAnimationProfile)
        profile = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            spec["profile"], PROFILE_DIR, unreal.VHVNPCAnimationProfile, factory)
    if not isinstance(profile, unreal.VHVNPCAnimationProfile):
        raise RuntimeError(path + " is not a VHVNPCAnimationProfile")
    profile.modify()
    profile.set_editor_properties({
        "standing_idle_animation": load(spec["idle"], unreal.AnimSequence),
        "walking_animation": load(spec["walk"], unreal.AnimSequence) if spec.get("walk") else None,
        "running_animation": load(spec["run"], unreal.AnimSequence) if spec.get("run") else None,
        "sitting_idle_animation": load(spec["sit"], unreal.AnimSequence) if spec.get("sit") else None,
        "special_action_animation": load(spec["special"], unreal.AnimSequence) if spec.get("special") else None,
        "use_special_action_during_dialogue": bool(spec.get("dialogue_action", False)),
        "loop_special_action": bool(spec.get("loop_action", False)),
        "special_action_playback_speed": 1.0,
        "idle_walk_threshold": 5.0,
        "walk_playback_speed": 1.0,
        "run_speed_threshold": float(spec.get("run_threshold", 350.0)),
        "run_playback_speed": 1.0,
        "transition_blend_time": 0.2,
        "initial_posture": unreal.VHVNPCPosture.STANDING,
    })
    skeleton = mesh.get_editor_property("skeleton")
    if not profile.has_all_required_animations() or not profile.is_compatible_with_skeleton(skeleton):
        raise RuntimeError("Profile validation failed: " + path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + path)
    return profile


def prepare_assets():
    unreal.EditorAssetLibrary.make_directory(PROFILE_DIR)
    result = {}
    shared_abps = {}
    for key, spec in SPECS.items():
        mesh = load(spec["mesh"], unreal.SkeletalMesh)
        skeleton = mesh.get_editor_property("skeleton")
        if not skeleton:
            raise RuntimeError(spec["mesh"] + " has no skeleton")
        profile = ensure_profile(spec, mesh)
        if spec.get("abp_path"):
            blueprint = load(spec["abp_path"], unreal.AnimBlueprint)
            if blueprint.get_editor_property("target_skeleton") != skeleton:
                raise RuntimeError(spec["abp_path"] + " targets the wrong skeleton")
        else:
            abp_key = (spec["abp"], path_name(skeleton))
            blueprint = shared_abps.get(abp_key)
            if not blueprint:
                blueprint = ensure_anim_blueprint(spec["abp"], skeleton)
                shared_abps[abp_key] = blueprint
        result[key] = {
            "mesh": mesh,
            "profile": profile,
            "abp": blueprint,
            "anim_class": blueprint.generated_class(),
        }
    for key, spec in EXISTING.items():
        mesh = load(spec["mesh"], unreal.SkeletalMesh)
        profile = load(spec["profile"], unreal.VHVNPCAnimationProfile)
        blueprint = load(spec["abp"], unreal.AnimBlueprint)
        skeleton = mesh.get_editor_property("skeleton")
        if not profile.is_compatible_with_skeleton(skeleton):
            raise RuntimeError(spec["profile"] + " is incompatible with " + spec["mesh"])
        if blueprint.get_editor_property("target_skeleton") != skeleton:
            raise RuntimeError(spec["abp"] + " targets the wrong skeleton")
        result[key] = {
            "mesh": mesh,
            "profile": profile,
            "abp": blueprint,
            "anim_class": blueprint.generated_class(),
        }
    return result


def configure_component(component, assets, mesh_z):
    component.modify()
    component.set_anim_instance_class(None)
    component.set_skinned_asset_and_update(assets["mesh"])
    component.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
    component.set_anim_instance_class(assets["anim_class"])
    component.set_relative_location(unreal.Vector(0.0, 0.0, mesh_z), False, False)
    component.set_relative_rotation(
        unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0), False, False)
    component.set_relative_scale3d(unreal.Vector(1.0, 1.0, 1.0))


def configure_npc(actor, assets):
    actor.modify()
    actor.set_animation_profile(None)
    configure_component(actor.get_editor_property("mesh"), assets, -96.0)
    actor.set_animation_profile(assets["profile"])
    actor.set_npc_posture(unreal.VHVNPCPosture.STANDING)
    actor.set_special_animation_action_active(False)


def player_snapshot(default):
    component = default.get_editor_property("mesh")
    capsule = default.get_editor_property("capsule_component")
    boom = default.get_editor_property("camera_boom")
    camera = default.get_editor_property("follow_camera")
    return {
        "capsule_radius": capsule.get_unscaled_capsule_radius(),
        "capsule_half_height": capsule.get_unscaled_capsule_half_height(),
        "camera_boom_target_arm_length": boom.get_editor_property("target_arm_length"),
        "camera_boom_relative_location": vector(boom.get_editor_property("relative_location")),
        "camera_boom_relative_rotation": rotator(boom.get_editor_property("relative_rotation")),
        "camera_relative_location": vector(camera.get_editor_property("relative_location")),
        "camera_relative_rotation": rotator(camera.get_editor_property("relative_rotation")),
        "mesh": path_name(component.get_skeletal_mesh_asset()),
        "anim_class": path_name(component.get_editor_property("anim_class")),
        "profile": path_name(default.get_editor_property("player_animation_profile")),
        "mesh_relative_location": vector(component.get_editor_property("relative_location")),
        "mesh_relative_rotation": rotator(component.get_editor_property("relative_rotation")),
        "mesh_relative_scale": vector(component.get_editor_property("relative_scale3d")),
    }


def run():
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    unreal.EditorAssetLibrary.make_directory(BACKUP_DIR)
    backups = {
        "level": ensure_backup(
            MAP_PATH, BACKUP_DIR + "/Lvl_Village_Main_PreCharacterReplacement"),
        "player_blueprint": ensure_backup(
            PLAYER_BP_PATH, BACKUP_DIR + "/BP_ThirdPersonCharacter_PreCharacterReplacement"),
    }
    prepared = prepare_assets()

    backup_map_path = BACKUP_DIR + "/Lvl_Village_Main_PreCharacterReplacement"
    if not unreal.EditorLoadingAndSavingUtils.load_map(backup_map_path):
        raise RuntimeError("Could not load baseline backup map")
    baseline_npcs = [actor for actor in
                     unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                     if isinstance(actor, unreal.VHVNPCCharacter)]
    baseline_visual = {actor.get_actor_label(): actor_visual(actor) for actor in baseline_npcs}
    baseline_transform = {actor.get_actor_label(): transform(actor) for actor in baseline_npcs}
    baseline_old_b_count = sum(
        1 for actor in baseline_npcs
        if actor_visual(actor)["mesh"] == "/Game/Character/npcB/NPCc.NPCc")
    baseline_old_c_count = sum(
        1 for actor in baseline_npcs
        if actor_visual(actor)["mesh"] == "/Game/Character/npcC/NPCc.NPCc")

    player_blueprint = load(PLAYER_BP_PATH, unreal.Blueprint)
    player_class = player_blueprint.generated_class()
    player_default = unreal.get_default_object(player_class)
    player_before = player_snapshot(player_default)
    player_blueprint.modify()
    player_default.modify()
    configure_component(player_default.get_editor_property("mesh"), prepared["player"], -90.0)
    player_default.set_editor_property("player_animation_profile", prepared["player"]["profile"])
    unreal.BlueprintEditorLibrary.compile_blueprint(player_blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(player_blueprint, only_if_is_dirty=False):
        raise RuntimeError("Could not save player Blueprint")
    player_after = player_snapshot(player_default)
    preserved_player_settings = [
        "capsule_radius", "capsule_half_height", "camera_boom_target_arm_length",
        "camera_boom_relative_location", "camera_boom_relative_rotation",
        "camera_relative_location", "camera_relative_rotation",
    ]
    for key in preserved_player_settings:
        if player_before[key] != player_after[key]:
            raise RuntimeError("Player {} changed unexpectedly".format(key))

    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load " + MAP_PATH)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    npcs = [actor for actor in subsystem.get_all_level_actors()
            if isinstance(actor, unreal.VHVNPCCharacter)]
    labels = {}
    for actor in npcs:
        labels.setdefault(actor.get_actor_label(), []).append(actor)
    required_labels = list(SPECIFIC_REPLACEMENTS) + ["HBCT_Intro_Instructor"]
    invalid_labels = [label for label in required_labels if len(labels.get(label, [])) != 1]
    if invalid_labels:
        raise RuntimeError("Missing or duplicate target labels: " + ", ".join(invalid_labels))
    instructor = labels["HBCT_Intro_Instructor"][0]
    if participant_id(instructor) != "Instructor":
        raise RuntimeError("HBCT_Intro_Instructor does not have participant ID Instructor")

    before_transform = {path_name(actor): transform(actor) for actor in npcs}
    before_gameplay = {path_name(actor): gameplay_identity(actor) for actor in npcs}
    before_visual = {actor.get_actor_label(): actor_visual(actor) for actor in npcs}

    old_b_path = "/Game/Character/npcB/NPCc.NPCc"
    old_c_path = "/Game/Character/npcC/NPCc.NPCc"
    renewed_b_path = "/Game/Character/npcB/renewed/NPCb.NPCb"
    renewed_c_path = "/Game/Character/npcC/renewed/NPCb.NPCb"
    b_actors = []
    c_actors = []
    for actor in npcs:
        mesh_path = path_name(actor.get_editor_property("mesh").get_skeletal_mesh_asset())
        if mesh_path in (old_b_path, renewed_b_path):
            b_actors.append(actor)
        elif mesh_path in (old_c_path, renewed_c_path):
            c_actors.append(actor)
    if not b_actors or not c_actors:
        raise RuntimeError("Could not resolve the complete NPC B/C appearance groups")

    for actor in b_actors:
        configure_npc(actor, prepared["npcB_renewed"])
    for actor in c_actors:
        configure_npc(actor, prepared["npcC_renewed"])

    configure_npc(instructor, prepared["instructor"])
    for label, key in SPECIFIC_REPLACEMENTS.items():
        configure_npc(labels[label][0], prepared[key])

    failures = []
    changed_rows = []
    for actor in npcs:
        actor_path = path_name(actor)
        if transform(actor) != before_transform[actor_path]:
            failures.append(actor.get_actor_label() + ": actor world transform changed")
        if gameplay_identity(actor) != before_gameplay[actor_path]:
            failures.append(actor.get_actor_label() + ": gameplay component identity changed")
        after = actor_visual(actor)
        if actor.get_actor_label() not in baseline_visual:
            failures.append(actor.get_actor_label() + ": absent from baseline backup")
        elif transform(actor) != baseline_transform[actor.get_actor_label()]:
            failures.append(actor.get_actor_label() + ": world transform differs from baseline backup")
        if actor.get_actor_label() in baseline_visual and after != baseline_visual[actor.get_actor_label()]:
            changed_rows.append({
                "label": actor.get_actor_label(),
                "participant_id": participant_id(actor),
                "world_transform": transform(actor),
                "before": baseline_visual[actor.get_actor_label()],
                "after": after,
            })
        mesh = actor.get_editor_property("mesh").get_skeletal_mesh_asset()
        profile = actor.get_animation_profile()
        if actor in b_actors + c_actors or actor == instructor or actor.get_actor_label() in SPECIFIC_REPLACEMENTS:
            if not profile or not profile.is_compatible_with_skeleton(mesh.get_editor_property("skeleton")):
                failures.append(actor.get_actor_label() + ": incompatible animation profile")
    remaining_old_b = [actor.get_actor_label() for actor in npcs if path_name(
        actor.get_editor_property("mesh").get_skeletal_mesh_asset()) == old_b_path]
    remaining_old_c = [actor.get_actor_label() for actor in npcs if path_name(
        actor.get_editor_property("mesh").get_skeletal_mesh_asset()) == old_c_path]
    if remaining_old_b or remaining_old_c:
        failures.append("Old B/C appearances remain: {} {}".format(remaining_old_b, remaining_old_c))

    expected_specific_meshes = {
        label: path_name(prepared[key]["mesh"]) for label, key in SPECIFIC_REPLACEMENTS.items()}
    for label, mesh_path in expected_specific_meshes.items():
        if path_name(labels[label][0].get_editor_property("mesh").get_skeletal_mesh_asset()) != mesh_path:
            failures.append(label + ": specific replacement mismatch")
    if path_name(instructor.get_editor_property("mesh").get_skeletal_mesh_asset()) != path_name(
            prepared["instructor"]["mesh"]):
        failures.append("Instructor replacement mismatch")
    if failures:
        raise RuntimeError("Safety validation failed: " + "; ".join(failures))

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Failed to save Lvl_Village_Main")

    profiles = {}
    blueprints = {}
    for key, assets in prepared.items():
        if key in SPECS:
            profiles[key] = path_name(assets["profile"])
            blueprints[key] = {
                "path": path_name(assets["abp"]),
                "target_skeleton": path_name(
                    assets["abp"].get_editor_property("target_skeleton")),
                "generated_class": path_name(assets["anim_class"]),
                "compiled": bool(assets["anim_class"]),
            }
    result = {
        "map": MAP_PATH,
        "backups": backups,
        "player": {"before": player_before, "after": player_after},
        "selected_assets": {
            "player_mesh": path_name(prepared["player"]["mesh"]),
            "instructor_mesh": path_name(prepared["instructor"]["mesh"]),
            "npcB_corrected_mesh": path_name(prepared["npcB_renewed"]["mesh"]),
            "npcC_corrected_mesh": path_name(prepared["npcC_renewed"]["mesh"]),
            "npcE_mesh": path_name(prepared["npcE"]["mesh"]),
        },
        "profiles": profiles,
        "animation_blueprints": blueprints,
        "old_b_actor_count": baseline_old_b_count,
        "old_c_actor_count": baseline_old_c_count,
        "changed_actor_count": len(changed_rows),
        "changed_actors": sorted(changed_rows, key=lambda row: row["label"]),
        "specific_replacements": expected_specific_meshes,
        "validation": {
            "all_actor_world_transforms_preserved": True,
            "all_gameplay_component_identities_preserved": True,
            "player_camera_and_capsule_preserved": True,
            "old_b_mesh_remaining": remaining_old_b,
            "old_c_mesh_remaining": remaining_old_c,
            "npcE_assigned_to_level_actor": False,
        },
    }
    with open(OUTPUT_PATH, "w", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2, ensure_ascii=False)
    unreal.log_warning("VHV_CHARACTER_REPLACEMENT_RESULT=" + OUTPUT_PATH)
    unreal.log_warning("VHV_CHARACTER_REPLACEMENT_COMPLETE changed={} oldB={} oldC={}".format(
        len(changed_rows), len(b_actors), len(c_actors)))


try:
    run()
except Exception:
    unreal.log_error(traceback.format_exc())
    raise
