"""Create the Lvl_Village_Main gameplay map, its environment Level Instance,
and the lightweight environment master material with its instances.

Safe to rerun: assets are only created when missing. Existing maps are never
rebuilt or overwritten, so level-design work in either map is preserved.

Layout:
  /Game/VHV_Stuff/Maps/Lvl_Village_Main                  gameplay map (lighting, nav, player start,
                                                          NPCs, quest volumes) + one Level Instance
  /Game/VHV_Stuff/Maps/Environment/LI_Village_Env        environment art only (ground, buildings, props)
  /Game/VHV_Stuff/Environment/Materials/M_Env_Master     lightweight master material
  /Game/VHV_Stuff/Environment/Materials/MI_Env_*         blockout colour presets
"""

import traceback
import unreal


MAIN_MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
ENV_MAP_PATH = "/Game/VHV_Stuff/Maps/Environment/LI_Village_Env"
MATERIAL_DIR = "/Game/VHV_Stuff/Environment/Materials"
MASTER_MATERIAL_NAME = "M_Env_Master"
LOG = "[Village Main Map]"

CUBE_MESH_PATH = "/Engine/BasicShapes/Cube"
WHITE_TEXTURE_PATH = "/Engine/EngineResources/WhiteSquareTexture"
FLAT_NORMAL_TEXTURE_PATH = "/Engine/EngineMaterials/DefaultNormal"

# Playable area in metres. The ground and the navigation bounds share this size.
VILLAGE_SIZE_M = 200.0
NAV_HEIGHT_M = 20.0

# Instance name -> (tint RGB, roughness, UV scale)
MATERIAL_PRESETS = {
    "MI_Env_Ground": ((0.30, 0.36, 0.20), 0.95, 1.0),
    "MI_Env_Road": ((0.45, 0.40, 0.33), 0.90, 1.0),
    "MI_Env_Wall": ((0.80, 0.74, 0.62), 0.85, 1.0),
    "MI_Env_Wood": ((0.40, 0.26, 0.15), 0.80, 1.0),
    "MI_Env_Roof": ((0.55, 0.22, 0.14), 0.70, 1.0),
    "MI_Env_Foliage": ((0.18, 0.35, 0.12), 0.90, 1.0),
}


def asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def editor_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def load_required(path):
    # unreal.load_asset also resolves /Engine content, which the asset registry
    # does not scan when the script runs headless.
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Missing required asset {}".format(path))
    return asset


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError("Could not create {}".format(path))


