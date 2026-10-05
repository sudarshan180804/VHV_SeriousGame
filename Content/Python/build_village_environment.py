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

import math
import random
import traceback
import unreal

import village_layout as layout
from vhv_meshkit import MeshKit, PRIMS, ensure_material_instance, transform


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
}
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
    "SM_ElectricPole": 60.0, "SM_GardenBed": 230.0,
}


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def smooth(points, samples=8):
    """Catmull-Rom curve through the control points."""
    pts = [points[0]] + list(points) + [points[-1]]
    out = []
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        for s in range(samples):
            t = s / float(samples)
            t2, t3 = t * t, t * t * t
            out.append(tuple(
                0.5 * (2 * p1[k] + (-p0[k] + p2[k]) * t + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                       + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3)
                for k in (0, 1)))
    out.append(tuple(points[-1]))
    return out


def tangent(curve, i):
    a = curve[max(0, i - 1)]
    b = curve[min(len(curve) - 1, i + 1)]
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = math.hypot(dx, dy) or 1.0
    return dx / length, dy / length


def distance_to_curve(x, y, curve):
    best = float("inf")
    for (ax, ay), (bx, by) in zip(curve, curve[1:]):
        dx, dy = bx - ax, by - ay
        denom = dx * dx + dy * dy
        t = 0.0 if denom == 0 else max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / denom))
        best = min(best, math.hypot(x - (ax + t * dx), y - (ay + t * dy)))
    return best


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

    def place(self, name, folder, x, y, yaw=0.0, scale=1.0, collision=True):
        self.counts[name] = self.counts.get(name, 0) + 1
        self.spawn(name, "{}_{:03d}".format(name[3:], self.counts[name]), folder, x, y, yaw, scale,
                   collision=collision)
        self.placed.append((x, y, RADIUS.get(name, 150.0) * scale))

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


def build_boundary(actors):
    """Invisible blocking walls on all four sides of the playable village."""
    half, height, thickness = layout.BOUNDARY_HALF_SIZE, layout.BOUNDARY_HEIGHT, 100.0
    length = 2.0 * half + thickness
    center_z = height / 2.0 - 1000.0
    walls = (("N", (0.0, half), (length, thickness)), ("S", (0.0, -half), (length, thickness)),
             ("E", (half, 0.0), (thickness, length)), ("W", (-half, 0.0), (thickness, length)))
    for side, (x, y), (size_x, size_y) in walls:
        wall = actors.spawn_actor_from_class(unreal.BlockingVolume, unreal.Vector(x, y, center_z))
        wall.set_actor_label("{}Boundary_{}".format(PREFIX, side))
        wall.set_folder_path("Village/Boundary")
        # A new volume's default brush is a 200 uu cube.
        wall.set_actor_scale3d(unreal.Vector(size_x / 200.0, size_y / 200.0, height / 200.0))


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
    houses = village.build_houses()
    village.finish_roads()
    village.build_trees()
    village.build_power_poles()
    build_boundary(village.actors)

    world =unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
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
