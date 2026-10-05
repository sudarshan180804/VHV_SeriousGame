"""Create TEMPORARY placeholder models for the village.

Each model is one static mesh asset in /Game/VHV_Stuff/Environment/Placeholders.
Every copy in the village references that one asset, so replacing a model is:

    Content Browser -> right-click the SM_* asset -> Reimport with New File
    (or import your FBX with the same name and choose Replace)

and every house, tree, or stall using it updates. See Docs/VHV_LEVEL_DESIGN_GUIDE.md.

A model is only generated when its asset does not exist, so this script never
overwrites a model you have replaced. Delete an asset and rerun to regenerate it.
Model conventions: centimetres, origin at the centre of the footprint on the
ground, front (door side) facing -Y.
"""

import importlib
import math
import traceback
import unreal

import vhv_meshkit

# Unreal keeps imported modules cached between runs in the editor; reload so edits are picked up.
importlib.reload(vhv_meshkit)

from vhv_meshkit import MeshKit, ensure_material_instance  # noqa: E402


PLACEHOLDER_DIR = "/Game/VHV_Stuff/Environment/Placeholders"
LOG = "[Placeholder Models]"

# Slot name -> (sRGB colour, roughness). Materials are MI_PH_<slot> instances of M_Env_Master.
PALETTE = {
    "WoodPlank": ((190, 110, 58), 0.8),
    "WoodDark": ((96, 52, 26), 0.8),
    "RoofOrange": ((222, 118, 88), 0.6),
    "RoofRed": ((170, 62, 52), 0.6),
    "RoofMetal": ((140, 150, 155), 0.5),
    "Plaster": ((232, 226, 208), 0.9),
    "PlasterTrim": ((196, 194, 184), 0.9),
    "Skirting": ((226, 140, 112), 0.9),
    "Concrete": ((150, 150, 146), 0.95),
    "Glass": ((44, 92, 96), 0.15),
    "DoorWood": ((150, 80, 40), 0.7),
    "Metal": ((40, 40, 42), 0.5),
    "Sign": ((38, 120, 82), 0.7),
    "Canvas": ((58, 110, 170), 0.9),
    "Leaf": ((62, 110, 40), 0.9),
    "PalmLeaf": ((92, 128, 52), 0.9),
    "Bark": ((100, 78, 58), 0.95),
    "PalmBark": ((130, 112, 88), 0.95),
    "Clay": ((160, 86, 50), 0.8),
    "BikeRed": ((170, 30, 30), 0.4),
    "Soil": ((90, 66, 44), 1.0),
    "PaintWhite": ((235, 235, 228), 0.6),
    "PaintRed": ((196, 38, 34), 0.6),
}


def materials():
    return {slot: ensure_material_instance("MI_PH_" + slot, srgb, roughness)
            for slot, (srgb, roughness) in PALETTE.items()}


# ---------------------------------------------------------------------------
# Houses (reference photos: wooden house on posts, concrete house with veranda)
# ---------------------------------------------------------------------------

