"""Place the first-pass major structures in Lvl_Village_Main safely."""

import datetime
import json
import math
import os
import re
import traceback

import unreal


TARGET_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Main"
FORBIDDEN_MAP = "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout"
ASSET_ROOT = "/Game/3D_Models"
REPORT_FOLDER = "VHV_VillagePlacement"
LOG_PREFIX = "[VHV Village Placement]"

TRACE_TOP_Z = 100000.0
TRACE_BOTTOM_Z = -100000.0
PLAYER_GROUND_TOLERANCE = 25.0
PLAYER_SUPPORT_RADIUS = 45.0
PLAYER_HEIGHT = 190.0
TRANSFORM_TOLERANCE = 0.1
VALIDATION_ONLY = os.environ.get("VHV_VILLAGE_PLACEMENT_APPLY", "").strip() != "1"
CENTRAL_ROAD_HALF_WIDTH = 500.0


PLACEMENTS = (
    # asset, actor label, x, y, yaw, folder
    ("H1", "VM_H1_MaliHouse", -4650.0, 10400.0, 90.0, "Village_Main/Residential"),
    ("H2", "VM_H2_PrasertHouse", -5200.0, 5850.0, 90.0, "Village_Main/Residential"),
    ("H3", "VM_H3_SaengHouse", -1725.0, 10920.0, 90.0, "Village_Main/Residential"),
    ("H4", "VM_H4_ShopHouse", 5100.0, 7215.0, 270.0, "Village_Main/Residential"),
    ("H5", "VM_H5_Workshop", 7725.0, 7475.0, 270.0, "Village_Main/Residential"),
    ("H6", "VM_H6_ChaiHouse", 8100.0, 14690.0, 180.0, "Village_Main/Residential"),
    ("H7", "VM_H7_NeighborHouse", 10300.0, 16000.0, 180.0, "Village_Main/Residential"),
    ("H8", "VM_H8_FamilyHouse", 7575.0, 10600.0, 270.0, "Village_Main/Residential"),
    ("P1", "VM_P1_HealthPost", 4050.0, 11115.0, 270.0, "Village_Main/HealthPost"),
    ("P2", "VM_P2_CommunityPavilion", 2625.0, 15800.0, 180.0, "Village_Main/Community"),
    ("P3", "VM_P3_ExercisePark", -3900.0, 16250.0, 180.0, "Village_Main/ExercisePark"),
    ("P4", "VM_P4_MainMarket", -1950.0, 3250.0, 90.0, "Village_Main/Market"),
    ("P5", "VM_P5_MarketKiosk", 2175.0, 5070.0, 270.0, "Village_Main/Market"),
    ("P6", "VM_P6_DrinkStall", -2250.0, 5330.0, 90.0, "Village_Main/Market"),
    ("P7", "VM_P7_ShopRow", 2325.0, 3190.0, 270.0, "Village_Main/Market"),
    ("SPIRITHOUSE", "VM_SpiritHouse", -1500.0, 715.0, 90.0, "Village_Main/Landmarks"),
)


def _package_path(obj):
    return obj.get_path_name().split(".", 1)[0] if obj else ""


def _safe_get(obj, property_name, default=None):
    try:
        return obj.get_editor_property(property_name)
    except Exception:
        return default


def _vector_dict(value):
    return {"x": float(value.x), "y": float(value.y), "z": float(value.z)}


def _near(a, b, tolerance=TRANSFORM_TOLERANCE):
    return abs(float(a) - float(b)) <= tolerance


def _current_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return subsystem.get_editor_world() if subsystem else None


def _validate_world(report):
    world = _current_world()
    path = _package_path(world)
    report["loaded_world"] = path
    if not world or path != TARGET_MAP:
        report["errors"].append(
            "Loaded editor world must be {}; found {}".format(TARGET_MAP, path or "None"))
        return None
    if path == FORBIDDEN_MAP:
        report["errors"].append("Refusing to operate on {}".format(FORBIDDEN_MAP))
        return None
    return world


def _load_assets(report):
    assets = {}
    for asset_name, _, _, _, _, _ in PLACEMENTS:
        path = "{}/{}".format(ASSET_ROOT, asset_name)
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset or not isinstance(asset, unreal.StaticMesh):
            report["missing_assets"].append(path)
            continue
        assets[asset_name] = asset
    if len(assets) != len(PLACEMENTS):
        report["errors"].append("All 16 exact Static Mesh assets are required")
    return assets