def save_asset(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def spawn_actor(actor_class, label, location=(0.0, 0.0, 0.0), rotation=None):
    actor = editor_actors().spawn_actor_from_class(
        actor_class, unreal.Vector(*location), rotation or unreal.Rotator())
    if not actor:
        raise RuntimeError("Could not spawn {}".format(label))
    actor.set_actor_label(label)
    return actor


# ---------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------

def build_master_material():
    path = "{}/{}".format(MATERIAL_DIR, MASTER_MATERIAL_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log("{} Keeping existing {}".format(LOG, path))
        return load_required(path)

    ensure_directory(MATERIAL_DIR)
    material = asset_tools().create_asset(
        MASTER_MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Could not create {}".format(path))

    mel = unreal.MaterialEditingLibrary
    expr = mel.create_material_expression

    # Shared UV tiling: TexCoord * UVScale.
    tex_coord = expr(material, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    uv_scale = expr(material, unreal.MaterialExpressionScalarParameter, -1100, 120)
    uv_scale.set_editor_properties({"parameter_name": "UVScale", "default_value": 1.0})
    tiled_uv = expr(material, unreal.MaterialExpressionMultiply, -900, 40)
    mel.connect_material_expressions(tex_coord, "", tiled_uv, "A")
    mel.connect_material_expressions(uv_scale, "", tiled_uv, "B")

    # Base colour: texture (white by default) * Tint.
    base_texture = expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, -200)
    base_texture.set_editor_properties({
        "parameter_name": "BaseColorTexture",
        "texture": load_required(WHITE_TEXTURE_PATH),
        "sampler_type": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    })
    mel.connect_material_expressions(tiled_uv, "", base_texture, "UVs")
    tint = expr(material, unreal.MaterialExpressionVectorParameter, -700, 50)
    tint.set_editor_properties({
        "parameter_name": "Tint",
        "default_value": unreal.LinearColor(1.0, 1.0, 1.0, 1.0),
    })
    base_color = expr(material, unreal.MaterialExpressionMultiply, -400, -100)
    mel.connect_material_expressions(base_texture, "RGB", base_color, "A")
    mel.connect_material_expressions(tint, "", base_color, "B")
    mel.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Roughness: a single scalar instead of a texture keeps memory low.
    roughness = expr(material, unreal.MaterialExpressionScalarParameter, -400, 100)
    roughness.set_editor_properties({"parameter_name": "Roughness", "default_value": 0.85})
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Normal map is optional and off by default (saves a texture sample and its memory).
    normal_texture = expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, 250)
    normal_texture.set_editor_properties({
        "parameter_name": "NormalTexture",
        "texture": load_required(FLAT_NORMAL_TEXTURE_PATH),
        "sampler_type": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
    })
    mel.connect_material_expressions(tiled_uv, "", normal_texture, "UVs")
    flat_normal = expr(material, unreal.MaterialExpressionConstant3Vector, -700, 500)
    flat_normal.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    use_normal = expr(material, unreal.MaterialExpressionStaticSwitchParameter, -400, 300)
    use_normal.set_editor_properties({"parameter_name": "UseNormalMap", "default_value": False})
    mel.connect_material_expressions(normal_texture, "RGB", use_normal, "True")
    mel.connect_material_expressions(flat_normal, "", use_normal, "False")
    mel.connect_material_property(use_normal, "", unreal.MaterialProperty.MP_NORMAL)

    mel.layout_material_expressions(material)
    mel.recompile_material(material)
    save_asset(material)
    unreal.log("{} Created {}".format(LOG, path))
    return material


def build_material_instances(master):
    mel = unreal.MaterialEditingLibrary
    instances = {}
    for name, (rgb, roughness, uv_scale) in MATERIAL_PRESETS.items():
        path = "{}/{}".format(MATERIAL_DIR, name)
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            instances[name] = load_required(path)
            continue
        instance = asset_tools().create_asset(
            name, MATERIAL_DIR, unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        if not instance:
            raise RuntimeError("Could not create {}".format(path))
        mel.set_material_instance_parent(instance, master)
        mel.set_material_instance_vector_parameter_value(
            instance, "Tint", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        mel.set_material_instance_scalar_parameter_value(instance, "Roughness", roughness)
        mel.set_material_instance_scalar_parameter_value(instance, "UVScale", uv_scale)
        mel.update_material_instance(instance)
        save_asset(instance)
        instances[name] = instance
        unreal.log("{} Created {}".format(LOG, path))
    return instances


# ---------------------------------------------------------------------------
# Maps
# ---------------------------------------------------------------------------

def new_map(path):
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if not world:
        raise RuntimeError("Could not create a blank map for {}".format(path))
    return world


def save_map(world, path):
    ensure_directory(path.rsplit("/", 1)[0])
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, path):
        raise RuntimeError("Could not save {}".format(path))


def build_environment_map(ground_material):
    if unreal.EditorAssetLibrary.does_asset_exist(ENV_MAP_PATH):
        unreal.log("{} Keeping existing {}".format(LOG, ENV_MAP_PATH))
        return

    world = new_map(ENV_MAP_PATH)
    # 1 m thick slab whose top surface sits at Z = 0.
    ground = spawn_actor(unreal.StaticMeshActor, "Env_Ground", (0.0, 0.0, -50.0))
    ground.set_folder_path("Ground")
    component = ground.get_editor_property("static_mesh_component")
    component.set_static_mesh(load_required(CUBE_MESH_PATH))
    component.set_material(0, ground_material)
    ground.set_actor_scale3d(unreal.Vector(VILLAGE_SIZE_M, VILLAGE_SIZE_M, 1.0))
    save_map(world, ENV_MAP_PATH)
    unreal.log("{} Created {}".format(LOG, ENV_MAP_PATH))


def configure_lighting():
    # Late-morning sun: fully dynamic, matching the project's Lumen / no-static-lighting setup.
    sun = spawn_actor(unreal.DirectionalLight, "Sun", (0.0, 0.0, 1000.0),
                      unreal.Rotator(roll=0.0, pitch=-45.0, yaw=35.0))
    sun_component = sun.get_editor_property("directional_light_component")
    sun_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun_component.set_editor_property("atmosphere_sun_light", True)

    sky_light = spawn_actor(unreal.SkyLight, "SkyLight", (0.0, 0.0, 1000.0))
    sky_component = sky_light.get_editor_property("light_component")
    sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    # Captured once at load: the sun never moves, so a real-time capture would only cost GPU time.
    sky_component.set_editor_property("real_time_capture", False)

    lighting = [
        sun,
        sky_light,
        spawn_actor(unreal.SkyAtmosphere, "SkyAtmosphere"),
        spawn_actor(unreal.VolumetricCloud, "VolumetricCloud"),
        spawn_actor(unreal.ExponentialHeightFog, "ExponentialHeightFog"),
    ]
    for actor in lighting:
        actor.set_folder_path("Lighting")


def add_environment_instance(env_world):
    instance = spawn_actor(unreal.LevelInstance, "LI_Village_Env")
    instance.set_folder_path("Environment")
    instance.set_editor_property("world_asset", env_world)
    return instance


def add_navigation_bounds():
    # Default volume brush is a 200 uu cube, so scale = size_cm / 200.
    nav = spawn_actor(unreal.NavMeshBoundsVolume, "NavMeshBounds", (0.0, 0.0, 0.0))
    nav.set_folder_path("Gameplay")
    nav.set_actor_scale3d(unreal.Vector(
        VILLAGE_SIZE_M * 100.0 / 200.0,
        VILLAGE_SIZE_M * 100.0 / 200.0,
        NAV_HEIGHT_M * 100.0 / 200.0))
    return nav


def build_main_map():
    if unreal.EditorAssetLibrary.does_asset_exist(MAIN_MAP_PATH):
        unreal.log("{} Keeping existing {}".format(LOG, MAIN_MAP_PATH))
        return

    world = new_map(MAIN_MAP_PATH)
    configure_lighting()
    # Load after new_map: creating a blank map unloads previously loaded worlds.
    add_environment_instance(load_required(ENV_MAP_PATH))
    add_navigation_bounds()
    player_start = spawn_actor(unreal.PlayerStart, "PlayerStart", (0.0, 0.0, 100.0))
    player_start.set_folder_path("Gameplay")
    save_map(world, MAIN_MAP_PATH)
    unreal.log("{} Created {}".format(LOG, MAIN_MAP_PATH))


# Rendering budget for low-end PCs. Profiling at Medium quality showed the volumetric clouds taking
# about 45% of GPU time, and the real-time sky capture re-rendering the sky every frame.
CLOUD_SAMPLE_SCALES = {
    "view_sample_count_scale": 0.35,
    "reflection_view_sample_count_scale_value": 0.25,
    "shadow_view_sample_count_scale": 0.35,
    "shadow_reflection_view_sample_count_scale_value": 0.25,
}


def tune_lighting_actors(actors):
    """Cheaper cloud sampling and a sky light captured once (the sun never moves). True if anything changed."""
    changed = False
    for actor in actors:
        if isinstance(actor, unreal.VolumetricCloud):
            component = actor.get_editor_property("volumetric_cloud_component")
            for prop, value in CLOUD_SAMPLE_SCALES.items():
                if abs(component.get_editor_property(prop) - value) > 1e-4:
                    component.set_editor_property(prop, value)
                    changed = True
        elif isinstance(actor, unreal.SkyLight):
            component = actor.get_editor_property("light_component")
            if component.get_editor_property("real_time_capture"):
                component.set_editor_property("real_time_capture", False)
                changed = True
    return changed


def tune_main_map_lighting():
    """Apply the lighting budget to an existing Lvl_Village_Main (saved only when something changed)."""
    if not unreal.EditorAssetLibrary.does_asset_exist(MAIN_MAP_PATH):
        return
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAIN_MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAIN_MAP_PATH))
    if tune_lighting_actors(editor_actors().get_all_level_actors()):
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        save_map(world, MAIN_MAP_PATH)
        unreal.log_warning("{} Tuned lighting in {}".format(LOG, MAIN_MAP_PATH))


def main():
    master = build_master_material()
    instances = build_material_instances(master)
    build_environment_map(instances["MI_Env_Ground"])
    build_main_map()
    tune_main_map_lighting()
    unreal.log("{} Done".format(LOG))


try:
    main()
except Exception:
    unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
    raise