def house_wooden(k):
    hx, hy = 350.0, 250.0
    floor_z, eave_z = 120.0, 380.0
    porch_x = -hx - 160.0
    for x in (-335.0, -110.0, 110.0, 335.0, porch_x + 10.0):
        for y in (-235.0, 0.0, 235.0):
            k.box_from("Concrete", x - 15, x + 15, y - 15, y + 15, 0, 20)
            k.box_from("WoodDark", x - 7.5, x + 7.5, y - 7.5, y + 7.5, 20, floor_z - 15)
    k.box_from("WoodDark", -hx - 5, hx + 5, -hy - 5, hy + 5, floor_z - 15, floor_z)
    k.box_from("WoodPlank", porch_x, -hx - 5, -hy, hy, floor_z - 12, floor_z - 2)
    # Plank walls with battens and trim.
    k.box_from("WoodPlank", -hx, hx, -hy, hy, floor_z, eave_z)
    x = -hx + 90.0
    while x < hx - 20.0:
        for wy, out in ((-hy, -1.0), (hy, 1.0)):
            k.box_from("WoodDark", x - 2, x + 2, wy, wy + out * 2, floor_z, eave_z)
        x += 90.0
    y = -hy + 90.0
    while y < hy - 20.0:
        for wx, out in ((-hx, -1.0), (hx, 1.0)):
            k.box_from("WoodDark", wx, wx + out * 2, y - 2, y + 2, floor_z, eave_z)
        y += 90.0
    for z0, z1 in ((floor_z, floor_z + 12), (eave_z - 10, eave_z)):
        k.box_from("WoodDark", -hx - 3, hx + 3, -hy - 3, hy + 3, z0, z1)
    # Door, windows.
    door_x = -160.0
    k.box_from("WoodDark", door_x - 52, door_x + 52, -hy - 5, -hy, floor_z, floor_z + 212)
    k.box_from("WoodPlank", door_x - 44, door_x + 44, -hy - 7, -hy - 5, floor_z + 2, floor_z + 204)
    for wall, pos, width in ((("y", -hy, -1.0), 30.0, 100.0), (("y", hy, 1.0), -160.0, 100.0),
                             (("y", hy, 1.0), 160.0, 100.0), (("x", hx, 1.0), -110.0, 80.0),
                             (("x", hx, 1.0), 110.0, 80.0)):
        k.window(wall, pos, floor_z + 90, width, 110, "WoodDark", "Glass", mullion=True)
    # Side porch: posts to the roof, railing with balusters.
    rail_z = floor_z + 90.0
    for y in (-hy + 5, hy - 5):
        k.box_from("WoodDark", porch_x, porch_x + 12, y - 6, y + 6, 20, eave_z)
        k.box_from("WoodDark", porch_x, -hx, y - 3, y + 3, rail_z - 5, rail_z)
    k.box_from("WoodDark", porch_x, porch_x + 6, -hy, hy, rail_z - 5, rail_z)
    y = -hy + 20.0
    while y < hy - 10.0:
        k.box_from("WoodDark", porch_x + 1, porch_x + 5, y - 2, y + 2, floor_z, rail_z)
        y += 18.0
    # Boxed stairs with railings.
    steps, tread, width = 8, 25.0, 100.0
    rise = floor_z / steps
    for i in range(steps):
        top = rise * (i + 1)
        y_near = -hy - (steps - i) * tread
        k.box_from("WoodPlank", door_x - width / 2, door_x + width / 2, y_near, y_near + tread, 0, top)
        k.box_from("WoodDark", door_x - width / 2, door_x + width / 2, y_near - 2, y_near + tread, top - 3, top)
    foot_y = -hy - steps * tread
    for side in (-1.0, 1.0):
        x = door_x + side * (width / 2 + 4)
        k.box_from("WoodDark", x - 5, x + 5, foot_y, foot_y + 10, 0, 100)
        k.box_from("WoodDark", x - 5, x + 5, -hy - 10, -hy, 0, floor_z + 95)
        k.beam("WoodDark", (x, foot_y + 5, 100), (x, -hy - 5, floor_z + 95), 6)
        for i in range(1, steps):
            by, bz = foot_y + i * tread, rise * i
            k.box_from("WoodDark", x - 2, x + 2, by - 2, by + 2, bz, bz + 95)
    # Orange gable roof over house and porch, wooden gables, gutter and downpipe.
    ridge = k.gable_roof("RoofOrange", porch_x - 30, hx + 45, hy, eave_z, 30.0, 60.0, fascia="WoodDark")
    k.gable_fill("WoodPlank", hx, 1.0, hy, eave_z, ridge)
    k.gable_fill("WoodPlank", -hx, -1.0, hy, eave_z, ridge)
    gutter_z = eave_z - 60.0 * 0.577 - 12.0
    k.box_from("Metal", porch_x - 30, hx + 45, -hy - 70, -hy - 60, gutter_z, gutter_z + 10)
    k.cylinder("Metal", (hx - 15, -hy - 65, 0), 4, gutter_z, steps=8)


