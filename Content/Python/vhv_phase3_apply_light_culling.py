"""Apply or restore conservative camera-distance culling for six village point lights."""

import hashlib
import json
import os
import traceback

import unreal


MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
PROJECT = unreal.Paths.project_dir()
MAP_FILE = os.path.join(PROJECT, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
REPORT_FILE = os.path.join(PROJECT, "Saved", "Reports", "VHV_Phase3_LightCulling_Apply.json")
EXPECTED_RADIUS_FIX_HASH = "718956848dd6242f40808d233460e91bc97795c36f890593a852d301c95e5bd5"
MAX_DRAW_DISTANCE = 4000.0
FADE_RANGE = 1000.0
LIGHTS = {
    "PointLight_4": ("PointLight", (-5568.0, 20829.0, 999.0)),
    "PointLight_5": ("PointLight2", (-6226.0, 20829.0, 999.0)),
    "PointLight_6": ("PointLight3", (-6798.0, 20829.0, 999.0)),
    "PointLight_9": ("PointLight4", (781.9958684053668, 4930.0, 504.00000017070073)),
    "PointLight_1": ("PointLight5", (-4624.002988129077, 9530.0, 813.9999999107108)),
    "PointLight_0": ("PointLight6", (-3876.0, 15463.0, 611.0)),
}
LOG = "[VHV Phase3 Light Culling]"


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
    apply_mode = "-VHVPhase3ApplyLightCulling" in command_line
    restore_mode = "-VHVPhase3RestoreLightCulling" in command_line
    if apply_mode == restore_mode:
        raise RuntimeError("Specify exactly one apply or restore flag")
    before_hash = sha256(MAP_FILE)
    if apply_mode and before_hash.lower() != EXPECTED_RADIUS_FIX_HASH:
        raise RuntimeError("Apply requires the verified radius-fix hash; found " + before_hash)

    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    if not world:
        raise RuntimeError("Failed to load production map")
    by_name = {
        actor.get_name(): actor
        for actor in unreal.EditorActorSubsystem().get_all_level_actors()
    }
    target_distance = MAX_DRAW_DISTANCE if apply_mode else 0.0
    target_fade = FADE_RANGE if apply_mode else 0.0
    changed = []
    for name, (label, expected_location) in LIGHTS.items():
        actor = by_name.get(name)
        if not actor or actor.get_actor_label() != label:
            raise RuntimeError("Missing or mismatched point light: " + name)
        location = actor.get_actor_location()
        actual_location = (location.x, location.y, location.z)
        if any(not close(a, b) for a, b in zip(actual_location, expected_location)):
            raise RuntimeError("Unexpected location for {}: {}".format(name, actual_location))
        component = actor.get_component_by_class(unreal.PointLightComponent)
        before_distance = float(component.get_editor_property("max_draw_distance"))
        before_fade = float(component.get_editor_property("max_distance_fade_range"))
        expected_distance = 0.0 if apply_mode else MAX_DRAW_DISTANCE
        expected_fade = 0.0 if apply_mode else FADE_RANGE
        if not close(before_distance, expected_distance) or not close(before_fade, expected_fade):
            raise RuntimeError(
                "Unexpected culling state for {}: distance {}, fade {}".format(
                    name, before_distance, before_fade
                )
            )
        component.modify()
        component.set_editor_property("max_draw_distance", target_distance)
        component.set_editor_property("max_distance_fade_range", target_fade)
        changed.append(
            {
                "actor": actor.get_path_name(),
                "label": label,
                "before_max_draw_distance": before_distance,
                "after_max_draw_distance": target_distance,
                "before_fade_range": before_fade,
                "after_fade_range": target_fade,
            }
        )

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError("Failed to save production map")
    after_hash = sha256(MAP_FILE)
    payload = {
        "schema": 1,
        "mode": "apply" if apply_mode else "restore",
        "map": MAP,
        "map_sha256_before": before_hash,
        "map_sha256_after": after_hash,
        "changed_lights": changed,
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
