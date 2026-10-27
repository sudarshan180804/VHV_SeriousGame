"""Read-only Phase 2 inventory for the converted production village."""

import hashlib
import json
import os

import unreal


PROJECT = unreal.Paths.project_dir()
MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
TARGET_ROOT = "/Game/VHV_StaticVegetation/"
REPORT = os.path.join(PROJECT, "Saved", "Reports", "VHV_Phase2_Render_Inventory.json")


def value(obj, name):
    try:
        result = obj.get_editor_property(name)
    except Exception as exc:
        return {"unsupported": str(exc)}
    if hasattr(result, "get_path_name"):
        return result.get_path_name()
    if isinstance(result, unreal.EnumBase):
        return str(result)
    if isinstance(result, (bool, int, float, str)) or result is None:
        return result
    return str(result)


def properties(obj, names):
    return {name: value(obj, name) for name in names}


def component(actor):
    return actor.get_component_by_class(unreal.StaticMeshComponent)


def object_path(obj):
    return obj.get_path_name() if obj else ""


world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
if not world:
    raise RuntimeError("Failed to load production map read-only")

actors = unreal.EditorActorSubsystem().get_all_level_actors()
vegetation = []
mobility_counts = {}
for actor in actors:
    comp = component(actor)
    if not comp:
        continue
    mesh = comp.get_editor_property("static_mesh")
    mesh_path = object_path(mesh)
    if not mesh_path.startswith(TARGET_ROOT):
        continue
    mobility = str(comp.get_editor_property("mobility"))
    mobility_counts[mobility] = mobility_counts.get(mobility, 0) + 1
    vegetation.append({
        "actor": actor.get_path_name(),
        "label": actor.get_actor_label(),
        "mesh": mesh_path,
        "mobility": mobility,
        "render": properties(comp, [
            "cast_shadow", "cast_dynamic_shadow", "cast_static_shadow",
            "cast_contact_shadow", "evaluate_world_position_offset",
            "visible", "hidden_in_game", "bounds_scale",
            "allow_cull_distance_volume", "never_distance_cull",
        ]),
    })

directional_lights = []
sky_lights = []
post_process_volumes = []
for actor in actors:
    if isinstance(actor, unreal.DirectionalLight):
        comp = actor.get_component_by_class(unreal.DirectionalLightComponent)
        directional_lights.append({
            "actor": actor.get_path_name(),
            "label": actor.get_actor_label(),
            "component": properties(comp, [
                "mobility", "intensity", "cast_shadows", "cast_dynamic_shadows",
                "cast_static_shadows", "cast_volumetric_shadow",
                "dynamic_shadow_distance_movable_light",
                "dynamic_shadow_distance_stationary_light",
                "num_dynamic_shadow_cascades", "shadow_bias", "shadow_slope_bias",
                "contact_shadow_length", "affect_global_illumination",
                "affect_reflection",
            ]) if comp else None,
        })
    elif isinstance(actor, unreal.SkyLight):
        comp = actor.get_component_by_class(unreal.SkyLightComponent)
        sky_lights.append({
            "actor": actor.get_path_name(),
            "label": actor.get_actor_label(),
            "component": properties(comp, [
                "mobility", "intensity", "real_time_capture", "cast_shadows",
                "lower_hemisphere_is_black", "cubemap_resolution",
                "affect_global_illumination", "affect_reflection",
            ]) if comp else None,
        })
    elif isinstance(actor, unreal.PostProcessVolume):
        settings = actor.get_editor_property("settings")
        post_process_volumes.append({
            "actor": actor.get_path_name(),
            "label": actor.get_actor_label(),
            "enabled": value(actor, "enabled"),
            "unbound": value(actor, "unbound"),
            "priority": value(actor, "priority"),
            "settings": properties(settings, [
                "dynamic_global_illumination_method", "reflection_method",
                "lumen_scene_lighting_quality", "lumen_scene_detail",
                "lumen_scene_view_distance", "lumen_final_gather_quality",
                "lumen_reflection_quality", "lumen_reflections_screen_traces",
                "lumen_final_gather_screen_traces", "motion_blur_amount",
                "bloom_intensity", "ambient_occlusion_intensity",
            ]),
        })

map_file = os.path.join(PROJECT, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
with open(map_file, "rb") as stream:
    map_sha256 = hashlib.sha256(stream.read()).hexdigest()

report = {
    "schema": 1,
    "map": MAP,
    "map_sha256": map_sha256,
    "level_actor_count": len(actors),
    "static_vegetation_count": len(vegetation),
    "mobility_counts": mobility_counts,
    "vegetation": vegetation,
    "directional_lights": directional_lights,
    "sky_lights": sky_lights,
    "post_process_volumes": post_process_volumes,
}
os.makedirs(os.path.dirname(REPORT), exist_ok=True)
with open(REPORT, "w", encoding="utf-8") as stream:
    json.dump(report, stream, indent=2, sort_keys=True)

unreal.log("VHV_PHASE2_INVENTORY " + json.dumps({
    "map_sha256": map_sha256,
    "static_vegetation_count": len(vegetation),
    "mobility_counts": mobility_counts,
    "directional_lights": len(directional_lights),
    "sky_lights": len(sky_lights),
    "post_process_volumes": len(post_process_volumes),
}, sort_keys=True))
