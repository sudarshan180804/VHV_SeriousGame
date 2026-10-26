"""Read-only production inventory for the VHV MegaPlant PVE conversion.

Run from UE 5.7's Python console or with -ExecutePythonScript.  This script
loads only the production map, writes JSON/Markdown reports under Saved, and
does not save packages or modify actors/assets.
"""

import hashlib
import json
import os
import re
import traceback

import unreal


MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
REPORT_DIR = os.path.join(unreal.SystemLibrary.get_project_directory(), "Saved", "Reports")
JSON_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_PVE_Inventory.json")
MD_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_PVE_Conversion.md")
LOG = "[VHV Static Vegetation Inventory]"
TARGET_FOLDER = "/Game/VHV_StaticVegetation"

SPECIES = {
    "Japanese_Medlar": {
        "pve": "/Game/Megaplant_Library/Tree_Japanese_Medlar/Tree_Japanese_Medlar_01/PVE_Japanese_Medlar_01",
        "kind": "tree",
    },
    "Beach_Rose": {
        "pve": "/Game/Megaplant_Library/Shrub_Beach_Rose/Shrub_Beach_Rose_01/PVE_Beach_Rose_01",
        "kind": "shrub",
    },
    "Common_Hazel": {
        "pve": "/Game/Megaplant_Library/Tree_Common_Hazel/Tree_Common_Hazel_01/PVE_Common_Hazel_01",
        "kind": "tree",
    },
    "Goat_Willow": {
        "pve": "/Game/Megaplant_Library/Tree_Goat_Willow/Tree_Goat_Willow_01/PVE_Goat_Willow_01",
        "kind": "tree",
    },
    "Greasewood": {
        "pve": "/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/PVE_Greasewood_01",
        "kind": "shrub",
    },
    "Black_Alder": {
        "pve": "/Game/Megaplant_Library/Tree_Black_Alder/Tree_Black_Alder_01/PVE_Black_Alder_01",
        "kind": "tree",
    },
}


