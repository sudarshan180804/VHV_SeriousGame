"""Audit, apply, or restore the Phase 3 PointLight5 attenuation-radius fix."""

import hashlib
import json
import os
import traceback

import unreal


MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
PROJECT = unreal.Paths.project_dir()
MAP_FILE = os.path.join(PROJECT, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
REPORT_FILE = os.path.join(PROJECT, "Saved", "Reports", "VHV_Phase3_LightRadius_Apply.json")
EXPECTED_PHASE2_HASH = "42e8802caf21d27d29d603929cdffe986a83cb6cfdc59a41a272be76e6fdacad"
ACTOR_NAME = "PointLight_1"
ACTOR_LABEL = "PointLight5"
EXPECTED_LOCATION = (-4624.002988129077, 9530.0, 813.9999999107108)
OPTIMIZED_RADIUS = 2000.0
ORIGINAL_RADIUS = 20000.0
LOG = "[VHV Phase3 Light Radius]"


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def close(a, b, tolerance=0.01):
    return abs(float(a) - float(b)) <= tolerance


def run():
    command_line = unreal.SystemLibrary.get_command_line()
    apply_mode = "-VHVPhase3ApplyLightRadius" in command_line
    restore_mode = "-VHVPhase3RestoreLightRadius" in command_line
    if apply_mode and restore_mode:
        raise RuntimeError("Apply and restore flags are mutually exclusive")

    before_hash = sha256(MAP_FILE)
    if apply_mode and before_hash.lower() != EXPECTED_PHASE2_HASH:
        raise RuntimeError(
            "Apply requires the verified Phase 2 map hash; found {}".format(before_hash)
        )

    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    if not world:
        raise RuntimeError("Failed to load production map")
    actors = unreal.EditorActorSubsystem().get_all_level_actors()
    matches = [actor for actor in actors if actor.get_name() == ACTOR_NAME]
    if len(matches) != 1:
        raise RuntimeError("Expected one {}, found {}".format(ACTOR_NAME, len(matches)))
    actor = matches[0]
    if actor.get_actor_label() != ACTOR_LABEL:
        raise RuntimeError("Unexpected actor label: " + actor.get_actor_label())
    location = actor.get_actor_location()
    actual_location = (location.x, location.y, location.z)
    if any(not close(a, b) for a, b in zip(actual_location, EXPECTED_LOCATION)):
        raise RuntimeError("Unexpected PointLight5 location: {}".format(actual_location))
    component = actor.get_component_by_class(unreal.PointLightComponent)
    if not component:
        raise RuntimeError("PointLight5 has no PointLightComponent")

    before_radius = float(component.get_editor_property("attenuation_radius"))
    changed = False
    target_radius = before_radius
    if apply_mode:
        if not close(before_radius, ORIGINAL_RADIUS):
            raise RuntimeError("Apply expected radius {}, found {}".format(ORIGINAL_RADIUS, before_radius))
        target_radius = OPTIMIZED_RADIUS
    elif restore_mode:
        if not close(before_radius, OPTIMIZED_RADIUS):
            raise RuntimeError("Restore expected radius {}, found {}".format(OPTIMIZED_RADIUS, before_radius))
        target_radius = ORIGINAL_RADIUS

    if apply_mode or restore_mode:
        component.modify()
        component.set_editor_property("attenuation_radius", target_radius)
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
            raise RuntimeError("Failed to save production map")
        changed = True

    after_radius = float(component.get_editor_property("attenuation_radius"))
    if not close(after_radius, target_radius):
        raise RuntimeError("Radius did not become {}".format(target_radius))
    after_hash = sha256(MAP_FILE)
    payload = {
        "schema": 1,
        "map": MAP,
        "actor": actor.get_path_name(),
        "label": actor.get_actor_label(),
        "location": actual_location,
        "mode": "apply" if apply_mode else "restore" if restore_mode else "audit",
        "changed": changed,
        "attenuation_radius_before": before_radius,
        "attenuation_radius_after": after_radius,
        "map_sha256_before": before_hash,
        "map_sha256_after": after_hash,
    }
    os.makedirs(os.path.dirname(REPORT_FILE), exist_ok=True)
    with open(REPORT_FILE, "w", encoding="utf-8") as stream:
        json.dump(payload, stream, indent=2, sort_keys=True)
    unreal.log(LOG + " " + json.dumps(payload, sort_keys=True))


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error(LOG + " FAILED:\n" + traceback.format_exc())
        raise