def _mesh_bounds(mesh, report):
    bounds = mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    values = (origin.x, origin.y, origin.z, extent.x, extent.y, extent.z)
    if not all(math.isfinite(float(value)) for value in values):
        report["errors"].append("Non-finite bounds for {}".format(_package_path(mesh)))
        return None
    if min(float(extent.x), float(extent.y), float(extent.z)) <= 0.0:
        report["errors"].append("Invalid zero/negative bounds for {}".format(_package_path(mesh)))
        return None
    return origin, extent


def _is_reliable_terrain(actor, component):
    actor_class = actor.get_class().get_name().lower() if actor else ""
    component_class = component.get_class().get_name().lower() if component else ""
    actor_name = actor.get_name().lower() if actor else ""
    actor_label = actor.get_actor_label().lower() if actor and hasattr(actor, "get_actor_label") else ""
    component_name = component.get_name().lower() if component else ""
    if "landscape" in actor_class or "landscape" in component_class:
        return True
    tokens = ("terrain", "ground", "landscape")
    words = re.split(r"[^a-z0-9]+", " ".join((actor_name, actor_label, component_name)))
    return any(word.startswith(token) for token in tokens for word in words if word)


def _can_physically_obstruct_player(actor):
    if isinstance(actor, (unreal.StaticMeshActor, unreal.Pawn)):
        return True
    class_name = actor.get_class().get_name().lower() if actor else ""
    return "character" in class_name or "vehicle" in class_name


def _terrain_surface(world, x, y, actors_to_ignore=None):
    start = unreal.Vector(x, y, TRACE_TOP_Z)
    end = unreal.Vector(x, y, TRACE_BOTTOM_Z)
    hits = unreal.SystemLibrary.line_trace_multi(
        world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, list(actors_to_ignore or []), unreal.DrawDebugTrace.NONE, True) or []
    if not bool(hits):
        return None, "trace returned no blocking hits"

    terrain_hits = []
    for hit in hits:
        (unused_blocking, unused_initial_overlap, unused_time, unused_distance,
         unused_location, impact_point, unused_normal, impact_normal,
         unused_phys_mat, actor, component, unused_hit_bone_name, unused_bone_name,
         unused_hit_item, unused_element_index, unused_face_index,
         unused_trace_start, unused_trace_end) = hit.to_tuple()
        if (_is_reliable_terrain(actor, component)
                and float(impact_normal.z) >= 0.35):
            terrain_hits.append((float(impact_point.z), actor, component, impact_normal))
    if not terrain_hits:
        return None, "no hit could be identified reliably as terrain"

    terrain_hits.sort(key=lambda item: item[0], reverse=True)
    top = terrain_hits[0]
    materially_different = [item for item in terrain_hits[1:] if abs(item[0] - top[0]) > 50.0]
    if materially_different:
        return None, "multiple terrain-like surfaces were hit at materially different heights"
    return {
        "z": top[0],
        "actor": top[1].get_actor_label() if top[1] else "",
        "component": top[2].get_name() if top[2] else "",
        "normal": _vector_dict(top[3]),
    }, ""


def _rotated_xy(origin, extent, yaw_degrees):
    radians = math.radians(yaw_degrees)
    cosine = math.cos(radians)
    sine = math.sin(radians)
    origin_x = cosine * float(origin.x) - sine * float(origin.y)
    origin_y = sine * float(origin.x) + cosine * float(origin.y)
    extent_x = abs(cosine) * float(extent.x) + abs(sine) * float(extent.y)
    extent_y = abs(sine) * float(extent.x) + abs(cosine) * float(extent.y)
    return origin_x, origin_y, extent_x, extent_y


def _xy_footprint(bounds, x, y, yaw):
    origin, extent = bounds
    origin_x, origin_y, extent_x, extent_y = _rotated_xy(origin, extent, yaw)
    return float(x) + origin_x, float(y) + origin_y, extent_x, extent_y


