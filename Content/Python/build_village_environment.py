"""Build the village environment inside LI_Village_Env from village_layout.py.

Generates:
  * SM_Gen_Terrain  flat village square with hills around it
  * SM_Gen_Roads    curved main road, concrete lanes, dirt tracks and house paths
and places the placeholder models from build_placeholder_models.py: story
buildings, market stalls, randomly scattered neighbour houses with yards,
tree clusters, a tree belt on the hills, roadside palms, and power poles.

Every actor it places is labelled "GEN_*" under the "Village" Outliner folder.
Rerunning deletes and rebuilds only those actors (and the older "BO_*" blockout),
so anything you add to LI_Village_Env by hand stays. Replacing a placeholder
model never requires rerunning this script; see Docs/VHV_LEVEL_DESIGN_GUIDE.md.
"""

import importlib
import math
import random
import traceback
import unreal

import village_layout as layout
import vhv_geometry
import vhv_meshkit

# Unreal keeps imported modules cached between runs in the editor; reload so edits are picked up.
for _module in (layout, vhv_geometry, vhv_meshkit):
    importlib.reload(_module)

from vhv_geometry import curve_intersections, distance_to_curve, smooth, tangent  # noqa: E402
from vhv_meshkit import MeshKit, PRIMS, ensure_material_instance, transform  # noqa: E402


ENV_MAP_PATH = "/Game/VHV_Stuff/Maps/Environment/LI_Village_Env"
PLACEHOLDER_DIR = "/Game/VHV_Stuff/Environment/Placeholders"
GENERATED_DIR = "/Game/VHV_Stuff/Environment/Generated"
CUBE_PATH = "/Engine/BasicShapes/Cube"
CYLINDER_PATH = "/Engine/BasicShapes/Cylinder"
PREFIX = "GEN_"
REMOVED_PREFIXES = ("GEN_", "BO_")
REMOVED_LABELS = ("Env_Ground",)
LOG = "[Village Environment]"

SURFACE_MATERIALS = {
    "Asphalt": ("MI_Env_Asphalt", (72, 72, 74), 0.85),
    "Concrete": ("MI_Env_ConcreteRoad", (168, 166, 158), 0.9),
    "Dirt": ("MI_Env_Dirt", (150, 118, 82), 1.0),
    "Brick": ("MI_Env_Brick", (176, 96, 66), 0.9),
    "Paint": ("MI_Env_Paint", (235, 235, 228), 0.6),
}
SIGN_BOARD_MATERIAL = "/Game/VHV_Stuff/Environment/Materials/MI_PH_Plaster"
SIGN_POST_MATERIAL = "/Game/VHV_Stuff/Environment/Materials/MI_PH_WoodDark"
SIGN_TEXT_COLOR = unreal.Color(r=52, g=36, b=24, a=255)
GRASS_MATERIAL = ("MI_Env_Ground", (92, 120, 56), 0.95)
WATER_MATERIAL = ("MI_Env_Water", (38, 72, 80), 0.05)

# Placeholder footprints: (front depth from centre, door x, back depth, half width) in local space.
HOUSE_SHAPES = {
    "SM_House_Wooden": (452.0, -160.0, 312.0, 540.0),
    "SM_House_Concrete": (495.0, -100.0, 328.0, 490.0),
}
# Clearance radius per placed model, used to keep things from overlapping.
RADIUS = {
    "SM_House_Wooden": 650.0, "SM_House_Concrete": 650.0, "SM_HealthCentre": 1150.0,
    "SM_CommunitySala": 700.0, "SM_MarketStall": 160.0, "SM_Shed": 200.0,
    "SM_Tree_Palm": 150.0, "SM_Tree_Mango": 320.0, "SM_Plant_Banana": 160.0, "SM_Bush": 110.0,
    # Under half the 300 cm segment spacing, so the three back-fence segments fit side by side.
    "SM_Fence_Wood": 145.0, "SM_WaterJar": 45.0, "SM_Motorbike": 100.0,
    "SM_ElectricPole": 60.0, "SM_GardenBed": 230.0, "SM_RoadBarrier": 160.0,
}


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def local_to_world(cx, cy, yaw, lx, ly):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return cx + lx * c - ly * s, cy + lx * s + ly * c


