"""Apply the two actor-scoped fixes and remove the player Jump input mapping."""

import json
import os

import unreal


MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
BACKUP_DIR = "/Game/VHV_Backups/SittingToggle_20261023"
BACKUP_MAP = BACKUP_DIR + "/Lvl_Village_Main_PreSittingToggle"
INPUT_CONTEXT = "/Game/Input/IMC_Default"
BACKUP_INPUT = BACKUP_DIR + "/IMC_Default_PreJumpDisable"
OUTPUT = os.path.join(unreal.Paths.project_saved_dir(), "Diagnostics", "sitting_toggle_integration.json")

TARGETS = {
    "HBCT_Motivation_SaengVHV": {
        "mesh": "/Game/Character/nurse/nurse",
        "profile": "/Game/Character/MixamoCommon/Profiles/DA_VHV_Nurse_Animations",
        "abp": "/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_Nurse",
    },
    "HBCT_RoleModel_FitVillager": {
        "mesh": "/Game/Character/npcE/WALKING_PARTNER",
        "profile": "/Game/Character/MixamoCommon/Profiles/DA_VHV_NPCE_Animations",
        "abp": "/Game/Character/MixamoCommon/ABP_VHV_Mixamo_NPC_NPCE",
    },
}


def path(value):
    return value.get_path_name() if value else ""


def load(asset_path, expected=None):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset or (expected and not isinstance(asset, expected)):
        raise RuntimeError("Missing or wrong-class asset " + asset_path)
    return asset