def _find_h7_location(world, bounds_by_name, report):
    original_x, original_y = 14100.0, 25800.0
    h6_x, h6_y = 10800.0, 22600.0
    base_angle = math.atan2(h6_y - original_y, h6_x - original_x)
    angle_offsets = (0.0, 15.0, -15.0, 30.0, -30.0, 45.0, -45.0,
                     60.0, -60.0, 75.0, -75.0, 90.0, -90.0)

    fixed_footprints = []
    for asset_name, label, x, y, yaw, unused_folder in PLACEMENTS:
        if asset_name == "H7":
            continue
        center_x, center_y, extent_x, extent_y = _xy_footprint(
            bounds_by_name[asset_name], x, y, yaw)
        fixed_footprints.append((label, center_x, center_y, extent_x, extent_y))

    rejected_overlap_count = 0
    for distance in range(250, 3001, 250):
        for offset in angle_offsets:
            angle = base_angle + math.radians(offset)
            x = round(original_x + distance * math.cos(angle), 1)
            y = round(original_y + distance * math.sin(angle), 1)
            center_x, center_y, extent_x, extent_y = _xy_footprint(
                bounds_by_name["H7"], x, y, 180.0)
            overlap = next((label for label, other_x, other_y, other_extent_x, other_extent_y
                            in fixed_footprints
                            if abs(center_x - other_x) <= extent_x + other_extent_x
                            and abs(center_y - other_y) <= extent_y + other_extent_y), None)
            if overlap:
                rejected_overlap_count += 1
                continue
            terrain, error = _terrain_surface(world, x, y)
            support_hits = []
            if terrain:
                for sample_x, sample_y in (
                        (center_x - extent_x, center_y - extent_y),
                        (center_x - extent_x, center_y + extent_y),
                        (center_x + extent_x, center_y - extent_y),
                        (center_x + extent_x, center_y + extent_y)):
                    support, unused_error = _terrain_surface(world, sample_x, sample_y)
                    if not support:
                        support_hits = []
                        break
                    support_hits.append(support)
            if terrain and len(support_hits) == 4 and all(
                    abs(float(support["z"]) - float(terrain["z"])) <= 50.0
                    for support in support_hits):
                report["h7_adjustment"] = {
                    "original": {"x": original_x, "y": original_y},
                    "final": {"x": x, "y": y},
                    "distance_cm": float(distance),
                    "direction_offset_degrees": float(offset),
                    "toward_h6_preferred": True,
                    "overlap_candidates_rejected": rejected_overlap_count,
                    "terrain_actor": terrain["actor"],
                }
                return x, y, terrain
    return None, None, None


def _preflight_placements(world, assets, managed, report):
    managed_actors = list(managed.values())
    bounds_by_name = {}
    for asset_name, _, _, _, _, _ in PLACEMENTS:
        mesh = assets.get(asset_name)
        if mesh:
            bounds_by_name[asset_name] = _mesh_bounds(mesh, report)

    planned = []
    for asset_name, label, x, y, yaw, folder in PLACEMENTS:
        mesh = assets.get(asset_name)
        if not mesh:
            continue
        bounds = bounds_by_name.get(asset_name)
        if not bounds:
            continue
        terrain, terrain_error = _terrain_surface(world, x, y, managed_actors)
        if not terrain:
            report["terrain_issues"].append({
                "actor_label": label, "x": x, "y": y, "reason": terrain_error})
            continue
        origin, extent = bounds
        local_bottom_z = float(origin.z) - float(extent.z)
        actor_z = float(terrain["z"]) - local_bottom_z
        origin_x, origin_y, extent_x, extent_y = _rotated_xy(origin, extent, yaw)
        world_center = unreal.Vector(
            x + origin_x, y + origin_y, actor_z + float(origin.z))
        support_z = []
        support_error = ""
        for sample_x, sample_y in (
                (world_center.x - extent_x, world_center.y - extent_y),
                (world_center.x - extent_x, world_center.y + extent_y),
                (world_center.x + extent_x, world_center.y - extent_y),
                (world_center.x + extent_x, world_center.y + extent_y)):
            support, error = _terrain_surface(world, sample_x, sample_y, managed_actors)
            if not support:
                support_error = "footprint corner lacks terrain support: {}".format(error)
                break
            support_z.append(float(support["z"]))
        if support_error or any(abs(z - float(terrain["z"])) > 50.0 for z in support_z):
            report["terrain_issues"].append({
                "actor_label": label, "x": x, "y": y,
                "reason": support_error or "footprint terrain varies by more than 50 cm"})
            continue
        planned.append({
            "asset_name": asset_name,
            "asset": mesh,
            "asset_path": _package_path(mesh),
            "actor_label": label,
            "folder": folder,
            "location": unreal.Vector(x, y, actor_z),
            "rotation": unreal.Rotator(pitch=0.0, yaw=yaw, roll=0.0),
            "yaw": yaw,
            "terrain": terrain,
            "bounds_center": world_center,
            "bounds_extent": unreal.Vector(extent_x, extent_y, float(extent.z)),
            "local_bounds_origin": _vector_dict(origin),
            "local_bounds_extent": _vector_dict(extent),
        })
    if len(planned) != len(PLACEMENTS):
        report["errors"].append("Terrain and bounds preflight must succeed for all 16 placements")
    return planned


