"""Stage the HBCT milestone-one story in Lvl_Village_Main.

Places the same HBCT_* actors (NPCs, ambient scenes, quest location volumes,
story props, opening camera hook) that author_hbct_milestone_01.py places in
Lvl_Village_Blockout, positioned around the scene anchors in village_layout.py.
Relative staging inside each scene is unchanged from the blockout map, and the
labels and tags match, so DA_QuestArc_HBCT runs in this map unmodified.

Safe to rerun: actors are found by label and moved, never duplicated. It does
not touch the HBCT data assets; author_hbct_milestone_01.py still owns those.
"""

import importlib
import traceback
import unreal

import author_hbct_milestone_01 as hbct
import village_layout as layout
import vhv_geometry

# Unreal keeps imported modules cached between runs in the editor; reload so edits are picked up.
for _module in (hbct, layout, vhv_geometry):
    importlib.reload(_module)

from vhv_geometry import smooth  # noqa: E402


MAIN_MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
LEVEL_DATA_PATH = hbct.ACTIVITY_DIR + "/DA_Level_HBCT"
QUEST_ARC_PATH = hbct.QUEST_DIR + "/DA_QuestArc_HBCT"
LOG = "[Village Story Actors]"

NPC_Z = 100.0
SALA_PLATFORM_Z = 30.0
# The HBCT story props sit about 42 cm above the ground (table height), so each set gets a table.
TABLE_TOP_Z = 44.0
PROP_MATERIAL_DIR = "/Game/VHV_Stuff/Environment/Materials"


def rotator(yaw=0.0, pitch=0.0):
    return unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw)


def at(anchor, dx, dy, z):
    return (anchor[0] + dx, anchor[1] + dy, z)


def place_npc(npc_class, label, location, yaw, participant_tag=""):
    actor = hbct.spawn_actor(npc_class, label, location, rotator(yaw))
    component = actor.get_quest_participant_component()
    component.modify()
    component.set_editor_property("quest_participation_enabled", bool(participant_tag))
    component.set_editor_property(
        "participant_tag", hbct.tag(participant_tag) if participant_tag else unreal.GameplayTag())
    actor.set_folder_path("Story/NPCs")
    return actor


def load_ambient(name):
    path = "{}/{}".format(hbct.AMBIENT_DIR, name)
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Missing {}; run author_hbct_milestone_01.py first".format(path))
    return asset


def ambient_scene(label, location, asset_name, receiver_tag, bindings):
    actor = hbct.configure_ambient_actor(label, location, load_ambient(asset_name), receiver_tag, bindings)
    actor.set_folder_path("Story/Scenes")
    return actor


def location_volume(label, location, location_tag):
    actor = hbct.configure_location(label, location, location_tag)
    actor.set_folder_path("Story/Locations")
    return actor


def story_prop(label, location, scale, material=None):
    actor = hbct.create_blockout_prop(label, location, scale)
    actor.set_folder_path("Story/Props")
    if material:
        actor.get_editor_property("static_mesh_component").set_material(
            0, unreal.load_asset("{}/{}".format(PROP_MATERIAL_DIR, material)))
    return actor


def story_table(label, anchor, x0, x1, y0, y1):
    """Low wooden table (top at TABLE_TOP_Z) under a scene's story props, so they do not float."""
    size = (x1 - x0, y1 - y0, TABLE_TOP_Z)
    actor = hbct.spawn_actor(unreal.StaticMeshActor, label,
                             at(anchor, (x0 + x1) / 2.0, (y0 + y1) / 2.0, TABLE_TOP_Z / 2.0))
    actor.set_folder_path("Story/Props")
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    component = actor.get_editor_property("static_mesh_component")
    component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    component.set_material(0, unreal.load_asset(PROP_MATERIAL_DIR + "/MI_PH_WoodDark"))
    return actor


def stage_intro(npc_class):
    a = layout.INSTRUCTOR
    instructor = place_npc(npc_class, "HBCT_Intro_Instructor", at(a, 0.0, 0.0, NPC_Z), -90.0,
                           "VHV.Participant.Instructor")
    hook = hbct.spawn_actor(unreal.TargetPoint, "HBCT_OpeningCameraHook", at(a, -40.0, -200.0, 180.0),
                            rotator(yaw=90.0, pitch=-8.0))
    hook.set_folder_path("Story/Camera")

    player_start = hbct.find_actor("PlayerStart")
    if not player_start:
        player_start = hbct.spawn_actor(unreal.PlayerStart, "PlayerStart", at(a, -40.0, -360.0, NPC_Z))
    player_start.set_actor_location(unreal.Vector(*at(a, -40.0, -360.0, NPC_Z)), False, True)
    player_start.set_actor_rotation(rotator(yaw=90.0), False)
    return instructor


