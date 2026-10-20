"""Selective, section-safe Fab material upgrades for VHV interior props.

Only compatible generic wood, painted-metal, and grey-plastic slots are
replaced. Printed/story materials and all unsuitable surface types stay intact.
The script never changes slot count/order, mesh settings, geometry, or actors.
"""

import unreal


LOG = "[Interior Fab Upgrade]"
ROOT = "/Game/3D_Models/Interior"
FAB_WOOD = "/Game/Fab/Megascans/Surfaces/Wooden_Planks_wckqdbvs/Medium/wckqdbvs_tier_2/Materials/MI_wckqdbvs"
FAB_DARK_WOOD = "/Game/Fab/Megascans/Surfaces/Worn_Wooden_Planks_whnfbg3/Medium/whnfbg3_tier_2/Materials/MI_whnfbg3"
FAB_PAINTED_METAL = "/Game/Fab/Megascans/Surfaces/Scratched_Painted_Metal_tduwejdc/Medium/tduwejdc_tier_2/Materials/MI_tduwejdc"
FAB_POLYPROPYLENE = "/Game/Fab/Megascans/Surfaces/Scratched_Polypropylene_Plastic_schbehmp/Medium/schbehmp_tier_2/Materials/MI_schbehmp"


def asset_path(asset):
    return asset.get_path_name() if asset else "<None>"


def mats(prefix):
    return "/Game/3D_Models/Mats/{}.{}".format(prefix, prefix)


# mesh suffix: {material slot index: (expected existing material, Fab replacement)}
UPGRADES = {
    "SM_01_Prasert_SofaChair": {
        0: (mats("M_Dark_Wood"), FAB_DARK_WOOD),
        1: (mats("M_Warm_Wood"), FAB_WOOD),
    },
    "SM_02_Television": {
        1: (mats("M_Warm_Grey_Plastic"), FAB_POLYPROPYLENE),
        2: (mats("M_Powdercoated_Steel"), FAB_PAINTED_METAL),
    },
    "SM_05_Shoe_Rack": {
        0: (mats("M_Powdercoated_Steel"), FAB_PAINTED_METAL),
        1: (mats("M_Pale_Wood"), FAB_WOOD),
    },
    "SM_07_Chai_Wall_Calendar": {0: (mats("M_Warm_Wood"), FAB_DARK_WOOD)},
    "SM_10_Chai_Mobile_Phone": {1: (mats("M_Warm_Grey_Plastic"), FAB_POLYPROPYLENE)},
    "SM_11_Digital_Weight_Scale": {
        1: (mats("M_Warm_Grey_Plastic"), FAB_POLYPROPYLENE),
        2: (mats("M_Brushed_Steel"), FAB_PAINTED_METAL),
    },
    "SM_12_Health_Post_Exam_Bed": {1: (mats("M_Powdercoated_Steel"), FAB_PAINTED_METAL)},
    "SM_13_Glucometer": {2: (mats("M_Warm_Grey_Plastic"), FAB_POLYPROPYLENE)},
    "SM_15_Universal_Small_Table": {
        0: (mats("M_Warm_Wood"), FAB_WOOD),
        1: (mats("M_Pale_Wood"), FAB_DARK_WOOD),
    },
    "SM_17_Reusable_Noticeboard": {
        0: (mats("M_Dark_Wood"), FAB_DARK_WOOD),
        1: (mats("M_Powdercoated_Steel"), FAB_PAINTED_METAL),
    },
    "SM_19_Walking_Route_Sign": {
        0: (mats("M_Dark_Wood"), FAB_DARK_WOOD),
        1: (mats("M_Powdercoated_Steel"), FAB_PAINTED_METAL),
    },
    "SM_20_Park_Goku_Poster_Display": {
        0: (mats("M_Dark_Wood"), FAB_DARK_WOOD),
        2: (mats("M_Powdercoated_Steel"), FAB_PAINTED_METAL),
    },
}


def run():
    replacements = {path: unreal.EditorAssetLibrary.load_asset(path) for path in
                    (FAB_WOOD, FAB_DARK_WOOD, FAB_PAINTED_METAL, FAB_POLYPROPYLENE)}
    missing = [path for path, material in replacements.items() if not material]
    if missing:
        raise RuntimeError("Missing required Fab materials: {}".format(missing))

    changed_meshes = 0
    changed_slots = 0
    for mesh_name, changes in sorted(UPGRADES.items()):
        mesh = unreal.EditorAssetLibrary.load_asset("{}/{}.{}".format(ROOT, mesh_name, mesh_name))
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError("Missing interior static mesh {}".format(mesh_name))
        slots_before = list(mesh.get_editor_property("static_materials"))
        names_before = [str(slot.get_editor_property("material_slot_name")) for slot in slots_before]
        made_change = False
        for index, (expected, replacement_path) in sorted(changes.items()):
            replacement_object_path = asset_path(replacements[replacement_path])
            if index >= len(slots_before):
                raise RuntimeError("{} has no slot {}".format(mesh_name, index))
            current = asset_path(slots_before[index].get_editor_property("material_interface"))
            if current == replacement_object_path:
                continue
            if current != expected:
                raise RuntimeError("{} slot {} expected {}, found {}; refusing overwrite".format(
                    mesh_name, index, expected, current))
            mesh.set_material(index, replacements[replacement_path])
            made_change = True
            changed_slots += 1

        slots_after = list(mesh.get_editor_property("static_materials"))
        if len(slots_after) != len(slots_before):
            raise RuntimeError("{} material slot count changed".format(mesh_name))
        if [str(slot.get_editor_property("material_slot_name")) for slot in slots_after] != names_before:
            raise RuntimeError("{} material slot ordering changed".format(mesh_name))
        for index, (_, replacement_path) in changes.items():
            replacement_object_path = asset_path(replacements[replacement_path])
            if asset_path(slots_after[index].get_editor_property("material_interface")) != replacement_object_path:
                raise RuntimeError("{} slot {} was not assigned {}".format(mesh_name, index, replacement_path))
        if made_change:
            if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
                raise RuntimeError("Could not save {}".format(mesh.get_path_name()))
            changed_meshes += 1

    unreal.log("{} PASS | MeshesModified={} | SlotsReplaced={}".format(
        LOG, changed_meshes, changed_slots))


if __name__ == "__main__":
    run()