def _aabb_overlaps(center_a, extent_a, center_b, extent_b):
    return (
        abs(float(center_a.x) - float(center_b.x)) <= float(extent_a.x) + float(extent_b.x)
        and abs(float(center_a.y) - float(center_b.y)) <= float(extent_a.y) + float(extent_b.y)
        and abs(float(center_a.z) - float(center_b.z)) <= float(extent_a.z) + float(extent_b.z)
    )


def _planned_overlaps(planned, existing_actors, managed_labels, report):
    for index, first in enumerate(planned):
        for second in planned[index + 1:]:
            if _aabb_overlaps(
                    first["bounds_center"], first["bounds_extent"],
                    second["bounds_center"], second["bounds_extent"]):
                report["approximate_overlaps"].append({
                    "kind": "planned_vs_planned",
                    "first": first["actor_label"],
                    "second": second["actor_label"],
                })
                report["building_overlaps"].append({
                    "first": first["actor_label"], "second": second["actor_label"]})

    for placement in planned:
        for actor in existing_actors:
            if not actor or actor.get_actor_label() in managed_labels or _is_reliable_terrain(actor, None):
                continue
            try:
                center, extent = actor.get_actor_bounds(False, False)
            except Exception:
                continue
            if max(float(extent.x), float(extent.y), float(extent.z)) <= 0.0:
                continue
            if _aabb_overlaps(
                    placement["bounds_center"], placement["bounds_extent"], center, extent):
                report["approximate_overlaps"].append({
                    "kind": "planned_vs_existing",
                    "planned": placement["actor_label"],
                    "existing_label": actor.get_actor_label(),
                    "existing_class": actor.get_class().get_name(),
                })


def _layout_space_checks(planned, report):
    for placement in planned:
        center = placement["bounds_center"]
        extent = placement["bounds_extent"]
        if (float(center.y) + float(extent.y) >= 0.0
                and float(center.y) - float(extent.y) <= 17000.0
                and float(center.x) - float(extent.x) < CENTRAL_ROAD_HALF_WIDTH
                and float(center.x) + float(extent.x) > -CENTRAL_ROAD_HALF_WIDTH):
            report["central_road_conflicts"].append({
                "actor_label": placement["actor_label"],
                "footprint_x_min": float(center.x) - float(extent.x),
                "footprint_x_max": float(center.x) + float(extent.x),
                "reserved_x_min": -CENTRAL_ROAD_HALF_WIDTH,
                "reserved_x_max": CENTRAL_ROAD_HALF_WIDTH,
            })

    by_label = {placement["actor_label"]: placement for placement in planned}
    for label in ("VM_P1_HealthPost", "VM_P2_CommunityPavilion", "VM_P3_ExercisePark"):
        focus = by_label.get(label)
        if not focus:
            continue
        nearest = None
        for other in planned:
            if other is focus:
                continue
            gap_x = max(0.0, abs(float(focus["bounds_center"].x)
                                 - float(other["bounds_center"].x))
                        - float(focus["bounds_extent"].x) - float(other["bounds_extent"].x))
            gap_y = max(0.0, abs(float(focus["bounds_center"].y)
                                 - float(other["bounds_center"].y))
                        - float(focus["bounds_extent"].y) - float(other["bounds_extent"].y))
            clearance = math.sqrt(gap_x * gap_x + gap_y * gap_y)
            if nearest is None or clearance < nearest["clearance_cm"]:
                nearest = {"nearest_actor": other["actor_label"], "clearance_cm": clearance}
        report["activity_clearances_cm"][label] = nearest