def _value(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def _path(obj):
    try:
        return obj.get_path_name() if obj else ""
    except Exception:
        return ""


def _vec(value):
    if value is None:
        return None
    return [round(float(value.x), 6), round(float(value.y), 6), round(float(value.z), 6)]


def _rot(value):
    if value is None:
        return None
    return [round(float(value.pitch), 6), round(float(value.yaw), 6), round(float(value.roll), 6)]


def _enum(value):
    return str(value) if value is not None else ""


def _mesh_for(component):
    mesh = _value(component, "skeletal_mesh")
    if mesh is None:
        try:
            mesh = component.get_skeletal_mesh_asset()
        except Exception:
            mesh = None
    return mesh


def _species_variant(mesh_path):
    name = mesh_path.rsplit("/", 1)[-1].split(".", 1)[0]
    for species in SPECIES:
        match = re.match(r"^SK_{}(?:_01)?_([ABCD])(?:_Skeleton)?$".format(re.escape(species)), name)
        if match:
            return species, match.group(1), name
    return None, None, name


def _component_snapshot(component):
    mesh = _mesh_for(component)
    try:
        collision_enabled = str(component.get_collision_enabled())
    except Exception:
        collision_enabled = ""
    try:
        collision_profile = str(component.get_collision_profile_name())
    except Exception:
        collision_profile = str(_value(component, "collision_profile_name", ""))
    return {
        "component_name": component.get_name(),
        "skeletal_mesh": _path(mesh),
        "collision_enabled": collision_enabled,
        "collision_profile": collision_profile,
        "generate_overlap_events": bool(_value(component, "generate_overlap_events", False)),
        "cast_shadow": bool(_value(component, "cast_shadow", False)),
        "cast_dynamic_shadow": bool(_value(component, "cast_dynamic_shadow", False)),
        "cast_static_shadow": bool(_value(component, "cast_static_shadow", False)),
        "cast_contact_shadow": bool(_value(component, "cast_contact_shadow", False)),
        "visible": bool(_value(component, "visible", True)),
        "hidden_in_game": bool(_value(component, "hidden_in_game", False)),
        "mobility": _enum(_value(component, "mobility")),
        "render_in_main_pass": bool(_value(component, "render_in_main_pass", True)),
        "render_in_depth_pass": bool(_value(component, "render_in_depth_pass", True)),
        "use_as_occluder": bool(_value(component, "use_as_occluder", True)),
        "receives_decals": bool(_value(component, "receives_decals", True)),
        "material_slots": [_path(item) for item in list(component.get_materials())],
        "bounds_origin": _vec(component.bounds.origin) if getattr(component, "bounds", None) else None,
        "bounds_extent": _vec(component.bounds.box_extent) if getattr(component, "bounds", None) else None,
    }


def _file_hash(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _map_file_record():
    project = unreal.SystemLibrary.get_project_directory()
    map_file = os.path.join(project, "Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
    if not os.path.isfile(map_file):
        raise RuntimeError("Production map package is missing: {}".format(map_file))
    return {"path": map_file, "bytes": os.path.getsize(map_file), "sha256": _file_hash(map_file)}


def _asset_exists(path):
    return bool(path and unreal.EditorAssetLibrary.does_asset_exist(path))


def _scan_static_targets():
    """Scan exactly the requested folder and index recognized A-D variants."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = list(registry.get_assets_by_path(unreal.Name(TARGET_FOLDER), recursive=False))
    recognized = {}
    unrecognized = []
    duplicates = []
    for asset_data in assets:
        name = str(asset_data.asset_name)
        match = re.match(r"^SK_(.+)_01_([ABCD])$", name)
        species = match.group(1) if match else None
        variant = match.group(2) if match else None
        record = {
            "asset_name": name,
            "package_name": str(asset_data.package_name),
            "object_path": "{}.{}".format(str(asset_data.package_name), name),
            "asset_class": str(asset_data.asset_class_path),
        }
        if not match or species not in SPECIES:
            unrecognized.append(record)
            continue
        key = "{}:{}".format(species, variant)
        if key in recognized:
            duplicates.append(record)
        else:
            recognized[key] = record
    expected = {"{}:{}".format(species, variant) for species in SPECIES for variant in "ABCD"}
    return {
        "folder": TARGET_FOLDER,
        "asset_count": len(assets),
        "recognized": recognized,
        "unrecognized": unrecognized,
        "duplicates": duplicates,
        "missing_keys": sorted(expected - set(recognized)),
        "complete": len(assets) == 24 and set(recognized) == expected and not unrecognized and not duplicates,
    }


def _candidate_mapping(species, variant, source_mesh, target_scan):
    target_record = target_scan["recognized"].get("{}:{}".format(species, variant), {})
    target = target_record.get("package_name", "")
    return {
        "species": species,
        "variant": variant,
        "source_skeletal_mesh": source_mesh,
        "source_pve_graph": SPECIES[species]["pve"],
        "target_static_mesh": target,
        "target_asset_name": target_record.get("asset_name", ""),
        "target_asset_class": target_record.get("asset_class", ""),
        "target_exists": _asset_exists(target),
        "export_status": "not_generated" if not _asset_exists(target) else "awaiting_validation",
    }


def _actor_snapshot(actor):
    components = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
    eligible_components = []
    for component in components:
        snapshot = _component_snapshot(component)
        species, variant, mesh_name = _species_variant(snapshot["skeletal_mesh"])
        if species:
            snapshot["species"] = species
            snapshot["variant"] = variant
            snapshot["source_mesh_name"] = mesh_name
            snapshot["pve_graph"] = SPECIES[species]["pve"]
            eligible_components.append(snapshot)
    if not eligible_components:
        return None
    parent = actor.get_attach_parent_actor()
    return {
        "object_path": actor.get_path_name(),
        "name": actor.get_name(),
        "label": actor.get_actor_label(),
        "class": actor.get_class().get_path_name(),
        "folder": str(actor.get_folder_path()),
        "location": _vec(actor.get_actor_location()),
        "rotation": _rot(actor.get_actor_rotation()),
        "scale": _vec(actor.get_actor_scale3d()),
        "tags": sorted(str(tag) for tag in list(_value(actor, "tags", []))),
        "hidden": bool(_value(actor, "hidden", False)),
        "actor_enable_collision": bool(_value(actor, "actor_enable_collision", True)),
        "attach_parent": _path(parent),
        "eligible_components": eligible_components,
        "direct_actor_references": "not_enumerated: UE Python exposes no safe actor-reference graph query",
    }


def run():
    os.makedirs(REPORT_DIR, exist_ok=True)
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world or world.get_path_name().split(".", 1)[0] != MAP_PATH:
        raise RuntimeError("Loaded world does not match {}".format(MAP_PATH))
    actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    inventory = [item for item in (_actor_snapshot(actor) for actor in actors) if item]
    inventory.sort(key=lambda item: (item["label"].lower(), item["object_path"].lower()))
    target_scan = _scan_static_targets()
    mappings = {}
    for actor in inventory:
        for component in actor["eligible_components"]:
            mapping = _candidate_mapping(component["species"], component["variant"], component["skeletal_mesh"], target_scan)
            mappings[mapping["source_skeletal_mesh"]] = mapping
    by_species = {}
    for actor in inventory:
        for component in actor["eligible_components"]:
            by_species.setdefault(component["species"], 0)
            by_species[component["species"]] += 1
    report = {
        "schema": 2,
        "map": MAP_PATH,
        "map_file": _map_file_record(),
        "total_loaded_level_actors": len(actors),
        "eligible_megaplant_actor_count": len(inventory),
        "eligible_megaplant_component_count": sum(len(item["eligible_components"]) for item in inventory),
        "by_species": by_species,
        "actors": inventory,
        "source_to_target_mapping": sorted(mappings.values(), key=lambda item: item["source_skeletal_mesh"]),
        "target_folder_scan": target_scan,
        "world_partition": {
            "external_actor_directory": "absent for this map on disk",
            "unloaded_actors": "not applicable unless this map is later converted to World Partition",
        },
        "notes": [
            "Read-only inventory: no packages or actor state were saved.",
            "Only SkeletalMeshComponents matching known MegaPlant source naming were eligible.",
            "The PVE graph is authoritative; no skeletal-to-static conversion path is used.",
        ],
    }
    with open(JSON_PATH, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, sort_keys=True)
    lines = [
        "# VHV Static Vegetation PVE Conversion",
        "",
        "## Read-only inventory",
        "",
        "- Map: `{}`".format(MAP_PATH),
        "- Total loaded level actors: {}".format(len(actors)),
        "- Eligible MegaPlant actors: {}".format(len(inventory)),
        "- Eligible MegaPlant skeletal components: {}".format(report["eligible_megaplant_component_count"]),
        "- Map SHA-256: `{}`".format(report["map_file"]["sha256"]),
        "- Target folder scan: {} assets; complete={}".format(target_scan["asset_count"], target_scan["complete"]),
        "",
        "| Species | Eligible components | PVE graph |",
        "|---|---:|---|",
    ]
    for species in sorted(by_species):
        lines.append("| {} | {} | `{}` |".format(species, by_species[species], SPECIES[species]["pve"]))
    lines.extend([
        "",
        "## PVE export automation finding",
        "",
        "UE 5.7's ProceduralVegetationEditor exporter is editor-private C++ (`PV::Export::ExportCollectionAsMesh`) and has no reflected Python/Blueprint API. No unsupported automation call was attempted. Use the exact manual export list in the JSON inventory; all subsequent validation and replacement steps are scripted.",
        "",
        "## Safety status",
        "",
        "Production replacement is blocked until a verified checkpoint, direct-PVE static assets, per-variant validation, and matched performance evidence exist.",
        "",
        "Machine-readable inventory: `Saved/Reports/VHV_StaticVegetation_PVE_Inventory.json`.",
    ])
    with open(MD_PATH, "w", encoding="utf-8") as stream:
        stream.write("\n".join(lines) + "\n")
    unreal.log("{} wrote {} eligible components across {} actors".format(
        LOG, report["eligible_megaplant_component_count"], len(inventory)))
    return report


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} FAILED:\n{}".format(LOG, traceback.format_exc()))
        raise
