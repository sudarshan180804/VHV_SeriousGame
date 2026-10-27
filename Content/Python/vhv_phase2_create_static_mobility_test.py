"""Create a disposable production-map copy with only vegetation mobility changed."""

import hashlib
import json
import os

import unreal


SOURCE = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
TARGET_DIR = "/Game/VHV_StaticVegetation/Phase2"
TARGET = TARGET_DIR + "/Lvl_Village_Main_StaticMobility_Test"
MESH_ROOT = "/Game/VHV_StaticVegetation/"
PROJECT = unreal.Paths.project_dir()
REPORT = os.path.join(PROJECT, "Saved", "Reports", "VHV_Phase2_StaticMobility_TestMap.json")
FULL_AUDIT = os.path.join(PROJECT, "Saved", "Reports", "VHV_StaticVegetation_Full_Rollout_Audit.json")
SOURCE_FILE = os.path.join(PROJECT, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")


def sha256(path):
    with open(path, "rb") as stream:
        return hashlib.sha256(stream.read()).hexdigest()


source_before = sha256(SOURCE_FILE)
unreal.EditorAssetLibrary.make_directory(TARGET_DIR)

# Map duplication and mutation deliberately happen in separate editor launches.
# Loading a duplicated UWorld in the same Python turn can leave the duplicate
# package referenced during editor map teardown in UE 5.7.
if not unreal.EditorAssetLibrary.does_asset_exist(TARGET):
    duplicated = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET)
    if not duplicated:
        raise RuntimeError("Could not duplicate production map")
    if not unreal.EditorAssetLibrary.save_asset(TARGET, only_if_is_dirty=False):
        raise RuntimeError("Could not save duplicated test map")
    source_after = sha256(SOURCE_FILE)
    if source_after != source_before:
        raise RuntimeError("Production map changed while duplicating isolated test")
    unreal.log("VHV_PHASE2_STATIC_MOBILITY_DUPLICATED " + TARGET)
    raise SystemExit(0)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world or TARGET not in world.get_path_name():
    raise RuntimeError("Second launch must open the duplicated test map directly; current world is " +
                       (world.get_path_name() if world else "None"))

with open(FULL_AUDIT, "r", encoding="utf-8") as stream:
    rollout = json.load(stream)
target_actor_names = {
    row["replacement_actor"].rsplit(".", 1)[-1]
    for row in rollout["replacements"]
}
if len(target_actor_names) != 290:
    raise RuntimeError("Full rollout audit does not contain 290 unique replacement actors")

actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
changed = []
for actor in actors:
    if actor.get_name() not in target_actor_names:
        continue
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not comp:
        continue
    mesh = comp.get_editor_property("static_mesh")
    if not mesh or not mesh.get_path_name().startswith(MESH_ROOT):
        continue
    before = str(comp.get_editor_property("mobility"))
    comp.modify()
    comp.set_mobility(unreal.ComponentMobility.STATIC)
    after = str(comp.get_editor_property("mobility"))
    if "STATIC" not in after:
        raise RuntimeError("Mobility did not become Static for " + actor.get_path_name())
    changed.append({"actor": actor.get_path_name(), "before": before, "after": after})

if len(changed) != 290:
    raise RuntimeError("Expected 290 converted vegetation actors, found {}".format(len(changed)))
if not unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET):
    raise RuntimeError("Could not save disposable Static-mobility map")

source_after = sha256(SOURCE_FILE)
if source_after != source_before:
    raise RuntimeError("Production map changed while creating isolated test")

payload = {
    "schema": 1,
    "source": SOURCE,
    "target": TARGET,
    "source_sha256_before": source_before,
    "source_sha256_after": source_after,
    "changed_components": len(changed),
    "all_static": True,
    "actors": changed,
}
os.makedirs(os.path.dirname(REPORT), exist_ok=True)
with open(REPORT, "w", encoding="utf-8") as stream:
    json.dump(payload, stream, indent=2, sort_keys=True)
unreal.log("VHV_PHASE2_STATIC_MOBILITY_TEST " + json.dumps({
    "changed_components": len(changed), "source_sha256": source_after, "target": TARGET
}, sort_keys=True))