def stage_market(npc_class):
    m = layout.MARKET
    # slot -> (dx, dy, yaw, label); offsets match the blockout staging around its market centre.
    slots = {
        "Buyer": (-170.0, -180.0, 0.0, "HBCT_Market_SugarDrinkShopper"),
        "Vendor": (20.0, -180.0, 180.0, "HBCT_Market_Vendor"),
        "Smoker": (-230.0, 150.0, 20.0, "HBCT_Market_Smoker"),
        "SweetTea": (20.0, 170.0, 160.0, "HBCT_Market_SweetTeaVillager"),
        "Rider": (260.0, -40.0, 90.0, "HBCT_Market_Motorbike"),
        "JunkFood": (260.0, 220.0, -90.0, "HBCT_Market_JunkFoodVillager"),
    }
    npcs = {
        slot: place_npc(npc_class, label, at(m, dx, dy, NPC_Z), yaw)
        for slot, (dx, dy, yaw, label) in slots.items()
    }
    location_volume("HBCT_Location_Market", at(m, 0.0, 0.0, NPC_Z), "VHV.Location.HBCT.Market")
    ambient_scene(
        "HBCT_Market_SugarDrinkScene", at(m, 0.0, 0.0, NPC_Z),
        "DA_Ambient_HBCT_MarketObservation", "VHV.WorldReceiver.HBCT.MarketObservation",
        [hbct.bind(slot, npcs[slot]) for slot in ("Buyer", "Vendor", "Smoker", "SweetTea", "Rider", "JunkFood")])


def stage_prasert(npc_class):
    p = layout.PRASERT
    prasert = place_npc(npc_class, "HBCT_Motivation_Prasert", at(p, 0.0, 0.0, NPC_Z), 150.0,
                        "VHV.Participant.UnclePrasert")
    # In front of the stilt house, between it and the main road.
    location_volume("HBCT_Location_PrasertHouse", at(p, 0.0, 250.0, NPC_Z), "VHV.Location.HBCT.PrasertHouse")
    ambient_scene(
        "HBCT_Motivation_PrasertScene", at(p, 0.0, 0.0, NPC_Z),
        "DA_Ambient_HBCT_PrasertObservation", "VHV.WorldReceiver.HBCT.Motivation.Prasert",
        [hbct.bind("Prasert", prasert)])
    story_table("HBCT_Prop_Prasert_Table", p, -70.0, 155.0, 45.0, 110.0)
    story_prop("HBCT_Prop_Prasert_TV", at(p, 110.0, 90.0, 70.0), (0.65, 0.12, 0.45), "MI_PH_Metal")
    story_prop("HBCT_Prop_Prasert_SweetDrink", at(p, -50.0, 60.0, 55.0), (0.08, 0.08, 0.25), "MI_PH_BikeRed")
    story_prop("HBCT_Prop_Prasert_SnackPackets", at(p, -15.0, 85.0, 48.0), (0.25, 0.16, 0.06), "MI_PH_Canvas")


def stage_demo(npc_class):
    d = layout.DEMO
    z = SALA_PLATFORM_Z + NPC_Z
    directive_vhv = place_npc(npc_class, "HBCT_Motivation_DirectiveVHV", at(d, -85.0, -140.0, z), 0.0)
    directive_patient = place_npc(npc_class, "HBCT_Motivation_DirectivePatient", at(d, 85.0, -140.0, z), 180.0)
    motivational_vhv = place_npc(npc_class, "HBCT_Motivation_MotivationalVHV", at(d, -85.0, 140.0, z), 0.0)
    motivational_patient = place_npc(npc_class, "HBCT_Motivation_MotivationalPatient", at(d, 85.0, 140.0, z), 180.0)
    ambient_scene(
        "HBCT_Motivation_BadGoodDemo", at(d, 0.0, 0.0, z),
        "DA_Ambient_HBCT_BadGoodDemo", "VHV.WorldReceiver.HBCT.Motivation.BadGoodDemo",
        [hbct.bind("DirectiveVHV", directive_vhv), hbct.bind("DirectivePatient", directive_patient),
         hbct.bind("MotivationalVHV", motivational_vhv), hbct.bind("MotivationalPatient", motivational_patient)])
    # Low boards marking the two halves of the demo: red for directive, green for motivational.
    story_prop("HBCT_Motivation_BadDemo", at(d, 0.0, -230.0, SALA_PLATFORM_Z + 20.0), (0.9, 0.06, 0.2), "MI_PH_RoofRed")
    story_prop("HBCT_Motivation_GoodDemo", at(d, 0.0, 230.0, SALA_PLATFORM_Z + 20.0), (0.9, 0.06, 0.2), "MI_PH_Sign")


