"""Make imported environment art lightweight.

Run after importing buildings, props, or foliage (for example from Fab or
Megascans). For every asset under ENVIRONMENT_ROOTS it:

  * caps textures at MAX_TEXTURE_SIZE (normal/mask maps at MAX_DETAIL_TEXTURE_SIZE)
  * enables Nanite on static meshes with at least NANITE_MIN_TRIANGLES triangles

Source files are untouched; only import/build settings change, so any cap can be
raised again later. Safe to rerun: already-optimized assets are skipped.
"""

import traceback
import unreal


ENVIRONMENT_ROOTS = (
    "/Game/VHV_Stuff/Environment",
    "/Game/Fab",
    "/Game/Megascans",
)
MAX_TEXTURE_SIZE = 1024
MAX_DETAIL_TEXTURE_SIZE = 512
NANITE_MIN_TRIANGLES = 2000
LOG = "[Optimize Environment]"

DETAIL_COMPRESSION = (
    unreal.TextureCompressionSettings.TC_NORMALMAP,
    unreal.TextureCompressionSettings.TC_MASKS,
)


def asset_paths():
    for root in ENVIRONMENT_ROOTS:
        if unreal.EditorAssetLibrary.does_directory_exist(root):
            for path in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
                yield path


def optimize_texture(texture):
    detail = texture.get_editor_property("compression_settings") in DETAIL_COMPRESSION
    target = MAX_DETAIL_TEXTURE_SIZE if detail else MAX_TEXTURE_SIZE
    current = texture.get_editor_property("max_texture_size")
    if current != 0 and current <= target:
        return False
    texture.set_editor_property("max_texture_size", target)
    return True


def optimize_static_mesh(mesh):
    settings = mesh.get_editor_property("nanite_settings")
    if settings.get_editor_property("enabled"):
        return False
    if mesh.get_num_triangles(0) < NANITE_MIN_TRIANGLES:
        return False
    settings.set_editor_property("enabled", True)
    mesh.set_editor_property("nanite_settings", settings)
    return True


def main():
    changed = {"textures": 0, "meshes": 0}
    scanned = 0
    for path in asset_paths():
        asset = unreal.EditorAssetLibrary.load_asset(path)
        scanned += 1
        if isinstance(asset, unreal.Texture2D):
            if optimize_texture(asset):
                changed["textures"] += 1
                unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
        elif isinstance(asset, unreal.StaticMesh):
            if optimize_static_mesh(asset):
                changed["meshes"] += 1
                unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.log_warning("{} Scanned {} assets; capped {} textures; enabled Nanite on {} meshes".format(
        LOG, scanned, changed["textures"], changed["meshes"]))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