def _preflight_managed_actors(existing_actors, report):
    labels = {placement[1] for placement in PLACEMENTS}
    matches = {label: [] for label in labels}
    for actor in existing_actors:
        label = actor.get_actor_label()
        if label in matches:
            matches[label].append(actor)
    resolved = {}
    for label, actors in matches.items():
        if len(actors) > 1:
            report["errors"].append("Duplicate managed actor label {}".format(label))
        elif len(actors) == 1:
            actor = actors[0]
            if not isinstance(actor, unreal.StaticMeshActor):
                report["errors"].append("Managed label {} belongs to a non-StaticMeshActor".format(label))
            else:
                resolved[label] = actor
        else:
            report["errors"].append("Required managed actor is missing: {}".format(label))
    return resolved


def _preflight_player_start(world, existing_actors, report):
    starts = [actor for actor in existing_actors if isinstance(actor, unreal.PlayerStart)]
    if len(starts) != 1:
        report["errors"].append("Expected exactly one PlayerStart; found {}".format(len(starts)))
        return None

    samples = []
    for x, y in (
            (0.0, 0.0),
            (PLAYER_SUPPORT_RADIUS, 0.0), (-PLAYER_SUPPORT_RADIUS, 0.0),
            (0.0, PLAYER_SUPPORT_RADIUS), (0.0, -PLAYER_SUPPORT_RADIUS)):
        terrain, error = _terrain_surface(world, x, y)
        if not terrain:
            report["player_start_issue"] = "Terrain support failed at ({}, {}): {}".format(x, y, error)
            report["errors"].append("PlayerStart origin terrain support is unreliable")
            return None
        samples.append(terrain["z"])
    if any(abs(z) > PLAYER_GROUND_TOLERANCE for z in samples):
        report["player_start_issue"] = (
            "Terrain near (0,0,0) is not within {:.1f} cm of Z=0: {}".format(
                PLAYER_GROUND_TOLERANCE, [round(z, 2) for z in samples]))
        report["errors"].append("PlayerStart cannot be safely grounded at exact (0,0,0)")
        return None
    if max(samples) - min(samples) > PLAYER_GROUND_TOLERANCE:
        report["player_start_issue"] = "Terrain support around PlayerStart is too uneven: {}".format(
            [round(z, 2) for z in samples])
        report["errors"].append("PlayerStart support area is too uneven")
        return None

    player_start = starts[0]
    for actor in existing_actors:
        if (actor is player_start or _is_reliable_terrain(actor, None)
                or not _can_physically_obstruct_player(actor)):
            continue
        try:
            center, extent = actor.get_actor_bounds(False, False)
        except Exception:
            continue
        if (abs(float(center.x)) <= float(extent.x) + PLAYER_SUPPORT_RADIUS
                and abs(float(center.y)) <= float(extent.y) + PLAYER_SUPPORT_RADIUS
                and float(center.z) - float(extent.z) < PLAYER_HEIGHT
                and float(center.z) + float(extent.z) > 0.0):
            report["player_start_issue"] = "Origin overlaps actor {} ({})".format(
                actor.get_actor_label(), actor.get_class().get_name())
            report["player_start_deferred"] = True
            report["player_start_support_z"] = [round(z, 2) for z in samples]
            return player_start
    report["player_start_support_z"] = [round(z, 2) for z in samples]
    return player_start


def _component_for(actor):
    component = _safe_get(actor, "static_mesh_component")
    if not component:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
    return component


def _actor_matches(actor, placement):
    return not _actor_differences(actor, placement)


def _angle_delta(a, b):
    return abs((float(a) - float(b) + 180.0) % 360.0 - 180.0)


