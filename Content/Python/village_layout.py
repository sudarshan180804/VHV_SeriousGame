"""Shared layout for Lvl_Village_Main.

build_village_environment.py (LI_Village_Env) and place_village_story_actors.py
(Lvl_Village_Main) both read this file, so buildings and story staging stay aligned.

Fixed parts: roads, story scene anchors, and the story buildings around them.
Random parts: neighbour houses, yard props, and vegetation are scattered with a
fixed SEED, so every run gives the same village. Change SEED for a new variation.

Units are centimetres. Ground top is Z = 0 inside the village (the walkable
200 m x 200 m square, -10000..10000); hills rise outside it. +X east, +Y north.
"""

SEED = 20260930

# Story scene anchors, in playthrough order. place_village_story_actors.py stages NPCs around these.
#
# The story district sits in the village centre. The game has no quest markers, the Instructor
# speaks the lesson conversations wherever the player is, and the arc ends by walking back to
# him, so every story place is kept within sight of the health-centre plaza (about 20-70 m
# walks, matching the compact staging of Lvl_Village_Blockout). Each scene keeps the blockout's
# orientation: the Instructor faces south towards the arriving player, Prasert and Saeng stand
# in front yards with their houses to the south, and Mali faces west with her house to the east.
INSTRUCTOR = (-4200.0, 1300.0)       # health-centre plaza, north side of the main road
MARKET = (-1300.0, 1600.0)           # market aisle centre, east of the plaza
PRASERT = (-2000.0, -1900.0)         # Uncle Prasert's front yard, across the road from the plaza
# The demo has no "reach location" step before it and an ambient scene cancels when the
# player is over 18 m from its NPCs, so the sala must stay within 18 m of Prasert's yard.
DEMO = (PRASERT[0] + 1350.0, PRASERT[1])   # community sala beside Prasert's house
SAENG = (1300.0, -1800.0)            # Aunt Saeng's front yard, further east on the same side
MALI = (2700.0, 2800.0)              # Aunt Mali's front yard, just off the north track

# Roads: (name, control points, width, surface, top height). Control points are smoothed
# into curves. Surfaces: "Asphalt", "Concrete", "Dirt". Different top heights stop
# overlapping junctions from flickering.
ROADS = (
    ("MainRoad", ((-10600.0, -700.0), (-8200.0, -250.0), (-6000.0, 0.0), (-4200.0, 250.0),
                  (-3000.0, 150.0), (-1500.0, 120.0), (0.0, -120.0), (1500.0, -60.0),
                  (3000.0, 120.0), (4500.0, 220.0), (5800.0, 40.0), (7000.0, 0.0),
                  (8400.0, -300.0), (10600.0, -800.0)), 800.0, "Asphalt", 4.0),
    ("EntranceLane", ((-5500.0, -10600.0), (-5700.0, -8200.0), (-6000.0, -5200.0),
                      (-5900.0, -2600.0), (-6000.0, 0.0)), 700.0, "Concrete", 3.0),
    ("NorthLane", ((7000.0, 0.0), (6800.0, 2000.0), (7050.0, 4000.0), (7000.0, 5500.0),
                   (6700.0, 7400.0)), 600.0, "Concrete", 3.0),
    ("SouthTrack", ((-3200.0, 150.0), (-3500.0, -2500.0), (-2900.0, -5000.0), (-3600.0, -7600.0),
                    (-3100.0, -9800.0)), 350.0, "Dirt", 2.0),
    ("NorthTrack", ((2200.0, 40.0), (2000.0, 3500.0), (2600.0, 6000.0), (1800.0, 8800.0)),
     350.0, "Dirt", 2.0),
    ("WestTrack", ((-8200.0, -250.0), (-8600.0, 3000.0), (-7600.0, 6500.0), (-8300.0, 9000.0)),
     350.0, "Dirt", 2.0),
    ("EastTrack", ((8400.0, -300.0), (8000.0, -3500.0), (8800.0, -6500.0), (8200.0, -9200.0)),
     350.0, "Dirt", 2.0),
)

# Story and public buildings: (name, placeholder mesh, centre, yaw). Placeholder models face -Y
# at yaw 0; yaw 180 faces north (+Y), 90 faces east (+X), -90 faces west (-X).
STORY_BUILDINGS = (
    ("HealthCentre", "SM_HealthCentre", (INSTRUCTOR[0], INSTRUCTOR[1] + 1100.0), 0.0),
    ("CommunitySala", "SM_CommunitySala", DEMO, 0.0),
    ("PrasertHouse", "SM_House_Wooden", (PRASERT[0], PRASERT[1] - 900.0), 180.0),
    ("SaengHouse", "SM_House_Concrete", (SAENG[0], SAENG[1] - 1000.0), 180.0),
    ("MaliHouse", "SM_House_Wooden", (MALI[0] + 750.0, MALI[1]), -90.0),
)

# Paved plaza between the health centre's entrance and the main road: (x0, x1, y0, y1).
HEALTH_CENTRE_PLAZA = (INSTRUCTOR[0] - 800.0, INSTRUCTOR[0] + 800.0, 700.0, 1550.0)

# Market stalls: two rows either side of the aisle through MARKET.
MARKET_STALL_ROW_OFFSET_Y = 750.0
MARKET_STALL_X_OFFSETS = (-1150.0, -700.0, -250.0, 250.0, 700.0, 1150.0)

POND = ((300.0, -6200.0), 900.0)

# Areas kept clear of random houses, props, and trees so story staging stays readable.
# ("circle", x, y, radius) or ("rect", x0, x1, y0, y1).
KEEP_CLEAR = (
    ("rect", INSTRUCTOR[0] - 1200.0, INSTRUCTOR[0] + 1200.0, 450.0, 3300.0),   # health centre and plaza
    ("rect", MARKET[0] - 1450.0, MARKET[0] + 1450.0, 450.0, 2800.0),           # market
    ("circle", PRASERT[0], PRASERT[1] - 500.0, 1150.0),                       # Prasert's yard and house
    ("circle", DEMO[0], DEMO[1], 900.0),                                       # sala demo
    ("circle", SAENG[0], SAENG[1] - 600.0, 1100.0),                            # Saeng's yard and house
    ("circle", MALI[0] + 450.0, MALI[1], 1100.0),                              # Mali's yard and house
    # Sightline from the plaza across the road to Prasert, the sala demo, and Saeng.
    ("rect", PRASERT[0] - 1300.0, SAENG[0] + 900.0, -1450.0, -250.0),
    ("circle", POND[0][0], POND[0][1], POND[1] + 300.0),
)

# Random neighbour houses.
HOUSE_COUNT = 32
HOUSE_MIN_SPACING = 1500.0
HOUSE_WOODEN_CHANCE = 0.55
HOUSE_SETBACK = (200.0, 1100.0)       # extra distance from the road edge

# Vegetation.
TREE_CLUSTERS = 70
TREE_BELT_COUNT = 160                 # trees on the hills around the village
ROADSIDE_PALM_SPACING = 2400.0

# Terrain: flat village square, hills beyond it.
TERRAIN_SIZE = 32000.0
TERRAIN_STEP = 400.0
# Flat beyond the boundary walls and the road ends (+/-10600), so roads never sink into the hills.
FLAT_HALF_SIZE = 10900.0
HILL_RAMP = 5000.0
HILL_HEIGHT = (500.0, 1300.0)

# Invisible walls keeping the player inside the village (the hills are walkable and the terrain
# ends at +/-16000, so without them the player can walk off the world). Just past the road ends.
BOUNDARY_HALF_SIZE = 10800.0
BOUNDARY_HEIGHT = 6000.0