def house_concrete(k):
    hx, hy = 450.0, 275.0
    base_z, eave_z = 20.0, 320.0
    veranda_y = -hy - 190.0
    k.box_from("Concrete", -hx - 20, hx + 20, veranda_y - 10, hy + 20, 0, base_z)
    k.box_from("Plaster", -hx, hx, -hy, hy, base_z, eave_z)
    k.box_from("Skirting", -hx - 2, hx + 2, -hy - 2, hy + 2, base_z, base_z + 60)
    for x in (-hx, hx):
        for y in (-hy, hy):
            k.box_from("PlasterTrim", x - 14, x + 14, y - 14, y + 14, base_z, eave_z)
    door_x = -100.0
    k.box_from("WoodDark", door_x - 55, door_x + 55, -hy - 5, -hy, base_z, base_z + 228)
    k.box_from("DoorWood", door_x - 47, door_x + 47, -hy - 7, -hy - 5, base_z, base_z + 220)
    for z in (35.0, 100.0, 165.0):
        k.box_from("WoodDark", door_x - 34, door_x + 34, -hy - 9, -hy - 7, base_z + z, base_z + z + 45)
    front, back = ("y", -hy, -1.0), ("y", hy, 1.0)
    right, left = ("x", hx, 1.0), ("x", -hx, -1.0)
    k.window(front, -300.0, base_z + 100, 130, 130, "Metal", "Glass", bars=5, crossbars=3)
    k.window(front, 190.0, base_z + 100, 180, 130, "Metal", "Glass", bars=7, crossbars=3)
    k.window(back, -220.0, base_z + 100, 130, 130, "Metal", "Glass", bars=5, crossbars=3)
    k.window(back, 220.0, base_z + 100, 130, 130, "Metal", "Glass", bars=5, crossbars=3)
    k.window(right, 90.0, base_z + 100, 60, 130, "Metal", "Glass", bars=3, crossbars=2)
    k.window(right, -60.0, base_z + 180, 40, 60, "Metal", "Glass", bars=1)
    k.window(left, 0.0, base_z + 100, 80, 130, "Metal", "Glass", bars=3, crossbars=2)
    # Veranda columns, beam, lean-to roof.
    column_y = veranda_y + 25.0
    veranda_top = eave_z - 45.0
    for x in (-hx + 5, -160.0, 140.0, hx - 5):
        k.box_from("PlasterTrim", x - 11, x + 11, column_y - 11, column_y + 11, base_z, veranda_top)
    k.box_from("PlasterTrim", -hx - 10, hx + 10, column_y - 10, column_y + 10, veranda_top - 12, veranda_top)
    under_eave_z = eave_z - 50.0 * 0.466 - 8.0
    k.prism_x("RoofRed", [(-hy - 35, under_eave_z), (veranda_y - 30, veranda_top - 5),
                          (veranda_y - 30, veranda_top + 1), (-hy - 35, under_eave_z + 6)], -hx - 40, hx + 40)
    # Bench on the veranda.
    bx, by = 40.0, -hy - 45.0
    k.box_from("DoorWood", bx - 65, bx + 65, by - 17, by + 17, base_z + 40, base_z + 45)
    for x in (bx - 50, bx + 50):
        k.box_from("WoodDark", x - 4, x + 4, by - 14, by + 14, base_z, base_z + 40)
    ridge = k.gable_roof("RoofRed", -hx - 40, hx + 40, hy, eave_z, 25.0, 50.0, fascia="PlasterTrim")
    k.gable_fill("Plaster", hx, 1.0, hy, eave_z, ridge)
    k.gable_fill("Plaster", -hx, -1.0, hy, eave_z, ridge)


# ---------------------------------------------------------------------------
# Public buildings
# ---------------------------------------------------------------------------

def health_centre(k):
    """Sub-district health centre: long single-storey building, door canopy, green sign."""
    hx, hy = 900.0, 600.0
    base_z, top_z = 20.0, 400.0
    k.box_from("Concrete", -hx - 30, hx + 30, -hy - 250, hy + 30, 0, base_z)
    k.box_from("Plaster", -hx, hx, -hy, hy, base_z, top_z)
    k.box_from("Skirting", -hx - 2, hx + 2, -hy - 2, hy + 2, base_z, base_z + 70)
    k.box_from("PlasterTrim", -hx - 40, hx + 40, -hy - 40, hy + 40, top_z, top_z + 25)
    k.box_from("PlasterTrim", -hx - 40, hx + 40, -hy - 40, -hy - 30, top_z + 25, top_z + 70)
    front = ("y", -hy, -1.0)
    k.box_from("Metal", -120, 120, -hy - 5, -hy, base_z, base_z + 240)
    k.box_from("Glass", -110, 110, -hy - 7, -hy - 5, base_z + 5, base_z + 235)
    for x in (-700.0, -450.0, -250.0, 250.0, 450.0, 700.0):
        k.window(front, x, base_z + 100, 150, 140, "Metal", "Glass", bars=3, crossbars=1)
    for x in (-600.0, -200.0, 200.0, 600.0):
        k.window(("y", hy, 1.0), x, base_z + 100, 150, 140, "Metal", "Glass", bars=3, crossbars=1)
    k.box_from("Sign", -250, 250, -hy - 45, -hy - 35, top_z - 10, top_z + 60)
    # Entrance canopy on two columns.
    for x in (-200.0, 200.0):
        k.box_from("PlasterTrim", x - 12, x + 12, -hy - 230, -hy - 206, base_z, 300)
    k.box_from("RoofRed", -260, 260, -hy - 250, -hy, 300, 315)