def stage_saeng(npc_class):
    s = layout.SAENG
    saeng = place_npc(npc_class, "HBCT_Motivation_Saeng", at(s, 100.0, 0.0, NPC_Z), 180.0,
                      "VHV.Participant.AuntSaeng")
    saeng_vhv = place_npc(npc_class, "HBCT_Motivation_SaengVHV", at(s, -90.0, 0.0, NPC_Z), 0.0)
    # Over the front yard and towards the road, so a player on the village walk passes through it.
    location_volume("HBCT_Location_SaengHouse", at(s, 0.0, 60.0, NPC_Z), "VHV.Location.HBCT.SaengHouse")
    ambient_scene(
        "HBCT_Motivation_SaengConversation", at(s, 0.0, 0.0, NPC_Z),
        "DA_Ambient_HBCT_SaengAppreciation", "VHV.WorldReceiver.HBCT.Motivation.Saeng",
        [hbct.bind("Saeng", saeng), hbct.bind("VHV", saeng_vhv)])


def stage_mali(npc_class):
    m = layout.MALI
    place_npc(npc_class, "HBCT_Motivation_Mali", at(m, 0.0, 0.0, NPC_Z), 180.0, "VHV.Participant.AuntMali")
    location_volume("HBCT_Location_MaliHouse", at(m, -190.0, 0.0, NPC_Z), "VHV.Location.HBCT.MaliHouse")
    # Table behind Mali (she faces west, the player arrives from the west), clear of the talk spot.
    story_table("HBCT_Prop_Mali_Table", m, 40.0, 120.0, -75.0, 10.0)
    story_prop("HBCT_Prop_Mali_IcedMilkTea", at(m, 60.0, -50.0, 55.0), (0.08, 0.08, 0.25), "MI_PH_Skirting")
    story_prop("HBCT_Prop_Mali_Sweets", at(m, 90.0, -15.0, 48.0), (0.28, 0.18, 0.06), "MI_PH_RoofOrange")


def validate_walk_through_locations():
    """Every Reach Location volume must sit on the village walk, so following the path completes it.

    Requires the walk's centreline to enter each box (a player on either half of the path then overlaps
    it). Raises before the map is saved, naming the volume to move the VILLAGE_WALK points towards.
    """
    walk = smooth(layout.VILLAGE_WALK)
    samples = [walk[-1]]
    for (ax, ay), (bx, by) in zip(walk, walk[1:]):
        samples.extend((ax + (bx - ax) * k / 10.0, ay + (by - ay) * k / 10.0) for k in range(10))
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if not isinstance(actor, unreal.VHVQuestLocationVolume):
            continue
        centre = actor.get_actor_location()
        extent = actor.get_editor_property("box_component").get_scaled_box_extent()
        if not any(abs(x - centre.x) <= extent.x and abs(y - centre.y) <= extent.y for x, y in samples):
            raise RuntimeError("{} is not on the village walk; move the VILLAGE_WALK points in "
                               "village_layout.py through it".format(actor.get_actor_label()))


def main():
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAIN_MAP_PATH):
        raise RuntimeError("Could not load {}; run setup_village_main_map.py first".format(MAIN_MAP_PATH))

    npc_bp = unreal.EditorAssetLibrary.load_asset(hbct.NPC_BP_PATH)
    if not npc_bp:
        raise RuntimeError("Missing {}".format(hbct.NPC_BP_PATH))
    npc_class = npc_bp.generated_class()

    stage_intro(npc_class)
    stage_market(npc_class)
    stage_prasert(npc_class)
    stage_demo(npc_class)
    stage_saeng(npc_class)
    stage_mali(npc_class)
    validate_walk_through_locations()

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAIN_MAP_PATH):
        raise RuntimeError("Could not save {}".format(MAIN_MAP_PATH))

    # Same checks the blockout map passes: required labels, unique participant
    # tags, actor data validation, and the player controller's quest defaults.
    level = unreal.EditorAssetLibrary.load_asset(LEVEL_DATA_PATH)
    arc = unreal.EditorAssetLibrary.load_asset(QUEST_ARC_PATH)
    hbct.validate_map_and_defaults(level, arc)
    unreal.log("{} Staged HBCT milestone one in {}".format(LOG, MAIN_MAP_PATH))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