def yaw_facing(fx, fy):
    """Yaw that turns a placeholder's -Y front towards direction (fx, fy)."""
    return math.degrees(math.atan2(fx, -fy))


def in_keep_clear(x, y, radius):
    for zone in layout.KEEP_CLEAR:
        if zone[0] == "circle":
            _, zx, zy, zr = zone
            if math.hypot(x - zx, y - zy) < zr + radius:
                return True
        else:
            _, x0, x1, y0, y1 = zone
            if x0 - radius < x < x1 + radius and y0 - radius < y < y1 + radius:
                return True
    return False


def terrain_height(x, y):
    edge = max(abs(x), abs(y))
    if edge <= layout.FLAT_HALF_SIZE:
        return 0.0
    t = min(1.0, (edge - layout.FLAT_HALF_SIZE) / layout.HILL_RAMP)
    t = t * t * (3.0 - 2.0 * t)
    noise = (0.5 + 0.25 * math.sin(x * 0.00031 + 1.3) * math.cos(y * 0.00027 + 0.4)
             + 0.25 * math.sin((x + y) * 0.00017 + 2.1))
    low, high = layout.HILL_HEIGHT
    return t * (low + (high - low) * noise)


# ---------------------------------------------------------------------------
# Builder
# ---------------------------------------------------------------------------