def community_sala(k):
    """Open pavilion: raised platform, pillars, red gable roof."""
    hx, hy = 500.0, 400.0
    k.box_from("WoodPlank", -hx, hx, -hy, hy, 0, 30)
    for x in (-hx + 20, 0.0, hx - 20):
        for y in (-hy + 20, hy - 20):
            k.box_from("WoodDark", x - 12, x + 12, y - 12, y + 12, 30, 330)
    ridge = k.gable_roof("RoofRed", -hx - 60, hx + 60, hy, 330.0, 35.0, 70.0, fascia="WoodDark")
    k.gable_fill("WoodDark", hx + 60, 1.0, hy, 330.0, ridge, thickness=4.0)
    k.gable_fill("WoodDark", -hx - 60, -1.0, hy, 330.0, ridge, thickness=4.0)


def market_stall(k):
    w, d = 250.0, 150.0
    for x in (-w / 2 + 8, w / 2 - 8):
        for y in (-d / 2 + 8, d / 2 - 8):
            k.box_from("WoodDark", x - 4, x + 4, y - 4, y + 4, 0, 220)
    k.box_from("WoodPlank", -w / 2 + 10, w / 2 - 10, -d / 2 + 10, d / 2 - 10, 85, 93)
    k.box("Canvas", (0, 0, 228), (w + 40, d + 40, 6), roll=-6.0)
    for i, x in enumerate((-80.0, -30.0, 20.0, 70.0)):
        k.box("Clay" if i % 2 else "Leaf", (x, 0, 105), (35, 35, 24))


def shed(k):
    k.box_from("WoodDark", -125, 125, -100, 100, 0, 210)
    k.box_from("WoodPlank", -40, 40, -103, -100, 0, 180)
    k.box("RoofMetal", (0, 0, 225), (290, 250, 5), roll=8.0)


# ---------------------------------------------------------------------------
# Vegetation and props
# ---------------------------------------------------------------------------

def tree_palm(k):
    k.cone("PalmBark", (0, 0, 0), 18, 11, 820, steps=8)
    crown = 820.0
    for i in range(9):
        yaw = i * 40.0
        pitch = -28.0 if i % 2 else -15.0
        length = 380.0
        cx = math.cos(math.radians(yaw)) * math.cos(math.radians(pitch)) * length / 2
        cy = math.sin(math.radians(yaw)) * math.cos(math.radians(pitch)) * length / 2
        cz = crown + math.sin(math.radians(pitch)) * length / 2
        k.box("PalmLeaf", (cx, cy, cz), (length, 70, 3), yaw=yaw, pitch=pitch)
    for i in range(4):
        a = math.radians(i * 90 + 20)
        k.sphere("Clay", (math.cos(a) * 18, math.sin(a) * 18, crown - 25), 13, steps=6)


def tree_mango(k):
    k.cone("Bark", (0, 0, 0), 28, 16, 330, steps=8)
    for x, y, z, r in ((0, 0, 470, 240), (130, 60, 400, 170), (-120, -80, 410, 180), (40, -140, 380, 150)):
        k.sphere("Leaf", (x, y, z), r, scale=(1.0, 1.0, 0.8), steps=8)