def _actor_differences(actor, placement):
    component = _component_for(actor)
    current_mesh = _safe_get(component, "static_mesh") if component else None
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    folder = str(actor.get_folder_path()) if hasattr(actor, "get_folder_path") else ""
    desired = placement["location"]
    actual = {
        "actor_label": actor.get_actor_label(),
        "folder": folder,
        "mesh": _package_path(current_mesh),
        "location": _vector_dict(location),
        "rotation": {
            "pitch": float(rotation.pitch), "yaw": float(rotation.yaw), "roll": float(rotation.roll)},
        "scale": _vector_dict(scale),
        "world": _package_path(actor.get_world()),
    }
    expected = {
        "actor_label": placement["actor_label"],
        "folder": placement["folder"],
        "mesh": placement["asset_path"],
        "location": _vector_dict(desired),
        "rotation": {"pitch": 0.0, "yaw": placement["yaw"], "roll": 0.0},
        "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        "world": TARGET_MAP,
    }
    fields = []
    if actual["actor_label"] != expected["actor_label"]:
        fields.append("actor_label")
    if actual["folder"] != expected["folder"]:
        fields.append("folder")
    if actual["mesh"] != expected["mesh"]:
        fields.append("mesh")
    if not all(_near(getattr(location, axis), getattr(desired, axis), 0.5)
               for axis in ("x", "y", "z")):
        fields.append("location")
    if (not _near(rotation.pitch, 0.0, 0.1)
            or _angle_delta(rotation.yaw, placement["yaw"]) > 0.1
            or not _near(rotation.roll, 0.0, 0.1)):
        fields.append("rotation")
    if not all(_near(getattr(scale, axis), 1.0, 0.01) for axis in ("x", "y", "z")):
        fields.append("scale")
    if actual["world"] != expected["world"]:
        fields.append("world")
    return {"fields": fields, "expected": expected, "actual": actual} if fields else None


def _configure_actor(actor, placement):
    actor.modify()
    component = _component_for(actor)
    if not component:
        raise RuntimeError("{} has no StaticMeshComponent".format(placement["actor_label"]))
    component.modify()
    component.set_static_mesh(placement["asset"])
    actor.set_actor_label(placement["actor_label"])
    actor.set_folder_path(placement["folder"])
    actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
    actor.set_actor_location(placement["location"], False, True)
    actor.set_actor_rotation(placement["rotation"], False)


def _place(actor_subsystem, planned, managed, player_start, report):
    placed_actors = {}
    for placement in planned:
        actor = managed.get(placement["actor_label"])
        if actor and _actor_matches(actor, placement):
            status = "unchanged"
            report["unchanged"].append(placement["actor_label"])
        else:
            if not actor:
                raise RuntimeError(
                    "Required existing managed actor is missing: {}".format(
                        placement["actor_label"]))
            else:
                status = "updated"
                report["updated"].append(placement["actor_label"])
            _configure_actor(actor, placement)
        placed_actors[placement["actor_label"]] = actor
        report["placements"].append({
            "asset": placement["asset_path"],
            "actor_label": placement["actor_label"],
            "folder": placement["folder"],
            "x": float(placement["location"].x),
            "y": float(placement["location"].y),
            "z": float(placement["location"].z),
            "yaw": placement["yaw"],
            "terrain_actor": placement["terrain"]["actor"],
            "terrain_z": placement["terrain"]["z"],
            "status": status,
        })

    if report["player_start_deferred"]:
        return placed_actors

    location = player_start.get_actor_location()
    rotation = player_start.get_actor_rotation()
    if not (_near(location.x, 0.0) and _near(location.y, 0.0) and _near(location.z, 0.0)
            and _near(rotation.pitch, 0.0) and _near(rotation.yaw % 360.0, 90.0)
            and _near(rotation.roll, 0.0)):
        player_start.modify()
        player_start.set_actor_location(unreal.Vector(0.0, 0.0, 0.0), False, True)
        player_start.set_actor_rotation(
            unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0), False)
        report["player_start_updated"] = True
    return placed_actors


def _verify_placements(actor_subsystem, planned, placed_actors, report):
    by_label = {}
    for actor in actor_subsystem.get_all_level_actors():
        label = actor.get_actor_label()
        if label in {placement["actor_label"] for placement in planned}:
            by_label.setdefault(label, []).append(actor)
    for placement in planned:
        actors = by_label.get(placement["actor_label"], [])
        exact_actor = placed_actors.get(placement["actor_label"])
        differences = _actor_differences(exact_actor, placement) if exact_actor else {
            "fields": ["actor_reference"], "expected": {}, "actual": {}}
        if len(actors) != 1 or actors[0] != exact_actor or differences:
            report["verification_failures"].append({
                "actor_label": placement["actor_label"],
                "lookup_count": len(actors),
                "lookup_is_exact_actor": len(actors) == 1 and actors[0] == exact_actor,
                "differences": differences,
            })
            report["errors"].append(
                "Post-placement verification failed for {}".format(placement["actor_label"]))
        else:
            report["verified"].append(placement["actor_label"])


