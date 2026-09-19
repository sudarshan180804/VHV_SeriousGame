"""Prepare the empty production HBCT content workspace.

Run this script manually inside Unreal Editor with Tools > Execute Python Script
or from Unreal's Python console. It never edits existing assets and is safe to
run repeatedly.
"""

import unreal


LOG_PREFIX = "[VHV HBCT Setup]"
HBCT_ROOT = "/Game/VHV_Stuff/HBCT"

FOLDERS = (
    HBCT_ROOT,
    HBCT_ROOT + "/Quests",
    HBCT_ROOT + "/Dialogue",
    HBCT_ROOT + "/Activities",
    HBCT_ROOT + "/NPCs",
    HBCT_ROOT + "/Animations",
    HBCT_ROOT + "/World",
    HBCT_ROOT + "/World/Locations",
    HBCT_ROOT + "/World/BehaviorTargets",
    HBCT_ROOT + "/World/WorldActions",
    HBCT_ROOT + "/UI",
    HBCT_ROOT + "/Maps",
)

# (package path, asset name, Unreal Python class name)
OPTIONAL_ASSETS = (
    (HBCT_ROOT + "/Quests", "DA_QuestArc_HBCT", "VHVQuestArcData"),
    (HBCT_ROOT + "/Activities", "DA_Level_HBCT", "VHVLevelData"),
    (HBCT_ROOT + "/Animations", "DA_InstructorActions", "VHVNPCActionSet"),
)


def _new_counts():
    return {
        "folders_created": 0,
        "folders_existing": 0,
        "assets_created": 0,
        "assets_existing": 0,
        "warnings": 0,
    }


def _warn(message, counts):
    counts["warnings"] += 1
    unreal.log_warning("{} Warning: {}".format(LOG_PREFIX, message))


def _is_safe_hbct_path(path):
    return path == HBCT_ROOT or path.startswith(HBCT_ROOT + "/")


def _ensure_folder(folder_path, counts):
    if not _is_safe_hbct_path(folder_path):
        _warn("Refusing folder outside {}: {}".format(HBCT_ROOT, folder_path), counts)
        return

    if unreal.EditorAssetLibrary.does_directory_exist(folder_path):
        counts["folders_existing"] += 1
        unreal.log("{} Exists {}".format(LOG_PREFIX, folder_path))
        return

    try:
        created = unreal.EditorAssetLibrary.make_directory(folder_path)
    except Exception as error:
        _warn("Could not create folder {}: {}".format(folder_path, error), counts)
        return

    if created or unreal.EditorAssetLibrary.does_directory_exist(folder_path):
        counts["folders_created"] += 1
        unreal.log("{} Created folder {}".format(LOG_PREFIX, folder_path))
    else:
        _warn("Could not create folder {}".format(folder_path), counts)


def _ensure_data_asset(package_path, asset_name, class_name, counts):
    asset_path = "{}/{}".format(package_path, asset_name)
    if not _is_safe_hbct_path(asset_path):
        _warn("Refusing asset outside {}: {}".format(HBCT_ROOT, asset_path), counts)
        return

    asset_type = getattr(unreal, class_name, None)
    if asset_type is None:
        _warn(
            "Native class unreal.{} is not exposed; skipped {}. Folders are ready for manual authoring.".format(
                class_name, asset_path),
            counts)
        return

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        counts["assets_existing"] += 1
        unreal.log("{} Skipped existing asset {}".format(LOG_PREFIX, asset_path))
        existing_asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        expected_class = asset_type.static_class()
        if not existing_asset:
            _warn("Existing asset could not be loaded: {}".format(asset_path), counts)
        elif existing_asset.get_class() != expected_class:
            _warn(
                "Existing asset {} is {}, expected {}; it was left unchanged.".format(
                    asset_path,
                    existing_asset.get_class().get_name(),
                    expected_class.get_name()),
                counts)
        return

    try:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", asset_type)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            package_path,
            asset_type,
            factory)
    except Exception as error:
        _warn("Could not create asset {}: {}".format(asset_path, error), counts)
        return

    if not asset:
        _warn("Could not create asset {}".format(asset_path), counts)
        return

    counts["assets_created"] += 1
    unreal.log("{} Created asset {}".format(LOG_PREFIX, asset_path))
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        _warn("Created asset could not be saved: {}".format(asset_path), counts)


def setup_hbct_content():
    counts = _new_counts()

    for folder_path in FOLDERS:
        try:
            _ensure_folder(folder_path, counts)
        except Exception as error:
            _warn("Unexpected folder error for {}: {}".format(folder_path, error), counts)

    for package_path, asset_name, class_name in OPTIONAL_ASSETS:
        try:
            _ensure_data_asset(package_path, asset_name, class_name, counts)
        except Exception as error:
            _warn(
                "Unexpected asset error for {}/{}: {}".format(
                    package_path, asset_name, error),
                counts)

    unreal.log("{} Folders created: {}".format(LOG_PREFIX, counts["folders_created"]))
    unreal.log("{} Folders already existing: {}".format(LOG_PREFIX, counts["folders_existing"]))
    unreal.log("{} Assets created: {}".format(LOG_PREFIX, counts["assets_created"]))
    unreal.log("{} Assets already existing: {}".format(LOG_PREFIX, counts["assets_existing"]))
    unreal.log("{} Warnings: {}".format(LOG_PREFIX, counts["warnings"]))
    unreal.log("{} Complete".format(LOG_PREFIX))
    return counts


if __name__ == "__main__":
    setup_hbct_content()