def plant_banana(k):
    for sx, sy, h in ((0, 0, 200), (45, 25, 160), (-35, 30, 140)):
        k.cylinder("PalmLeaf", (sx, sy, 0), 9, h, steps=8)
        for i in range(5):
            yaw = i * 72.0 + sx
            length = 170.0
            cx = sx + math.cos(math.radians(yaw)) * length / 2 * 0.9
            cy = sy + math.sin(math.radians(yaw)) * length / 2 * 0.9
            k.box("Leaf", (cx, cy, h + 25), (length, 45, 3), yaw=yaw, pitch=18.0)


def bush(k):
    for x, y, r in ((0, 0, 90), (70, 30, 65), (-60, 40, 60)):
        k.sphere("Leaf", (x, y, r * 0.6), r, scale=(1.0, 1.0, 0.7), steps=7)


def fence_wood(k):
    """3 m fence segment along X, centred on the origin."""
    for x in (-150.0, 150.0):
        k.box_from("WoodDark", x - 5, x + 5, -5, 5, 0, 120)
    for z in (35.0, 90.0):
        k.box_from("WoodDark", -150, 150, -3, 3, z, z + 7)
    x = -135.0
    while x < 140.0:
        k.box_from("PalmBark", x - 3, x + 3, -5, -3, 5, 110)
        x += 22.5


def water_jar(k):
    k.sphere("Clay", (0, 0, 48), 38, scale=(1.0, 1.0, 1.25), steps=10)
    k.cylinder("Clay", (0, 0, 88), 24, 14, steps=10)
    k.cylinder("WoodDark", (0, 0, 102), 27, 4, steps=10)


def motorbike(k):
    for x in (-62.0, 62.0):
        k.cylinder("Metal", (x, -6, 30), 30, 12, steps=12, roll=90.0)
    k.box("BikeRed", (0, 0, 55), (110, 26, 30))
    k.box("Metal", (-15, 0, 78), (70, 24, 10))
    k.box("BikeRed", (55, 0, 72), (30, 28, 45), pitch=-20.0)
    k.box("Metal", (62, 0, 100), (6, 60, 4))


def electric_pole(k):
    k.cone("Concrete", (0, 0, 0), 16, 10, 900, steps=8)
    k.box("Metal", (0, 0, 850), (10, 160, 10))
    for y in (-65.0, 0.0, 65.0):
        k.cylinder("Glass", (0, y, 855), 5, 14, steps=6)


def road_barrier(k):
    """3 m striped barrier along X, closing a road where it leaves the play area."""
    for x in (-140.0, 140.0):
        k.box_from("PaintWhite", x - 6, x + 6, -6, 6, 0, 110)
    stripe = 40.0
    for z in (55.0, 90.0):
        x = -150.0
        index = 0
        while x < 150.0:
            k.box_from("PaintRed" if index % 2 == 0 else "PaintWhite", x, min(x + stripe, 150.0), -4, 4, z, z + 18)
            x += stripe
            index += 1


def garden_bed(k):
    k.box_from("Soil", -200, 200, -100, 100, 0, 15)
    for y in (-60.0, -20.0, 20.0, 60.0):
        k.box_from("Leaf", -185, 185, y - 9, y + 9, 15, 38)


MODELS = (
    ("SM_House_Wooden", house_wooden, True),
    ("SM_House_Concrete", house_concrete, True),
    ("SM_HealthCentre", health_centre, True),
    ("SM_CommunitySala", community_sala, True),
    ("SM_MarketStall", market_stall, True),
    ("SM_Shed", shed, True),
    ("SM_Tree_Palm", tree_palm, True),
    ("SM_Tree_Mango", tree_mango, True),
    ("SM_Plant_Banana", plant_banana, False),
    ("SM_Bush", bush, False),
    ("SM_Fence_Wood", fence_wood, True),
    ("SM_WaterJar", water_jar, True),
    ("SM_Motorbike", motorbike, True),
    ("SM_ElectricPole", electric_pole, True),
    ("SM_GardenBed", garden_bed, False),
    ("SM_RoadBarrier", road_barrier, True),
)


def main():
    palette = materials()
    created, kept = [], []
    for name, builder, collision in MODELS:
        path = "{}/{}".format(PLACEHOLDER_DIR, name)
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            kept.append(name)
            continue
        kit = MeshKit(palette)
        builder(kit)
        kit.create_asset(path, collision=collision)
        created.append(name)
    unreal.log_warning("{} Created {}; kept existing {}".format(LOG, created, kept))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