def _write_report(report):
    folder = os.path.normpath(os.path.join(unreal.Paths.project_saved_dir(), REPORT_FOLDER))
    os.makedirs(folder, exist_ok=True)
    timestamp = datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%S%fZ")
    path = os.path.join(folder, "VillageMainPlacement_{}.json".format(timestamp))
    with open(path, "x", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
    return os.path.normpath(path)


def run(validation_only=VALIDATION_ONLY):
    report = {
        "target_map": TARGET_MAP,
        "loaded_world": "",
        "created": [],
        "updated": [],
        "unchanged": [],
        "placements": [],
        "missing_assets": [],
        "terrain_issues": [],
        "approximate_overlaps": [],
        "building_overlaps": [],
        "central_road_conflicts": [],
        "activity_clearances_cm": {},
        "player_start_updated": False,
        "player_start_issue": "",
        "player_start_deferred": False,
        "h7_adjustment": {},
        "terrain_actor_bounds": [],
        "verified": [],
        "verification_failures": [],
        "errors": [],
        "saved": False,
        "validation_only": bool(validation_only),
    }

    world = _validate_world(report)
    assets = _load_assets(report) if world else {}
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem) if world else None
    existing_actors = list(actor_subsystem.get_all_level_actors()) if actor_subsystem else []
    for actor in existing_actors:
        if _is_reliable_terrain(actor, None):
            try:
                center, extent = actor.get_actor_bounds(False, False)
                report["terrain_actor_bounds"].append({
                    "label": actor.get_actor_label(),
                    "class": actor.get_class().get_name(),
                    "center": _vector_dict(center),
                    "extent": _vector_dict(extent),
                })
            except Exception:
                pass
    managed = _preflight_managed_actors(existing_actors, report) if world else {}
    planned = _preflight_placements(
        world, assets, managed, report) if world and not report["missing_assets"] else []
    player_start = _preflight_player_start(world, existing_actors, report) if world else None
    _planned_overlaps(
        planned, existing_actors, {placement[1] for placement in PLACEMENTS}, report)
    _layout_space_checks(planned, report)
    if report["building_overlaps"]:
        report["errors"].append("Proposed building footprints overlap")
    if report["central_road_conflicts"]:
        report["errors"].append("Proposed buildings obstruct the reserved central road corridor")

    if report["errors"] or len(planned) != len(PLACEMENTS) or not player_start:
        report_path = _write_report(report)
        unreal.log_error("{} Preflight failed; no actors changed and no level saved. Report={}".format(
            LOG_PREFIX, report_path))
        return report

    report["placements"] = [{
        "asset": placement["asset_path"],
        "actor_label": placement["actor_label"],
        "folder": placement["folder"],
        "x": float(placement["location"].x),
        "y": float(placement["location"].y),
        "z": float(placement["location"].z),
        "yaw": placement["yaw"],
        "terrain_actor": placement["terrain"]["actor"],
        "terrain_z": placement["terrain"]["z"],
        "status": "proposed",
    } for placement in planned]

    if validation_only:
        report_path = _write_report(report)
        unreal.log(
            "{} Validation complete: placements={}, overlaps={}, saved=False; report={}".format(
                LOG_PREFIX, len(planned), len(report["approximate_overlaps"]), report_path))
        return report

    report["placements"] = []
    placed_actors = _place(actor_subsystem, planned, managed, player_start, report)
    _verify_placements(actor_subsystem, planned, placed_actors, report)
    if report["errors"] or len(report["verified"]) != len(PLACEMENTS):
        report_path = _write_report(report)
        unreal.log_error("{} Placement verification failed; level not saved. Report={}".format(
            LOG_PREFIX, report_path))
        return report
    if _package_path(_current_world()) != TARGET_MAP:
        raise RuntimeError("Loaded world changed during placement; refusing to save")
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save only {}".format(TARGET_MAP))
    report["saved"] = True
    report_path = _write_report(report)
    unreal.log(
        "{} Complete: created={}, updated={}, unchanged={}, overlaps={}, saved={}; report={}".format(
            LOG_PREFIX, len(report["created"]), len(report["updated"]),
            len(report["unchanged"]), len(report["approximate_overlaps"]),
            report["saved"], report_path))
    return report


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG_PREFIX, traceback.format_exc()))
        raise