class Village(object):
    def __init__(self):
        self.rng = random.Random(layout.SEED)
        self.actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.meshes = {}
        self.placed = []          # (x, y, radius) of everything placed, for spacing checks
        self.counts = {}
        self.curves = {name: (smooth(points), width) for name, points, width, _, _ in layout.ROADS}
        self.curves["VillageWalk"] = (smooth(layout.VILLAGE_WALK), layout.VILLAGE_WALK_WIDTH)
        self.road_mesh = MeshKit({surface: ensure_material_instance(name, srgb, rough)
                                  for surface, (name, srgb, rough) in SURFACE_MATERIALS.items()})

    # -- placement primitives -----------------------------------------------

    def mesh(self, name):
        if name not in self.meshes:
            path = "{}/{}".format(PLACEHOLDER_DIR, name)
            asset = unreal.load_asset(path)
            if not asset:
                raise RuntimeError("Missing {}; run build_placeholder_models.py first".format(path))
            self.meshes[name] = asset
        return self.meshes[name]

    def spawn(self, mesh, label, folder, x, y, yaw=0.0, scale=1.0, z=None, collision=True):
        z = terrain_height(x, y) if z is None else z
        actor = self.actors.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        actor.set_actor_label(PREFIX + label)
        actor.set_folder_path("Village/" + folder)
        if scale != 1.0:
            actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        component = actor.get_editor_property("static_mesh_component")
        component.set_static_mesh(mesh if not isinstance(mesh, str) else self.mesh(mesh))
        if not collision:
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        return actor

    def place(self, name, folder, x, y, yaw=0.0, scale=1.0, collision=True, stretch=1.0):
        """Place a placeholder model. `stretch` scales its length (local X) on top of `scale`."""
        self.counts[name] = self.counts.get(name, 0) + 1
        actor = self.spawn(name, "{}_{:03d}".format(name[3:], self.counts[name]), folder, x, y, yaw, scale,
                           collision=collision)
        if stretch != 1.0:
            actor.set_actor_scale3d(unreal.Vector(scale * stretch, scale, scale))
        self.placed.append((x, y, RADIUS.get(name, 150.0) * scale * min(stretch, 1.0)))

    def is_free(self, x, y, radius, road_margin=0.0, bounds=9700.0, check_keep_clear=True):
        if abs(x) > bounds or abs(y) > bounds:
            return False
        if check_keep_clear and in_keep_clear(x, y, radius):
            return False
        for curve, width in self.curves.values():
            if distance_to_curve(x, y, curve) < width / 2.0 + radius + road_margin:
                return False
        for px, py, pr in self.placed:
            if math.hypot(x - px, y - py) < pr + radius:
                return False
        return True

    # -- generated meshes -----------------------------------------------------

    def build_terrain(self):
        grass = ensure_material_instance(*GRASS_MATERIAL)
        kit = MeshKit({"Grass": grass})
        steps = int(layout.TERRAIN_SIZE / layout.TERRAIN_STEP)
        PRIMS.append_rectangle_xy(kit.mesh, kit._options("Grass"), unreal.Transform(),
                                  layout.TERRAIN_SIZE, layout.TERRAIN_SIZE, steps, steps)
        result = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(kit.mesh, False)
        positions = unreal.GeometryScript_List.convert_vector_list_to_array(result[1])
        lifted = [unreal.Vector(v.x, v.y, terrain_height(v.x, v.y)) for v in positions]
        unreal.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(
            kit.mesh, unreal.GeometryScript_List.convert_array_to_vector_list(lifted))
        unreal.GeometryScript_Normals.recompute_normals(kit.mesh, unreal.GeometryScriptCalculateNormalsOptions())
        terrain = kit.write_asset(GENERATED_DIR + "/SM_Gen_Terrain")
        self.spawn(terrain, "Terrain", "Ground", 0.0, 0.0, z=0.0)

    def sweep_road(self, curve, width, surface, top):
        path = []
        for i, (x, y) in enumerate(curve):
            tx, ty = tangent(curve, i)
            path.append(transform((x, y, 0.0), yaw=math.degrees(math.atan2(ty, tx))))
        section = [unreal.Vector2D(-width / 2.0, top - 8.0), unreal.Vector2D(width / 2.0, top - 8.0),
                   unreal.Vector2D(width / 2.0, top), unreal.Vector2D(-width / 2.0, top)]
        PRIMS.append_sweep_polygon(self.road_mesh.mesh, self.road_mesh._options(surface), unreal.Transform(),
                                   section, path, False, True, 1.0, 1.0, 0.0, 1.0)

    def build_roads(self):
        for name, points, width, surface, top in layout.ROADS:
            self.sweep_road(self.curves[name][0], width, surface, top)
        walk, walk_width = self.curves["VillageWalk"]
        self.sweep_road(walk, walk_width, "Brick", layout.VILLAGE_WALK_TOP)
        self.build_crossings()

    def build_crossings(self):
        """Zebra stripes wherever the village walk crosses the main road, found from the two curves."""
        road, width = self.curves["MainRoad"]
        walk = self.curves["VillageWalk"][0]
        main_top = next(top for name, _, _, _, top in layout.ROADS if name == "MainRoad")
        crossings = curve_intersections(walk, road)
        if not crossings:
            unreal.log_warning("{} The village walk never crosses the main road; no zebra crossings".format(LOG))
        for (px, py), (tx, ty) in crossings:
            nx, ny = -ty, tx
            yaw = math.degrees(math.atan2(ty, tx))
            stripe, gap = 50.0, 50.0
            count = int(width // (stripe + gap))
            start = -(count - 1) * (stripe + gap) / 2.0
            for k in range(count):
                offset = start + k * (stripe + gap)
                self.road_mesh.box("Paint", (px + nx * offset, py + ny * offset, main_top + 0.5),
                                   (layout.VILLAGE_WALK_WIDTH, stripe, 1.5), yaw=yaw)

    # -- semi-open play area ---------------------------------------------------------

    def build_play_area(self):
        """Fence the walkable village centre, close roads with barriers, back it with invisible walls."""
        x0, x1, y0, y1 = layout.PLAY_AREA
        edges = (("S", (x0, y0), (x1, y0)), ("E", (x1, y0), (x1, y1)),
                 ("N", (x1, y1), (x0, y1)), ("W", (x0, y1), (x0, y0)))
        segment = 300.0
        for side, (ax, ay), (bx, by) in edges:
            length = math.hypot(bx - ax, by - ay)
            dx, dy = (bx - ax) / length, (by - ay) / length
            yaw = math.degrees(math.atan2(dy, dx))
            blocked = []
            for along, footprint in self.road_crossings(ax, ay, bx, by):
                count = int(math.ceil((footprint + 100.0) / segment))
                for k in range(count):
                    offset = along + (k - (count - 1) / 2.0) * segment
                    self.place("SM_RoadBarrier", "Boundary", ax + dx * offset, ay + dy * offset, yaw)
                blocked.append((along - count * segment / 2.0, along + count * segment / 2.0))
            # Fill everything between barriers with fence, stretched to fit, so there is no opening
            # that looks like an exit but is only the invisible wall.
            for start, end in self.free_spans(length, blocked):
                pieces = max(1, int(math.ceil((end - start) / segment)))
                piece = (end - start) / pieces
                for k in range(pieces):
                    along = start + (k + 0.5) * piece
                    self.place("SM_Fence_Wood", "Boundary", ax + dx * along, ay + dy * along, yaw,
                               stretch=piece / segment)
            # Keep houses and trees off the fence line.
            self.curves["Boundary_" + side] = ([(ax, ay), (bx, by)], 80.0)
            self.boundary_wall(side, (ax + bx) / 2.0, (ay + by) / 2.0, length, dx, dy)

    @staticmethod
    def free_spans(length, blocked):
        """Parts of [0, length] not covered by the blocked (start, end) spans."""
        spans, cursor = [], 0.0
        for start, end in sorted(blocked):
            if start - cursor > 1.0:
                spans.append((cursor, min(start, length)))
            cursor = max(cursor, end)
        if length - cursor > 1.0:
            spans.append((cursor, length))
        return [(s, e) for s, e in spans if e - s > 1.0]

    def road_crossings(self, ax, ay, bx, by):
        """(distance along the edge, footprint along the edge) for every place a road crosses this edge.

        A road crossing at an angle covers width / sin(angle) of the edge, so barriers are sized to that.
        """
        horizontal = abs(by - ay) < 1.0
        lo, hi = (min(ax, bx), max(ax, bx)) if horizontal else (min(ay, by), max(ay, by))
        line = ay if horizontal else ax
        start = ax if horizontal else ay
        found = []
        for name, points, width, _, _ in layout.ROADS:
            curve = self.curves[name][0]
            for (px, py), (qx, qy) in zip(curve, curve[1:]):
                p, q = (py, qy) if horizontal else (px, qx)
                # Half-open test so a curve point lying exactly on the edge is counted once.
                if (p - line) * (q - line) > 0 or p == q or q == line:
                    continue
                t = (line - p) / (q - p)
                cross = (px + t * (qx - px)) if horizontal else (py + t * (qy - py))
                if lo <= cross <= hi:
                    length = math.hypot(qx - px, qy - py) or 1.0
                    across = abs((qy - py) if horizontal else (qx - px)) / length
                    found.append((abs(cross - start), width / max(across, 0.3)))
        return found

    def boundary_wall(self, side, cx, cy, length, dx, dy):
        thickness, height = 100.0, layout.BOUNDARY_HEIGHT
        wall = self.actors.spawn_actor_from_class(
            unreal.BlockingVolume, unreal.Vector(cx, cy, height / 2.0 - 1000.0))
        wall.set_actor_label("{}Boundary_{}".format(PREFIX, side))
        wall.set_folder_path("Village/Boundary")
        size_x = length + thickness if abs(dx) > 0.5 else thickness
        size_y = length + thickness if abs(dy) > 0.5 else thickness
        # A new volume's default brush is a 200 uu cube.
        wall.set_actor_scale3d(unreal.Vector(size_x / 200.0, size_y / 200.0, height / 200.0))

    # -- wayfinding ------------------------------------------------------------------------

    def basic_shape(self, path, label, folder, center, size, material, yaw=0.0):
        actor = self.spawn(unreal.load_asset(path), label, folder, center[0], center[1], yaw, z=center[2])
        actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
        actor.get_editor_property("static_mesh_component").set_material(0, material)
        return actor

    def sign_text(self, label, folder, x, y, z, yaw, text):
        actor = self.actors.spawn_actor_from_class(
            unreal.TextRenderActor, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        actor.set_actor_label(PREFIX + label)
        actor.set_folder_path("Village/" + folder)
        component = actor.get_editor_property("text_render")
        component.set_text(text)
        component.set_world_size(17.0)
        component.set_text_render_color(SIGN_TEXT_COLOR)
        component.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        component.set_vertical_alignment(unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
        return actor

    def require_off_roads(self, label, x, y, radius):
        """Fixed layout items must not stand on a road, track, or the village walk."""
        for name, (curve, width) in self.curves.items():
            if distance_to_curve(x, y, curve) < width / 2.0 + radius:
                raise RuntimeError("{} at ({:.0f}, {:.0f}) stands on {}; move it in village_layout.py".format(
                    label, x, y, name))

    def build_signposts(self):
        """Name boards pointing at story places, readable from both sides."""
        board_mat = unreal.load_asset(SIGN_BOARD_MATERIAL)
        post_mat = unreal.load_asset(SIGN_POST_MATERIAL)
        for index, ((sx, sy), entries) in enumerate(layout.SIGNPOSTS):
            name = "Signpost_{:02d}".format(index + 1)
            self.require_off_roads(name, sx, sy, 60.0)
            self.basic_shape(CUBE_PATH, name + "_Post", "Wayfinding", (sx, sy, 130.0), (12.0, 12.0, 260.0), post_mat)
            for row, (label, (tx, ty)) in enumerate(entries):
                dist = math.hypot(tx - sx, ty - sy) or 1.0
                dx, dy = (tx - sx) / dist, (ty - sy) / dist
                nx, ny = -dy, dx
                z = 228.0 - row * 42.0
                bx, by = sx + dx * 88.0, sy + dy * 88.0
                yaw = math.degrees(math.atan2(dy, dx))
                self.basic_shape(CUBE_PATH, "{}_Board{}".format(name, row + 1), "Wayfinding",
                                 (bx, by, z), (170.0, 6.0, 34.0), board_mat, yaw)
                for face, (fx, fy) in (("A", (nx, ny)), ("B", (-nx, -ny))):
                    # A viewer on this face looks along -face; their right is (fy, -fx).
                    right = dx * fy + dy * -fx
                    text = "{}  >".format(label) if right > 0 else "<  {}".format(label)
                    self.sign_text("{}_Text{}{}".format(name, row + 1, face), "Wayfinding",
                                   bx + fx * 4.0, by + fy * 4.0, z, math.degrees(math.atan2(fy, fx)), text)
            self.placed.append((sx, sy, 140.0))

    def build_flags(self):
        """Tall coloured flags beside story places, recognisable over the roofs."""
        pole_mat = unreal.load_asset(SIGN_POST_MATERIAL)
        for index, ((x, y), srgb) in enumerate(layout.FLAGS):
            colour = ensure_material_instance("MI_Env_Flag_{:02X}{:02X}{:02X}".format(*srgb), srgb, 0.8)
            name = "Flag_{:02d}".format(index + 1)
            self.require_off_roads(name, x, y, 40.0)
            self.basic_shape(CYLINDER_PATH, name + "_Pole", "Wayfinding", (x, y, 325.0), (12.0, 12.0, 650.0), pole_mat)
            self.basic_shape(CUBE_PATH, name + "_Banner", "Wayfinding", (x + 62.0, y, 580.0), (120.0, 4.0, 75.0), colour)
            self.placed.append((x, y, 80.0))

    def finish_roads(self):
        roads = self.road_mesh.write_asset(GENERATED_DIR + "/SM_Gen_Roads")
        self.spawn(roads, "Roads", "Ground", 0.0, 0.0, z=0.0)

    # -- fixed story places -----------------------------------------------------

    def build_story_buildings(self):
        for name, mesh, (x, y), yaw in layout.STORY_BUILDINGS:
            self.spawn(mesh, name, "StoryBuildings", x, y, yaw)
            self.placed.append((x, y, RADIUS[mesh]))
        x0, x1, y0, y1 = layout.HEALTH_CENTRE_PLAZA
        plaza = self.spawn(unreal.load_asset(CUBE_PATH), "HealthCentre_Plaza", "StoryBuildings",
                           (x0 + x1) / 2.0, (y0 + y1) / 2.0, z=-1.0)
        plaza.set_actor_scale3d(unreal.Vector((x1 - x0) / 100.0, (y1 - y0) / 100.0, 0.08))
        plaza.get_editor_property("static_mesh_component").set_material(
            0, ensure_material_instance(*SURFACE_MATERIALS["Concrete"]))

        mx, my = layout.MARKET
        for row, sign, yaw in (("S", -1.0, 0.0), ("N", 1.0, 180.0)):
            for dx in layout.MARKET_STALL_X_OFFSETS:
                self.place("SM_MarketStall", "Market",
                           mx + dx + self.rng.uniform(-30, 30),
                           my + sign * layout.MARKET_STALL_ROW_OFFSET_Y + self.rng.uniform(-25, 25),
                           yaw + self.rng.uniform(-5, 5))

        (px, py), radius = layout.POND
        pond = self.spawn(unreal.load_asset(CYLINDER_PATH), "Pond", "Ground", px, py, z=1.0)
        pond.set_actor_scale3d(unreal.Vector(radius / 50.0, radius / 50.0, 0.02))
        pond.get_editor_property("static_mesh_component").set_material(
            0, ensure_material_instance(*WATER_MATERIAL))
        for i in range(14):
            angle = self.rng.uniform(0, 2 * math.pi)
            r = radius + self.rng.uniform(40, 220)
            name = "SM_Bush" if i % 3 else "SM_Plant_Banana"
            self.place(name, "Trees", px + math.cos(angle) * r, py + math.sin(angle) * r,
                       self.rng.uniform(0, 360), self.rng.uniform(0.7, 1.1), collision=False)

    # -- random houses -------------------------------------------------------------

    def build_houses(self):
        weights = [("MainRoad", 4), ("EntranceLane", 1), ("NorthLane", 2), ("SouthTrack", 2),
                   ("NorthTrack", 2), ("WestTrack", 2), ("EastTrack", 2)]
        pool = [name for name, weight in weights for _ in range(weight)]
        houses = []
        attempts = 0
        while len(houses) < layout.HOUSE_COUNT and attempts < 4000:
            attempts += 1
            road = self.rng.choice(pool)
            curve, width = self.curves[road]
            i = self.rng.randint(3, len(curve) - 4)
            px, py = curve[i]
            tx, ty = tangent(curve, i)
            side = self.rng.choice((-1.0, 1.0))
            nx, ny = -ty * side, tx * side
            mesh = "SM_House_Wooden" if self.rng.random() < layout.HOUSE_WOODEN_CHANCE else "SM_House_Concrete"
            front, door_x, back, half_w = HOUSE_SHAPES[mesh]
            setback = self.rng.uniform(*layout.HOUSE_SETBACK)
            offset = width / 2.0 + front + setback
            cx, cy = px + nx * offset, py + ny * offset
            if any(math.hypot(cx - hx, cy - hy) < layout.HOUSE_MIN_SPACING for hx, hy, _, _ in houses):
                continue
            if not self.is_free(cx, cy, RADIUS[mesh], road_margin=-100.0, bounds=9200.0):
                continue
            yaw = yaw_facing(-nx, -ny) + self.rng.uniform(-12.0, 12.0)
            self.place(mesh, "Houses", cx, cy, yaw)
            houses.append((cx, cy, yaw, mesh))
            # Short dirt path from the door to the road.
            dx, dy = local_to_world(cx, cy, yaw, door_x, -front - 10.0)
            length = setback + 60.0
            path_end = (dx - nx * length, dy - ny * length)
            self.sweep_road([(dx, dy), path_end], 140.0, "Dirt", 1.5)
            # Register the path so later houses, yard props, and trees keep off it.
            self.curves["HousePath_{:02d}".format(len(houses))] = ([(dx, dy), path_end], 140.0)
        for cx, cy, yaw, mesh in houses:
            self.build_yard(cx, cy, yaw, mesh)
        return len(houses)

    def build_yard(self, cx, cy, yaw, mesh):
        front, door_x, back, half_w = HOUSE_SHAPES[mesh]
        rng = self.rng

        def try_place(name, folder, lx, ly, local_yaw=0.0, scale=1.0, collision=True):
            x, y = local_to_world(cx, cy, yaw, lx, ly)
            if self.is_free(x, y, RADIUS[name] * scale):
                self.place(name, folder, x, y, yaw + local_yaw, scale, collision)

        # Temporarily forget this house so its own yard props can sit close to it.
        own = self.placed.pop(next(i for i, p in enumerate(self.placed) if p[0] == cx and p[1] == cy))
        for _ in range(rng.randint(1, 3)):
            try_place("SM_WaterJar", "Props", rng.choice((-1, 1)) * rng.uniform(200, half_w - 60),
                      back + rng.uniform(60, 140), rng.uniform(0, 360), rng.uniform(0.8, 1.15))
        if rng.random() < 0.45:
            try_place("SM_Motorbike", "Props", -door_x + rng.uniform(80, 200), -front - rng.uniform(120, 260),
                      rng.uniform(60, 120))
        for _ in range(rng.randint(0, 3)):
            try_place("SM_Plant_Banana", "Trees", rng.choice((-1, 1)) * (half_w + rng.uniform(150, 350)),
                      rng.uniform(-200, back + 200), rng.uniform(0, 360), rng.uniform(0.8, 1.2), False)
        if rng.random() < 0.35:
            for lx in (-300.0, 0.0, 300.0):
                try_place("SM_Fence_Wood", "Props", lx, back + 380.0)
        if rng.random() < 0.3:
            try_place("SM_GardenBed", "Props", rng.uniform(-200, 200), back + 250.0, 0.0, 1.0, False)
        if rng.random() < 0.25:
            try_place("SM_Shed", "Props", -(half_w + 250.0), back - 80.0)
        self.placed.append(own)

    # -- vegetation and utilities ------------------------------------------------------

    def build_trees(self):
        kinds = (("SM_Tree_Palm", 0.4), ("SM_Tree_Mango", 0.3), ("SM_Plant_Banana", 0.2), ("SM_Bush", 0.1))

        def pick():
            roll, total = self.rng.random(), 0.0
            for name, weight in kinds:
                total += weight
                if roll <= total:
                    return name
            return kinds[-1][0]

        for _ in range(layout.TREE_CLUSTERS):
            cx, cy = self.rng.uniform(-9500, 9500), self.rng.uniform(-9500, 9500)
            name = pick()
            for _ in range(self.rng.randint(2, 7)):
                r, a = self.rng.uniform(0, 1100), self.rng.uniform(0, 2 * math.pi)
                x, y = cx + math.cos(a) * r, cy + math.sin(a) * r
                scale = self.rng.uniform(0.8, 1.3)
                if self.is_free(x, y, RADIUS[name] * scale * 0.7, road_margin=100.0):
                    self.place(name, "Trees", x, y, self.rng.uniform(0, 360), scale,
                               collision=name in ("SM_Tree_Palm", "SM_Tree_Mango"))

        placed, attempts = 0, 0
        while placed < layout.TREE_BELT_COUNT and attempts < layout.TREE_BELT_COUNT * 10:
            attempts += 1
            x, y = self.rng.uniform(-15000, 15000), self.rng.uniform(-15000, 15000)
            if max(abs(x), abs(y)) < 9900:
                continue
            name = "SM_Tree_Mango" if self.rng.random() < 0.55 else (
                "SM_Tree_Palm" if self.rng.random() < 0.8 else "SM_Bush")
            scale = self.rng.uniform(0.9, 1.5)
            if self.is_free(x, y, RADIUS[name] * scale * 0.6, bounds=15500.0, check_keep_clear=False):
                self.place(name, "Trees", x, y, self.rng.uniform(0, 360), scale)
                placed += 1

        for road in ("MainRoad", "EntranceLane", "NorthLane"):
            curve, width = self.curves[road]
            travelled, side = 0.0, 1.0
            for i in range(1, len(curve)):
                travelled += math.hypot(curve[i][0] - curve[i - 1][0], curve[i][1] - curve[i - 1][1])
                if travelled < layout.ROADSIDE_PALM_SPACING:
                    continue
                travelled, side = 0.0, -side
                tx, ty = tangent(curve, i)
                offset = width / 2.0 + self.rng.uniform(250, 450)
                x, y = curve[i][0] - ty * side * offset, curve[i][1] + tx * side * offset
                if self.is_free(x, y, 150.0):
                    self.place("SM_Tree_Palm", "Trees", x, y, self.rng.uniform(0, 360), self.rng.uniform(0.9, 1.25))

    def build_power_poles(self):
        curve, width = self.curves["MainRoad"]
        travelled = 0.0
        for i in range(1, len(curve)):
            travelled += math.hypot(curve[i][0] - curve[i - 1][0], curve[i][1] - curve[i - 1][1])
            if travelled < 3000.0:
                continue
            travelled = 0.0
            tx, ty = tangent(curve, i)
            offset = width / 2.0 + 180.0
            x, y = curve[i][0] + ty * offset, curve[i][1] - tx * offset
            if self.is_free(x, y, 60.0, road_margin=-200.0):
                self.place("SM_ElectricPole", "Utilities", x, y, math.degrees(math.atan2(ty, tx)))


def remove_previous(actors):
    removed = 0
    for actor in list(actors.get_all_level_actors()):
        label = actor.get_actor_label()
        if label.startswith(REMOVED_PREFIXES) or label in REMOVED_LABELS:
            actors.destroy_actor(actor)
            removed += 1
    return removed


def main():
    if not unreal.EditorLoadingAndSavingUtils.load_map(ENV_MAP_PATH):
        raise RuntimeError("Could not load {}; run setup_village_main_map.py first".format(ENV_MAP_PATH))
    village = Village()
    removed = remove_previous(village.actors)

    village.build_terrain()
    village.build_roads()
    village.build_story_buildings()
    # Boundary, signs and flags go in before the random scatter so houses and trees keep clear of them.
    village.build_play_area()
    village.build_signposts()
    village.build_flags()
    houses = village.build_houses()
    village.finish_roads()
    village.build_trees()
    village.build_power_poles()

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, ENV_MAP_PATH):
        raise RuntimeError("Could not save {}".format(ENV_MAP_PATH))
    unreal.log_warning("{} Removed {} old actors; placed {} random houses; counts {}".format(
        LOG, removed, houses, village.counts))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
