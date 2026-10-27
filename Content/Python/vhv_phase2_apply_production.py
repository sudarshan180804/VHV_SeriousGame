"""Apply the verified Phase 2 vegetation mobility change to production.

This script is deliberately narrow: it only changes the StaticMeshComponent
mobility of the 290 replacement actors recorded in the completed rollout audit.
"""

import hashlib
import json
import os
import traceback

import unreal


MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
MESH_ROOT = "/Game/VHV_StaticVegetation/"
EXPECTED_BEFORE = "4a30cc82351b7b4cf4c37b9ab3a343cb0fe980756cd6094b34418b414aa54310"
PROJECT = unreal.Paths.project_dir()
MAP_FILE = os.path.join(PROJECT, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
AUDIT_FILE = os.path.join(PROJECT, "Saved", "Reports", "VHV_StaticVegetation_Full_Rollout_Audit.json")
REPORT_FILE = os.path.join(PROJECT, "Saved", "Reports", "VHV_Phase2_Production_Apply.json")
LOG = "[VHV Phase2 Production Apply]"


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def full_object_path(path):
    """Normalize /Game/Folder/Asset to /Game/Folder/Asset.Asset."""
    leaf = path.rsplit("/", 1)[-1]
    return path if "." in leaf else "{}.{}".format(path, leaf)


def run():
    before_hash = sha256(MAP_FILE)
    if before_hash != EXPECTED_BEFORE:
        raise RuntimeError("Production map hash is not the verified Phase 1 state: {}".format(before_hash))

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or MAP not in world.get_path_name():
        raise RuntimeError("Production map must be the current editor world")

    with open(AUDIT_FILE, "r", encoding="utf-8") as stream:
        audit = json.load(stream)
    expected = {
        row["replacement_actor"].rsplit(".", 1)[-1]: row["target_static_mesh"]
        for row in audit["replacements"]
    }
    if len(expected) != 290:
        raise RuntimeError("Rollout audit does not contain 290 unique replacement actors")

    all_actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    by_name = {actor.get_name(): actor for actor in all_actors}
    verified = []
    for actor_name, expected_mesh in sorted(expected.items()):
        actor = by_name.get(actor_name)
        if actor is None:
            raise RuntimeError("Missing rollout actor: " + actor_name)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component is None:
            raise RuntimeError("Actor has no StaticMeshComponent: " + actor.get_path_name())
        mesh = component.get_editor_property("static_mesh")
        mesh_path = mesh.get_path_name() if mesh else ""
        normalized_expected = full_object_path(expected_mesh)
        if not mesh_path.startswith(MESH_ROOT) or mesh_path != normalized_expected:
            raise RuntimeError("Static mesh mismatch for {}: {} != {}".format(actor_name, mesh_path, normalized_expected))
        if component.is_simulating_physics():
            raise RuntimeError("Vegetation unexpectedly simulates physics: " + actor_name)
        collision = str(component.get_collision_profile_name())
        if collision != "NoCollision":
            raise RuntimeError("Vegetation collision policy changed for {}: {}".format(actor_name, collision))
        before = str(component.get_editor_property("mobility"))
        component.modify()
        component.set_mobility(unreal.ComponentMobility.STATIC)
        after = str(component.get_editor_property("mobility"))
        if "STATIC" not in after:
            raise RuntimeError("Mobility did not become Static for " + actor_name)
        verified.append({
            "actor": actor.get_path_name(),
            "mesh": mesh_path,
            "before": before,
            "after": after,
            "collision_profile": collision,
        })

    if len(verified) != 290:
        raise RuntimeError("Expected 290 verified vegetation components")
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError("Failed to save production map")
    after_hash = sha256(MAP_FILE)
    if after_hash == before_hash:
        raise RuntimeError("Map hash did not change after the mobility edit")

    payload = {
        "schema": 1,
        "map": MAP,
        "map_sha256_before": before_hash,
        "map_sha256_after": after_hash,
        "changed_components": len(verified),
        "all_static": True,
        "actors": verified,
    }
    os.makedirs(os.path.dirname(REPORT_FILE), exist_ok=True)
    with open(REPORT_FILE, "w", encoding="utf-8") as stream:
        json.dump(payload, stream, indent=2, sort_keys=True)
    unreal.log("{} {}".format(LOG, json.dumps({
        "changed_components": len(verified),
        "map_sha256_after": after_hash,
    }, sort_keys=True)))
    return payload


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} FAILED:\n{}".format(LOG, traceback.format_exc()))
        raise
