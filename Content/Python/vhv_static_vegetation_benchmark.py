"""Build two isolated, matched vegetation benchmark maps from the approved plan.

This script never loads or saves Lvl_Village_Main and refuses to overwrite an
existing benchmark map. Configuration A contains the original skeletal assets;
configuration B contains the validated direct-PVE Static Nanite assets.
"""

import hashlib
import json
import os
import traceback

import unreal


PROJECT = unreal.SystemLibrary.get_project_directory()
REPORT_DIR = os.path.join(PROJECT, "Saved", "Reports")
VALIDATION_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_PVE_Validation.json")
INVENTORY_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_PVE_Inventory.json")
MANIFEST_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Benchmark_Manifest.json")
TEMPLATE_MAP = "/Engine/Maps/Templates/Template_Default"
MAP_A = "/Game/VHV_StaticVegetation/Benchmark/Lvl_VHV_Vegetation_Benchmark_A_Skeletal"
MAP_B = "/Game/VHV_StaticVegetation/Benchmark/Lvl_VHV_Vegetation_Benchmark_B_Static"
PRODUCTION_MAP_FILE = os.path.join(PROJECT, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
LOG = "[VHV Vegetation Benchmark Builder]"


def _sha256(filename):
    digest = hashlib.sha256()
    with open(filename, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _component(actor, component_class):
    components = list(actor.get_components_by_class(component_class))
    if len(components) != 1:
        raise RuntimeError("Expected one {} on {}".format(component_class, actor.get_path_name()))
    return components[0]


def _set_common_rendering(component):
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    for name in ("cast_shadow", "cast_dynamic_shadow", "cast_static_shadow", "cast_contact_shadow",
                 "visible", "render_in_main_pass", "render_in_depth_pass", "use_as_occluder"):
        component.set_editor_property(name, True)
    component.set_editor_property("hidden_in_game", False)


def _spawn_camera(actor_subsystem, label, location, target, auto_activate=False):
    rotation = unreal.MathLibrary.find_look_at_rotation(location, target)
    actor = actor_subsystem.spawn_actor_from_class(unreal.CameraActor, location, rotation)
    if not actor:
        raise RuntimeError("Could not spawn camera {}".format(label))
    actor.set_actor_label(label)
    actor.set_folder_path("Benchmark/Cameras")
    camera = _component(actor, unreal.CameraComponent)
    camera.set_editor_property("field_of_view", 60.0)
    if auto_activate:
        actor.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
    return actor


def _new_level(level_subsystem, path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError("Refusing to overwrite existing benchmark level: {}".format(path))
    if not level_subsystem.new_level_from_template(path, TEMPLATE_MAP):
        raise RuntimeError("Could not create {} from {}".format(path, TEMPLATE_MAP))


def _build_level(level_subsystem, actor_subsystem, path, pairs, static_configuration):
    _new_level(level_subsystem, path)
    configuration = "Static" if static_configuration else "Skeletal"

    # A shared neutral ground catches identical dynamic shadows in both maps.
    ground = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, -5.0), unreal.Rotator())
    ground.set_actor_label("BenchmarkGround")
    ground.set_folder_path("Benchmark/Environment")
    ground.set_actor_scale3d(unreal.Vector(180.0, 180.0, 0.1))
    ground_component = _component(ground, unreal.StaticMeshComponent)
    ground_component.set_static_mesh(unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube"))
    ground_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    ground_component.set_simulate_physics(False)

    transforms = []
    for species_index, pair in enumerate(pairs):
        for instance_index in range(int(pair["instances_per_configuration"])):
            location = unreal.Vector((instance_index - 4.5) * 1400.0,
                                     (species_index - 2.5) * 2200.0, 0.0)
            rotation = unreal.Rotator(0.0, float((instance_index * 31 + species_index * 17) % 360), 0.0)
            if static_configuration:
                actor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
                component = _component(actor, unreal.StaticMeshComponent)
                asset = unreal.EditorAssetLibrary.load_asset(pair["static_mesh"])
                if not isinstance(asset, unreal.StaticMesh):
                    raise RuntimeError("Invalid benchmark static asset: {}".format(pair["static_mesh"]))
                component.set_static_mesh(asset)
                component.set_collision_profile_name("NoCollision")
                component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
                component.set_simulate_physics(False)
                component.set_evaluate_world_position_offset(False)
                component.set_evaluate_world_position_offset_in_ray_tracing(False)
            else:
                actor = actor_subsystem.spawn_actor_from_class(unreal.SkeletalMeshActor, location, rotation)
                component = _component(actor, unreal.SkeletalMeshComponent)
                asset = unreal.EditorAssetLibrary.load_asset(pair["skeletal_mesh"])
                if not isinstance(asset, unreal.SkeletalMesh):
                    raise RuntimeError("Invalid benchmark skeletal asset: {}".format(pair["skeletal_mesh"]))
                component.set_skeletal_mesh_asset(asset)
                component.set_collision_profile_name("PhysicsActor")
                component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
                component.set_simulate_physics(False)
            _set_common_rendering(component)
            actor.set_actor_label("Benchmark_{}_{}_{}_{:02d}".format(
                configuration, pair["species"], pair["variant"], instance_index + 1))
            actor.set_folder_path("Benchmark/Vegetation/{}/{}".format(pair["species"], pair["variant"]))
            actor.set_editor_property(
                "tags", ["VHV_Benchmark", configuration, pair["species"], pair["variant"]])
            transforms.append({"species": pair["species"], "variant": pair["variant"],
                               "instance": instance_index + 1,
                               "location": [location.x, location.y, location.z],
                               "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
                               "scale": [1.0, 1.0, 1.0]})

    cameras = {
        "measure": _spawn_camera(actor_subsystem, "BenchmarkCameraMeasure",
                                  unreal.Vector(-17000.0, -18000.0, 8500.0),
                                  unreal.Vector(0.0, 0.0, 650.0), True),
        "close": _spawn_camera(actor_subsystem, "BenchmarkCameraClose",
                                unreal.Vector(-2600.0, -6000.0, 1500.0),
                                unreal.Vector(0.0, -3300.0, 850.0)),
        "overview": _spawn_camera(actor_subsystem, "BenchmarkCameraOverview",
                                   unreal.Vector(0.0, -17500.0, 12500.0),
                                   unreal.Vector(0.0, 0.0, 500.0)),
    }
    if not level_subsystem.save_current_level():
        raise RuntimeError("Could not save isolated benchmark level {}".format(path))
    return {
        "map": path,
        "configuration": configuration,
        "vegetation_actor_count": len(transforms),
        "transforms": transforms,
        "camera_object_names": {name: camera.get_name() for name, camera in cameras.items()},
    }


def run():
    with open(VALIDATION_PATH, "r", encoding="utf-8") as stream:
        validation = json.load(stream)
    with open(INVENTORY_PATH, "r", encoding="utf-8") as stream:
        inventory = json.load(stream)
    plan = validation["performance"]["isolated_test_plan"]
    if plan.get("recommendation") != "GO" or len(plan.get("representative_pairs", [])) != 6:
        raise RuntimeError("Validation does not contain the approved six-pair GO benchmark plan")
    before_hash = _sha256(PRODUCTION_MAP_FILE)
    if before_hash != inventory["map_file"]["sha256"]:
        raise RuntimeError("Production map hash no longer matches protected inventory")
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not level_subsystem or not actor_subsystem:
        raise RuntimeError("Required editor subsystems are unavailable")
    pairs = plan["representative_pairs"]
    map_a = _build_level(level_subsystem, actor_subsystem, MAP_A, pairs, False)
    map_b = _build_level(level_subsystem, actor_subsystem, MAP_B, pairs, True)
    if map_a["transforms"] != map_b["transforms"]:
        raise RuntimeError("Benchmark A/B transforms are not identical")
    after_hash = _sha256(PRODUCTION_MAP_FILE)
    if before_hash != after_hash:
        raise RuntimeError("Safety stop: production map hash changed")
    manifest = {
        "schema": 1,
        "engine": "5.7.4",
        "production_map_loaded_or_saved": False,
        "production_map_sha256_before": before_hash,
        "production_map_sha256_after": after_hash,
        "blocked_variants_excluded": plan["excluded_variants"],
        "representative_pairs": pairs,
        "configuration_a": map_a,
        "configuration_b": map_b,
        "identical_transforms": True,
        "measurement": {"warmup_frames": 300, "measured_frames": 1800,
                        "order": ["A1", "B1", "B2", "A2"],
                        "resolution": [1920, 1080], "screen_percentage": 100,
                        "vsync": False, "fps_cap": 0},
    }
    with open(MANIFEST_PATH, "w", encoding="utf-8") as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
    unreal.log("{} created A/B maps with 60 vegetation actors each".format(LOG))
    return manifest


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} FAILED:\n{}".format(LOG, traceback.format_exc()))
        raise