def transform(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return {
        "location": [round(location.x, 4), round(location.y, 4), round(location.z, 4)],
        "rotation": [round(rotation.pitch, 4), round(rotation.yaw, 4), round(rotation.roll, 4)],
        "scale": [round(scale.x, 4), round(scale.y, 4), round(scale.z, 4)],
    }


def identity(actor):
    participant = actor.get_quest_participant_component()
    dialogue = actor.get_dialogue_component()
    return {
        "label": actor.get_actor_label(),
        "tags": sorted(str(tag) for tag in actor.tags),
        "participant": str(participant.get_effective_participant_id()) if participant else "None",
        "quest_enabled": bool(participant.get_editor_property("quest_participation_enabled")) if participant else False,
        "conversation": path(dialogue.get_editor_property("default_conversation")) if dialogue else "",
    }


def visual(actor):
    component = actor.get_editor_property("mesh")
    return {
        "mesh": path(component.get_skeletal_mesh_asset()),
        "materials": [path(material) for material in component.get_materials()],
        "profile": path(actor.get_animation_profile()),
        "anim_class": path(component.get_editor_property("anim_class")),
        "relative_location": str(component.get_editor_property("relative_location")),
        "relative_rotation": str(component.get_editor_property("relative_rotation")),
        "relative_scale": str(component.get_editor_property("relative_scale3d")),
    }


def mapping_rows(context):
    data = context.get_editor_property("default_key_mappings")
    return [{
        "action": path(mapping.get_editor_property("action")),
        "key": str(mapping.get_editor_property("key").get_editor_property("key_name")),
    } for mapping in list(data.get_editor_property("mappings"))]


unreal.EditorAssetLibrary.make_directory(BACKUP_DIR)
if not unreal.EditorAssetLibrary.does_asset_exist(BACKUP_MAP):
    if not unreal.EditorAssetLibrary.duplicate_asset(MAP, BACKUP_MAP):
        raise RuntimeError("Could not create map backup")
    if not unreal.EditorAssetLibrary.save_asset(BACKUP_MAP, only_if_is_dirty=False):
        raise RuntimeError("Could not save map backup")
if not unreal.EditorAssetLibrary.does_asset_exist(BACKUP_INPUT):
    if not unreal.EditorAssetLibrary.duplicate_asset(INPUT_CONTEXT, BACKUP_INPUT):
        raise RuntimeError("Could not create input backup")
    if not unreal.EditorAssetLibrary.save_asset(BACKUP_INPUT, only_if_is_dirty=False):
        raise RuntimeError("Could not save input backup")

if not unreal.EditorLoadingAndSavingUtils.load_map(MAP):
    raise RuntimeError("Could not load production map")
actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
labels = {actor.get_actor_label(): actor for actor in actors}
npcs = [actor for actor in actors if isinstance(actor, unreal.VHVNPCCharacter)]
before_transforms = {actor.get_actor_label(): transform(actor) for actor in npcs}
before_identities = {actor.get_actor_label(): identity(actor) for actor in npcs}

scene = labels.get("HBCT_Motivation_SaengConversation")
if not scene or not isinstance(scene, unreal.VHVAmbientConversationActor):
    raise RuntimeError("Motivation Saeng conversation actor missing")
bindings = {
    str(binding.get_editor_property("slot_id")): binding.get_editor_property("npc").get_actor_label()
    for binding in list(scene.get_editor_property("participants"))
}
if bindings.get("VHV") != "HBCT_Motivation_SaengVHV" or bindings.get("Saeng") != "HBCT_Motivation_Saeng":
    raise RuntimeError("Unexpected Saeng appreciation participant binding: " + str(bindings))

rows = []
for label, spec in TARGETS.items():
    actor = labels.get(label)
    if not actor or not isinstance(actor, unreal.VHVNPCCharacter):
        raise RuntimeError("Target NPC missing: " + label)
    mesh = load(spec["mesh"], unreal.SkeletalMesh)
    profile = load(spec["profile"], unreal.VHVNPCAnimationProfile)
    abp = load(spec["abp"], unreal.AnimBlueprint)
    skeleton = mesh.get_editor_property("skeleton")
    if not profile.is_compatible_with_skeleton(skeleton):
        raise RuntimeError(label + " profile/skeleton mismatch")
    if abp.get_editor_property("target_skeleton") != skeleton:
        raise RuntimeError(label + " AnimBP/skeleton mismatch")
    before = visual(actor)
    component = actor.get_editor_property("mesh")
    actor.modify()
    component.modify()
    actor.set_animation_profile(None)
    component.set_anim_instance_class(None)
    component.set_skinned_asset_and_update(mesh)
    component.set_editor_property("override_materials", [])
    component.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
    component.set_anim_instance_class(abp.generated_class())
    actor.set_animation_profile(profile)
    actor.set_is_sitting(False)
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    rows.append({"actor": label, "identity": identity(actor), "before": before, "after": visual(actor)})

context = load(INPUT_CONTEXT, unreal.InputMappingContext)
jump = load("/Game/Input/Actions/IA_Jump", unreal.InputAction)
input_before = mapping_rows(context)
context.modify()
context.unmap_all_keys_from_action(jump)
input_after = mapping_rows(context)
if any(row["action"] == path(jump) for row in input_after):
    raise RuntimeError("Jump mappings remain in IMC_Default")
if not any(row["key"] == "X" and row["action"].endswith("IA_LocateObjective.IA_LocateObjective") for row in input_after):
    raise RuntimeError("X objective navigation mapping was lost")
if not unreal.EditorAssetLibrary.save_loaded_asset(context, only_if_is_dirty=False):
    raise RuntimeError("Could not save IMC_Default")

player_bp = load("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter", unreal.Blueprint)
unreal.BlueprintEditorLibrary.compile_blueprint(player_bp)
if not player_bp.generated_class():
    raise RuntimeError("Player Blueprint compilation failed")

for actor in npcs:
    if before_transforms[actor.get_actor_label()] != transform(actor):
        raise RuntimeError(actor.get_actor_label() + " world transform changed")
    if before_identities[actor.get_actor_label()] != identity(actor):
        raise RuntimeError(actor.get_actor_label() + " gameplay identity changed")

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save production map")

report = {
    "map": MAP,
    "backup_map": BACKUP_MAP,
    "backup_input": BACKUP_INPUT,
    "motivation_bindings": bindings,
    "changed_actors": rows,
    "npc_count": len(npcs),
    "all_world_transforms_preserved": True,
    "all_gameplay_identities_preserved": True,
    "jump_mappings_before": [row for row in input_before if row["action"] == path(jump)],
    "jump_mappings_after": [row for row in input_after if row["action"] == path(jump)],
    "x_navigation_preserved": True,
    "sitting_property_default": False,
    "player_blueprint_compiled": True,
}
os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
with open(OUTPUT, "w", encoding="utf-8") as stream:
    json.dump(report, stream, indent=2, ensure_ascii=False)
unreal.log_warning("VHV_SITTING_TOGGLE_INTEGRATION=" + OUTPUT)
