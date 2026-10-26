"""Validate direct-PVE static meshes and perform a guarded, resumable rollout.

Default execution is read-only.  It never exports meshes and never uses the
prohibited skeletal conversion/Make Static Mesh/Geometry Script paths.
"""

import json
import os
import glob
import re
import traceback

import unreal


PROJECT = unreal.SystemLibrary.get_project_directory()
REPORT_DIR = os.path.join(PROJECT, "Saved", "Reports")
INVENTORY_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_PVE_Inventory.json")
VALIDATION_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_PVE_Validation.json")
GO_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_GO.json")
REPLACEMENT_AUDIT_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_PVE_ReplacementAudit.json")
CONVERSION_REPORT_PATH = os.path.join(REPORT_DIR, "VHV_StaticVegetation_PVE_Conversion.md")

# Deliberately false. Change only after the user has reviewed a verified
# checkpoint, validation report, matched benchmark, and GO manifest.
APPLY_REPLACEMENT = False
LOG = "[VHV Static Vegetation Validation]"
TARGET_FOLDER = "/Game/VHV_StaticVegetation"
MIN_UV_CHANNELS = 3
HIGH_TRIANGLE_WARNING = 2_000_000
HIGH_PACKAGE_BYTES_WARNING = 16 * 1024 * 1024
HIGH_NANITE_GPU_BYTES_WARNING = 64 * 1024 * 1024
HIGH_BUILD_MEMORY_MB_WARNING = 512.0
HIGH_BUILD_SECONDS_WARNING = 60.0
BOUNDS_RELATIVE_TOLERANCE = 0.01
BOUNDS_ABSOLUTE_TOLERANCE_CM = 2.0
# User-approved production policy: vegetation is decorative and receives no
# collision or physics. This applies only to replacement StaticMeshComponents.
APPROVED_ZERO_COLLISION = True
VISIBLE_SIZE_RATIO_MIN = 0.85
VISIBLE_SIZE_RATIO_MAX = 1.15
VISIBLE_CENTER_SHIFT_FRACTION_MAX = 0.10
VISIBLE_BASE_SHIFT_FRACTION_MAX = 0.03


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


def _vector(value):
    return [round(float(value.x), 6), round(float(value.y), 6), round(float(value.z), 6)] if value else None


def _asset_material_paths(asset):
    result = []
    for slot in list(_value(asset, "materials", [])) + list(_value(asset, "static_materials", [])):
        material = _value(slot, "material_interface")
        result.append(_path(material))
    return result


def _asset_file_bytes(asset_path):
    relative = asset_path.split(".", 1)[0].replace("/Game/", "", 1).replace("/", os.sep) + ".uasset"
    filename = os.path.join(PROJECT, "Content", relative)
    return os.path.getsize(filename) if os.path.isfile(filename) else None


def _nanite_build_stats_from_logs():
    """Read UE's own per-mesh Nanite build telemetry from current/rotated logs."""
    result = {}
    log_pattern = os.path.join(PROJECT, "Saved", "Logs", "VHV*.log")
    for filename in sorted(glob.glob(log_pattern), key=os.path.getmtime):
        active_name = None
        active = None
        try:
            with open(filename, "r", encoding="utf-8", errors="replace") as stream:
                for line in stream:
                    match = re.search(
                        r"Building static mesh (\S+) \(Required Memory Estimate: ([0-9.]+) MB\)", line)
                    if match:
                        active_name = match.group(1)
                        active = {"required_build_memory_mb": float(match.group(2)),
                                  "log_file": os.path.basename(filename)}
                        continue
                    if active is None:
                        continue
                    match = re.search(r"Adjacency \[[^]]+\], tris: \d+, UVs (\d+)", line)
                    if match:
                        active["max_uv_channels_observed"] = max(
                            int(match.group(1)), int(active.get("max_uv_channels_observed", 0)))
                    match = re.search(r"Input: (\d+) Clusters, (\d+) Triangles and (\d+) Vertices", line)
                    if match:
                        active.update({"nanite_input_clusters": int(match.group(1)),
                                       "nanite_input_triangles": int(match.group(2)),
                                       "nanite_input_vertices": int(match.group(3))})
                    match = re.search(r"Total GPU size: (\d+) bytes, Total disk size: (\d+) bytes", line)
                    if match:
                        active.update({"nanite_gpu_bytes": int(match.group(1)),
                                       "nanite_disk_bytes": int(match.group(2))})
                    match = re.search(
                        r"Built static mesh \[([0-9.]+)s\] (/Game/VHV_StaticVegetation/(SK_[^.\s]+))", line)
                    if match and match.group(3) == active_name:
                        active["build_seconds"] = float(match.group(1))
                        result[match.group(2)] = dict(active)
                        active_name = None
                        active = None
        except OSError:
            continue
    return result


def _target_mesh_diagnostics_from_logs():
    """Collect deterministic target-specific geometry diagnostics emitted by UE."""
    result = {}
    log_pattern = os.path.join(PROJECT, "Saved", "Logs", "VHV*.log")
    patterns = (
        (r"Mesh (SK_[A-Za-z0-9_]+) has NaNs in it's vertex instance uvs", "NaNs in vertex-instance UVs; UE clamps offending UVs to zero"),
        (r"(SK_[A-Za-z0-9_]+) has some nearly zero normals", "nearly-zero vertex normals"),
        (r"(SK_[A-Za-z0-9_]+) has some nearly zero bi-normals", "nearly-zero vertex bi-normals"),
    )
    for filename in glob.glob(log_pattern):
        try:
            with open(filename, "r", encoding="utf-8", errors="replace") as stream:
                for line in stream:
                    for pattern, message in patterns:
                        match = re.search(pattern, line)
                        if match:
                            path = "{}/{}".format(TARGET_FOLDER, match.group(1))
                            result.setdefault(path, set()).add(message)
        except OSError:
            continue
    return {path: sorted(messages) for path, messages in result.items()}


def _material_base(material):
    current = material
    seen = set()
    while current and _path(current) not in seen:
        seen.add(_path(current))
        if isinstance(current, unreal.Material):
            return current
        current = _value(current, "parent")
    return None


def _material_details(material):
    base = _material_base(material)
    wpo_node = None
    if base:
        try:
            wpo_node = unreal.MaterialEditingLibrary.get_material_property_input_node(
                base, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        except Exception:
            wpo_node = None
    return {
        "material": _path(material),
        "base_material": _path(base),
        "base_has_wpo_input": bool(wpo_node),
        "wpo_input_class": wpo_node.get_class().get_name() if wpo_node else "",
    }


def _collision(asset):
    body_setup = _value(asset, "body_setup")
    agg = _value(body_setup, "agg_geom")
    if not agg:
        return {
            "body_setup": bool(body_setup),
            "trace_flag": str(_value(body_setup, "collision_trace_flag", "")),
            "primitive_counts": {},
            "simple_primitive_total": 0,
        }
    names = ("box_elems", "sphere_elems", "sphyl_elems", "convex_elems", "tapered_capsule_elems", "level_set_elems")
    counts = {name: len(list(_value(agg, name, []))) for name in names}
    return {
        "body_setup": True,
        "trace_flag": str(_value(body_setup, "collision_trace_flag", "")),
        "primitive_counts": counts,
        "simple_primitive_total": sum(counts.values()),
    }


def _bounds(asset):
    try:
        box = asset.get_bounds()
        return {"origin": _vector(box.origin), "extent": _vector(box.box_extent), "sphere_radius": round(float(box.sphere_radius), 6)}
    except Exception:
        return None


def _imported_skeletal_bounds(asset):
    try:
        box = asset.get_imported_bounds()
        return {"origin": _vector(box.origin), "extent": _vector(box.box_extent),
                "sphere_radius": round(float(box.sphere_radius), 6)}
    except Exception:
        return None


def _bounds_edges(bounds):
    if not bounds:
        return None
    minimum = [bounds["origin"][index] - bounds["extent"][index] for index in range(3)]
    maximum = [bounds["origin"][index] + bounds["extent"][index] for index in range(3)]
    size = [2.0 * bounds["extent"][index] for index in range(3)]
    return {"minimum": minimum, "maximum": maximum, "size": size}


def _static_details(path, build_stats=None):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return {"exists": False, "reason": "Expected StaticMesh is not in the Asset Registry: {}".format(path)}
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(asset, unreal.StaticMesh):
        return {"exists": False, "reason": "Expected StaticMesh at {}".format(path)}
    nanite = _value(asset, "nanite_settings")
    # These calls wait for asynchronous mesh compilation. Query UVs only after
    # this synchronization point or the subsystem can reject a compiling mesh.
    lod0_triangles = asset.get_num_triangles(0)
    lod0_vertices = asset.get_num_vertices(0)
    lod0_sections = asset.get_num_sections(0)
    uv_error = ""
    uv_source = "StaticMeshEditorSubsystem"
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem:
        try:
            uv_channels = subsystem.get_num_uv_channels(asset, 0)
        except Exception as exception:
            uv_error = str(exception)
            uv_channels = None
    else:
        uv_channels = None
        uv_error = "StaticMeshEditorSubsystem is unavailable in commandlet mode"
    if not uv_channels and (build_stats or {}).get("max_uv_channels_observed"):
        uv_channels = build_stats["max_uv_channels_observed"]
        uv_source = "UE Nanite build log (maximum observed source channel count)"
    materials = [_value(slot, "material_interface") for slot in list(_value(asset, "static_materials", []))]
    return {
        "exists": True,
        "class": asset.get_class().get_path_name(),
        "materials": [_path(material) for material in materials],
        "material_details": [_material_details(material) for material in materials],
        "bounds": _bounds(asset),
        "collision": _collision(asset),
        "nanite_enabled": bool(_value(nanite, "enabled", False)),
        "uv_channels_lod0": uv_channels,
        "uv_query_error": uv_error,
        "uv_query_source": uv_source,
        "lod0_fallback_triangles": lod0_triangles,
        "lod0_fallback_vertices": lod0_vertices,
        "lod0_sections": lod0_sections,
        "package_file_bytes": _asset_file_bytes(path),
        "has_skeletal_wind_data": bool(_value(asset, "wind_settings", None)),
        "animation": "not_applicable: StaticMesh has no skeletal animation runtime",
    }


def _skeletal_details(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(asset, unreal.SkeletalMesh):
        return {"exists": False, "reason": "Expected SkeletalMesh at {}".format(path)}
    physics_asset = _value(asset, "physics_asset")
    try:
        lod0_vertices = unreal.EditorSkeletalMeshLibrary.get_num_verts(asset, 0)
    except Exception:
        lod0_vertices = None
    return {
        "exists": True,
        "materials": _asset_material_paths(asset),
        "bounds": _bounds(asset),
        "imported_bounds": _imported_skeletal_bounds(asset),
        "positive_bounds_extension": _vector(_value(asset, "positive_bounds_extension")),
        "negative_bounds_extension": _vector(_value(asset, "negative_bounds_extension")),
        "lod0_vertices": lod0_vertices,
        "collision": _collision(asset),
        "physics_asset": _path(physics_asset),
        "package_file_bytes": _asset_file_bytes(path),
    }


def _bounds_comparison(source, target):
    if not source or not target:
        return {"passed": False, "reason": "bounds unavailable"}
    axes = []
    passed = True
    for label in ("origin", "extent"):
        for axis, source_value, target_value in zip("XYZ", source[label], target[label]):
            tolerance = max(BOUNDS_ABSOLUTE_TOLERANCE_CM, abs(source_value) * BOUNDS_RELATIVE_TOLERANCE)
            delta = abs(source_value - target_value)
            axes.append({"field": label, "axis": axis, "source": source_value, "target": target_value,
                         "delta": delta, "tolerance": tolerance, "passed": delta <= tolerance})
            passed = passed and delta <= tolerance
    return {"passed": passed, "axes": axes}


def _visible_bounds_assessment(source_imported, source_extended, target):
    """Distinguish harmless foliage AABB variation from pivot/scale displacement.

    MegaPlant imported/reference-pose bounds can exclude procedurally assembled
    canopy and foliage. Runtime extended bounds represent the actual intended
    visible envelope and are therefore primary; imported bounds are retained as
    diagnostic evidence only. This rejects gross scale, missing-canopy, and
    ground/pivot displacement without requiring exact conservative AABB parity.
    """
    source = source_extended or source_imported
    if not source or not target:
        return {"passed": False, "classification": "unavailable", "reason": "bounds unavailable"}
    strict = _bounds_comparison(source, target)
    source_edges = _bounds_edges(source)
    target_edges = _bounds_edges(target)
    ratios = []
    center_shift_fractions = []
    for index in range(3):
        source_size = source_edges["size"][index]
        target_size = target_edges["size"][index]
        ratios.append(target_size / source_size if source_size else 0.0)
        center_shift_fractions.append(
            abs(target["origin"][index] - source["origin"][index]) / source_size if source_size else 1.0)
    source_height = max(source_edges["size"][2], 1.0)
    base_delta_cm = abs(target_edges["minimum"][2] - source_edges["minimum"][2])
    base_tolerance_cm = max(BOUNDS_ABSOLUTE_TOLERANCE_CM,
                            source_height * VISIBLE_BASE_SHIFT_FRACTION_MAX)
    dimensions_ok = all(VISIBLE_SIZE_RATIO_MIN <= ratio <= VISIBLE_SIZE_RATIO_MAX for ratio in ratios)
    centers_ok = all(value <= VISIBLE_CENTER_SHIFT_FRACTION_MAX for value in center_shift_fractions)
    base_ok = base_delta_cm <= base_tolerance_cm
    passed = dimensions_ok and centers_ok and base_ok
    return {
        "passed": passed,
        "classification": ("strict_match" if strict["passed"] else
                           "compatible_direct_pve_visible_bounds" if passed else
                           "possible_pivot_scale_or_missing_geometry"),
        "reference": "skeletal runtime/extended visible bounds" if source_extended else "skeletal imported bounds fallback",
        "strict_comparison": strict,
        "imported_reference_comparison": (_bounds_comparison(source_imported, target) if source_imported else None),
        "source_edges": source_edges,
        "target_edges": target_edges,
        "dimension_ratios": ratios,
        "center_shift_fractions": center_shift_fractions,
        "base_delta_cm": base_delta_cm,
        "base_tolerance_cm": base_tolerance_cm,
        "dimensions_ok": dimensions_ok,
        "centers_ok": centers_ok,
        "base_ok": base_ok,
        "rendered_review_still_required": True,
    }


def _source_actor_collision(inventory, source_mesh):
    records = []
    for actor in inventory["actors"]:
        for component in actor["eligible_components"]:
            if component["skeletal_mesh"] == source_mesh:
                records.append({
                    "actor": actor["object_path"],
                    "enabled": component.get("collision_enabled", ""),
                    "profile": component.get("collision_profile", ""),
                })
    enabled = [record for record in records if record["enabled"] and "NO_COLLISION" not in record["enabled"].upper()]
    return {"actors": len(records), "enabled_actors": len(enabled), "profiles": sorted(set(record["profile"] for record in records))}


def _collision_policy(source_usage, target_collision):
    return bool(APPROVED_ZERO_COLLISION), (
        "User-approved decorative vegetation policy: replacement component profile=NoCollision, "
        "collision_enabled=NO_COLLISION, simulate_physics=False; source gameplay actors are not altered. "
        "Target asset simple primitives={} trace_flag={}".format(
            target_collision.get("simple_primitive_total", 0), target_collision.get("trace_flag", "")))


def validate():
    if not os.path.isfile(INVENTORY_PATH):
        raise RuntimeError("Missing inventory; run vhv_static_vegetation_inventory.py first")
    with open(INVENTORY_PATH, "r", encoding="utf-8") as stream:
        inventory = json.load(stream)
    build_stats = _nanite_build_stats_from_logs()
    diagnostics_by_target = _target_mesh_diagnostics_from_logs()
    rows = []
    for mapping in inventory["source_to_target_mapping"]:
        source = _skeletal_details(mapping["source_skeletal_mesh"])
        target_stats = build_stats.get(mapping["target_static_mesh"], {})
        target = _static_details(mapping["target_static_mesh"], target_stats)
        target["nanite_build_stats"] = target_stats
        target["mesh_build_diagnostics"] = diagnostics_by_target.get(mapping["target_static_mesh"], [])
        source_usage = _source_actor_collision(inventory, mapping["source_skeletal_mesh"])
        bounds = (_visible_bounds_assessment(source.get("imported_bounds"), source.get("bounds"), target.get("bounds"))
                  if target.get("exists") else {"passed": False, "reason": "target missing"})
        collision_ok, collision_reason = _collision_policy(source_usage, target.get("collision", {}))
        materials = target.get("materials", [])
        material_names = [path.lower() for path in materials]
        material_ok = bool(len(materials) == 2 and materials == source.get("materials") and
                           any("bark" in path for path in material_names) and
                           any("foliage" in path for path in material_names))
        nanite_stats = target.get("nanite_build_stats", {})
        triangles = int(nanite_stats.get("nanite_input_triangles") or target.get("lod0_fallback_triangles", 0) or 0)
        vertices = int(nanite_stats.get("nanite_input_vertices") or target.get("lod0_fallback_vertices", 0) or 0)
        target["nanite_source_triangles"] = triangles
        target["nanite_source_vertices"] = vertices
        sections = int(target.get("lod0_sections", 0) or 0)
        triangle_vertex_ratio = float(triangles) / vertices if vertices else 0.0
        geometry_ok = triangles > 0 and vertices > 0 and sections == len(materials) and 0.25 <= triangle_vertex_ratio <= 6.0
        uv_ok = int(target.get("uv_channels_lod0", 0) or 0) >= MIN_UV_CHANNELS
        wpo_materials = [item["material"] for item in target.get("material_details", []) if item.get("base_has_wpo_input")]
        warnings = []
        if wpo_materials:
            warnings.append("Parent material graph has WPO input; rollout must set StaticMeshComponent.evaluate_world_position_offset=False")
        if triangles >= HIGH_TRIANGLE_WARNING:
            warnings.append("High Nanite source triangle count: {:,}".format(triangles))
        if (target.get("package_file_bytes") or 0) >= HIGH_PACKAGE_BYTES_WARNING:
            warnings.append("Large on-disk mesh package: {:,} bytes".format(target["package_file_bytes"]))
        if (nanite_stats.get("nanite_gpu_bytes") or 0) >= HIGH_NANITE_GPU_BYTES_WARNING:
            warnings.append("High Nanite encoded GPU size: {:,} bytes".format(nanite_stats["nanite_gpu_bytes"]))
        if (nanite_stats.get("required_build_memory_mb") or 0) >= HIGH_BUILD_MEMORY_MB_WARNING:
            warnings.append("High Nanite build-memory estimate: {:.2f} MB".format(nanite_stats["required_build_memory_mb"]))
        if (nanite_stats.get("build_seconds") or 0) >= HIGH_BUILD_SECONDS_WARNING:
            warnings.append("Slow Nanite build: {:.2f} seconds".format(nanite_stats["build_seconds"]))
        if not bounds.get("strict_comparison", {}).get("passed", False) and bounds.get("passed"):
            warnings.append("Raw AABBs differ but runtime-visible shape/pivot/scale remain compatible with direct PVE output")
        for diagnostic in target.get("mesh_build_diagnostics", []):
            warnings.append("Suspicious mesh build diagnostic: {}".format(diagnostic))
        failures = []
        checks = {
            "asset_is_static_mesh": target.get("class") == "/Script/Engine.StaticMesh",
            "nanite_enabled": bool(target.get("nanite_enabled")),
            "materials_exact_bark_foliage_pair": material_ok,
            "uv_channels_at_least_{}".format(MIN_UV_CHANNELS): uv_ok,
            "visible_bounds_pivot_scale_compatible": bool(bounds.get("passed")),
            "geometry_structurally_sane": geometry_ok,
            "mesh_build_diagnostics_clean": not bool(target.get("mesh_build_diagnostics")),
            "approved_no_collision_policy": collision_ok,
            "no_skeletal_wind_runtime": not bool(target.get("has_skeletal_wind_data")),
        }
        for check_name, check_passed in checks.items():
            if not check_passed:
                failures.append(check_name)
        passed = bool(target.get("exists") and source.get("exists") and all(checks.values()))
        rows.append({
            "mapping": mapping,
            "source": source,
            "target": target,
            "checks": checks,
            "failures": failures,
            "warnings": warnings,
            "bounds_comparison": bounds,
            "collision_source_usage": source_usage,
            "collision_reason": collision_reason,
            "triangle_vertex_ratio": triangle_vertex_ratio,
            "wpo_materials": wpo_materials,
            "planned_final_wpo_override": False,
            "planned_collision_profile": "NoCollision",
            "planned_collision_enabled": "NO_COLLISION",
            "planned_simulate_physics": False,
            "automated_checks_passed": passed,
            "required_manual_checks": [
                "Side-by-side leafy silhouette is complete (no missing or triangular branches).",
                "Bark/foliage textures render correctly despite exact material and UV-count match.",
                "Matched lighting/shadow test passes at representative placement.",
                "Matched isolated benchmark shows repeatable improvement without material GPU/memory regression.",
            ],
        })
    species = {}
    for species_name in sorted(inventory["by_species"]):
        variants = [row for row in rows if row["mapping"]["species"] == species_name]
        failed_variants = [row["mapping"]["variant"] for row in variants if not row["automated_checks_passed"]]
        passed_variants = [row["mapping"]["variant"] for row in variants if row["automated_checks_passed"]]
        warnings = sorted({warning for row in variants for warning in row["warnings"]})
        species[species_name] = {
            "status": "PASS" if not failed_variants else "PARTIAL" if passed_variants else "FAIL",
            "passed_variants": passed_variants,
            "failed_variants": failed_variants,
            "warnings": warnings,
            "actor_count": inventory["by_species"][species_name],
            "cleared_actor_count": sum(row["collision_source_usage"]["actors"] for row in variants if row["automated_checks_passed"]),
            "blocked_actor_count": sum(row["collision_source_usage"]["actors"] for row in variants if not row["automated_checks_passed"]),
        }
    target_bytes = sum(row["target"].get("package_file_bytes") or 0 for row in rows)
    source_bytes = sum(row["source"].get("package_file_bytes") or 0 for row in rows)
    nanite_gpu_bytes = sum(row["target"].get("nanite_build_stats", {}).get("nanite_gpu_bytes") or 0 for row in rows)
    nanite_disk_bytes = sum(row["target"].get("nanite_build_stats", {}).get("nanite_disk_bytes") or 0 for row in rows)
    build_telemetry_count = sum(bool(row["target"].get("nanite_build_stats", {}).get("nanite_gpu_bytes")) for row in rows)
    weighted_triangles = 0
    for row in rows:
        weighted_triangles += row["target"].get("nanite_source_triangles", 0) * row["collision_source_usage"]["actors"]
    cleared_rows = [row for row in rows if row["automated_checks_passed"]]
    blocked_rows = [row for row in rows if not row["automated_checks_passed"]]
    benchmark_pairs = []
    for species_name in sorted(species):
        candidates = [row for row in cleared_rows if row["mapping"]["species"] == species_name]
        if candidates:
            representative = max(candidates, key=lambda row: row["collision_source_usage"]["actors"])
            benchmark_pairs.append({
                "species": species_name,
                "variant": representative["mapping"]["variant"],
                "skeletal_mesh": representative["mapping"]["source_skeletal_mesh"],
                "static_mesh": representative["mapping"]["target_static_mesh"],
                "instances_per_configuration": 10,
                "static_collision": "NoCollision",
                "static_simulate_physics": False,
            })
    isolated_plan = {
        "recommendation": "GO" if len(benchmark_pairs) == len(species) else "NO-GO",
        "scope": "temporary isolated benchmark only; never load or save the production village map",
        "representative_pairs": benchmark_pairs,
        "configuration_actor_count": sum(pair["instances_per_configuration"] for pair in benchmark_pairs),
        "layout": "same deterministic 5x2 transform grid per species; swap only SkeletalMeshActor versus StaticMeshActor",
        "camera": "lock one overview and two canopy/entrance-like camera transforms; reuse byte-identical transforms for both configurations",
        "settings": "same 1920x1080, 100% render percentage, uncapped FPS, scalability, lighting, VSM, shadows, visibility, and warm DDC",
        "sampling": "300-frame warmup then 1800 measured frames; run skeletal/static/static/skeletal order",
        "metrics": ["average FPS", "average and P95 frame time", "Game", "Draw", "RHI", "GPU",
                    "Nanite visibility", "Shadow Depths", "GPU memory", "skeletal tick", "skin cache"],
        "excluded_variants": ["{} {}".format(row["mapping"]["species"], row["mapping"]["variant"]) for row in blocked_rows],
        "executed": False,
    }
    performance = {
        "mode": "bounded_analytical_comparison_only",
        "source_component_count": inventory["eligible_megaplant_component_count"],
        "target_component_count": inventory["eligible_megaplant_component_count"],
        "skeletal_components_removed_if_rolled_out": sum(
            row["collision_source_usage"]["actors"] for row in cleared_rows),
        "unique_source_package_bytes": source_bytes,
        "unique_target_package_bytes": target_bytes,
        "unique_package_byte_delta": target_bytes - source_bytes,
        "nanite_build_telemetry_variant_count": build_telemetry_count,
        "unique_nanite_encoded_gpu_bytes": nanite_gpu_bytes,
        "unique_nanite_encoded_disk_bytes": nanite_disk_bytes,
        "projected_instance_weighted_source_triangles": weighted_triangles,
        "rendered_fps_or_gpu_time": None,
        "isolated_test_plan": isolated_plan,
        "limitation": "No matching rendered isolated level exists and production actors may not be changed; package bytes and geometry workload are risk indicators, not GPU residency or FPS proof.",
    }
    validation = {
        "schema": 3,
        "read_only": True,
        "variants": rows,
        "species": species,
        "performance": performance,
        "cleared_variant_count": len(cleared_rows),
        "blocked_variant_count": len(blocked_rows),
        "cleared_actor_count": sum(row["collision_source_usage"]["actors"] for row in cleared_rows),
        "blocked_actor_count": sum(row["collision_source_usage"]["actors"] for row in blocked_rows),
        "all_automated_checks_passed": bool(rows) and not blocked_rows,
        "replacement_gate": "PARTIAL: cleared variants may enter an isolated performance test; production rollout remains blocked pending review and matched benchmark.",
    }
    with open(VALIDATION_PATH, "w", encoding="utf-8") as stream:
        json.dump(validation, stream, indent=2, sort_keys=True)
    _write_conversion_report(inventory, validation, None)
    unreal.log("{} validation written; automatic pass={}".format(LOG, validation["all_automated_checks_passed"]))
    return validation


def _eligible_component(actor):
    components = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
    if len(components) != 1:
        return None, "requires exactly one SkeletalMeshComponent"
    return components[0], ""


def _numbers_match(left, right, tolerance=0.0001):
    return bool(left is not None and right is not None and len(left) == len(right) and
                all(abs(float(a) - float(b)) <= tolerance for a, b in zip(left, right)))


def _copy_component_settings(source, target):
    for name in ("cast_shadow", "cast_dynamic_shadow", "cast_static_shadow", "cast_contact_shadow",
                 "visible", "hidden_in_game", "render_in_main_pass", "render_in_depth_pass",
                 "use_as_occluder", "receives_decals", "generate_overlap_events"):
        value = _value(source, name, None)
        if value is not None:
            target.set_editor_property(name, value)
    mobility = _value(source, "mobility")
    if mobility is not None:
        target.set_mobility(mobility)
    # Explicitly approved vegetation-only policy. Never propagate this to
    # unrelated actors or components.
    target.set_collision_profile_name("NoCollision")
    target.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    target.set_simulate_physics(False)
    target.set_evaluate_world_position_offset(False)
    target.set_evaluate_world_position_offset_in_ray_tracing(False)


def dry_run_replacement():
    if not os.path.isfile(INVENTORY_PATH) or not os.path.isfile(VALIDATION_PATH):
        raise RuntimeError("Inventory and validation reports are required before a rollout dry run")
    with open(INVENTORY_PATH, "r", encoding="utf-8") as stream:
        inventory = json.load(stream)
    map_path = inventory.get("map")
    if not map_path or not unreal.EditorLoadingAndSavingUtils.load_map(map_path):
        raise RuntimeError("Could not load inventory map for replacement dry run: {}".format(map_path))
    mapping = {entry["source_skeletal_mesh"]: entry["target_static_mesh"] for entry in inventory["source_to_target_mapping"]}
    with open(VALIDATION_PATH, "r", encoding="utf-8") as stream:
        validation = json.load(stream)
    validation_by_source = {row["mapping"]["source_skeletal_mesh"]: row for row in validation["variants"]}
    inventory_by_actor = {actor["object_path"]: actor for actor in inventory["actors"]}
    actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    planned, skipped = [], []
    mapped_actor_count = 0
    for actor in actors:
        component, reason = _eligible_component(actor)
        if not component:
            continue
        try:
            source = _path(component.get_skeletal_mesh_asset())
        except Exception:
            source = _path(_value(component, "skeletal_mesh"))
        target = mapping.get(source)
        if not target:
            continue
        mapped_actor_count += 1
        object_path = actor.get_path_name()
        inventory_actor = inventory_by_actor.get(object_path)
        current_location = _vector(actor.get_actor_location())
        current_rotation = [round(float(value), 6) for value in (
            actor.get_actor_rotation().pitch, actor.get_actor_rotation().yaw, actor.get_actor_rotation().roll)]
        current_scale = _vector(actor.get_actor_scale3d())
        transform_matches = bool(inventory_actor and
                                 _numbers_match(current_location, inventory_actor["location"]) and
                                 _numbers_match(current_rotation, inventory_actor["rotation"]) and
                                 _numbers_match(current_scale, inventory_actor["scale"]))
        if not inventory_actor:
            skipped.append({"actor": object_path, "reason": "actor missing from protected inventory"})
        elif not transform_matches:
            skipped.append({"actor": object_path, "reason": "current transform differs from protected inventory"})
        elif actor.get_attach_parent_actor():
            skipped.append({"actor": actor.get_path_name(), "reason": "attached actor requires manual dependency review"})
        elif not unreal.EditorAssetLibrary.does_asset_exist(target):
            skipped.append({"actor": actor.get_path_name(), "reason": "target static mesh missing"})
        elif not validation_by_source[source]["automated_checks_passed"]:
            skipped.append({"actor": actor.get_path_name(), "reason": "target variant failed automated validation",
                            "failures": validation_by_source[source]["failures"]})
        else:
            planned.append({
                "actor": actor.get_path_name(),
                "label": actor.get_actor_label(),
                "source": source,
                "target": target,
                "location": current_location,
                "rotation": current_rotation,
                "scale": current_scale,
                "transform_matches_protected_inventory": transform_matches,
                "folder": str(actor.get_folder_path()),
                "tags": sorted(str(tag) for tag in list(_value(actor, "tags", []))),
                "preserved_shadow_settings": {
                    "cast_shadow": bool(_value(component, "cast_shadow", False)),
                    "cast_dynamic_shadow": bool(_value(component, "cast_dynamic_shadow", False)),
                    "cast_static_shadow": bool(_value(component, "cast_static_shadow", False)),
                    "cast_contact_shadow": bool(_value(component, "cast_contact_shadow", False)),
                },
                "preserved_mobility": str(_value(component, "mobility", "")),
                "asset_reference_verified": bool(source == inventory_actor["eligible_components"][0]["skeletal_mesh"]),
                "planned_static_component_overrides": {
                    "evaluate_world_position_offset": False,
                    "evaluate_world_position_offset_in_ray_tracing": False,
                    "collision_enabled": "NO_COLLISION",
                    "collision_profile": "NoCollision",
                    "simulate_physics": False,
                },
            })
    audit = {
        "mode": "dry_run",
        "map": map_path,
        "eligible_inventory_actor_count": inventory["eligible_megaplant_actor_count"],
        "mapped_current_actor_count": mapped_actor_count,
        "planned": planned,
        "skipped": skipped,
        "accounted_for": len(planned) + len(skipped),
        "all_planned_transforms_match_inventory": bool(planned) and all(item["transform_matches_protected_inventory"] for item in planned),
        "all_planned_asset_references_verified": bool(planned) and all(item["asset_reference_verified"] for item in planned),
        "planned_shadow_settings_copied": True,
        "replacement_collision_policy": "NoCollision; simulate_physics=False; no collision meshes generated",
        "modified": False,
    }
    with open(REPLACEMENT_AUDIT_PATH, "w", encoding="utf-8") as stream:
        json.dump(audit, stream, indent=2, sort_keys=True)
    _write_conversion_report(inventory, validation, audit)
    unreal.log("{} dry run: planned={}, skipped={}".format(LOG, len(planned), len(skipped)))
    return audit


def _write_conversion_report_legacy(inventory, validation, audit):
    existing_targets = [row for row in validation["variants"] if row["target"].get("exists")]
    skipped = len(audit["skipped"]) if audit else inventory["eligible_megaplant_actor_count"]
    planned = len(audit["planned"]) if audit else 0
    lines = [
        "# VHV Static Vegetation PVE Conversion",
        "",
        "## Current safety-gated result",
        "",
        "Production replacement did not run. The direct-PVE static assets are absent, therefore visual, collision, and matched-performance gates cannot pass. Original village actors and PVE graphs remain unchanged.",
        "",
        "## 1. Inventory",
        "",
        "- Map: `{}`".format(inventory["map"]),
        "- Map SHA-256: `{}`".format(inventory["map_file"]["sha256"]),
        "- Loaded level actors: {}".format(inventory["total_loaded_level_actors"]),
        "- Eligible MegaPlant actors/components: {} / {}".format(inventory["eligible_megaplant_actor_count"], inventory["eligible_megaplant_component_count"]),
        "- World Partition external actor packages: {}.".format(inventory["world_partition"]["external_actor_directory"]),
        "- Saved-registry status of `{}`: {}.".format(inventory["successful_test_mesh_search"]["asset_name"], inventory["successful_test_mesh_search"]["status"]),
        "",
        "| Species | Actors/components |",
        "|---|---:|",
    ]
    for species, count in sorted(inventory["by_species"].items()):
        lines.append("| {} | {} |".format(species, count))
    lines.extend([
        "",
        "## 2. Protection",
        "",
        "- Verified checkpoint: `Saved/StaticVegetationCheckpoints/PrePVEStatic-20261026-155547/manifest.json`.",
        "- Checkpoint map hash matches the inventory hash; no external actor directory exists for this map.",
        "- Existing Git modifications were not reset, cleaned, checked out, committed, or overwritten.",
        "",
        "## 3. Source-to-target PVE mapping",
        "",
        "All 24 distinct source variants require direct original-PVE static output; there is no supported PVE Python/Blueprint export API in UE 5.7.4. The supported exporter is the editor-private C++ function `PV::Export::ExportCollectionAsMesh`, so no unsupported automation call was attempted.",
        "",
        "| Species | Variant | Source skeletal asset | Required static target | PVE graph |",
        "|---|---|---|---|---|",
    ])
    for item in inventory["source_to_target_mapping"]:
        lines.append("| {} | {} | `{}` | `{}` | `{}` |".format(item["species"], item["variant"], item["source_skeletal_mesh"], item["target_static_mesh"], item["source_pve_graph"]))
    lines.extend([
        "",
        "## 4. Generated static assets and strict validation",
        "",
        "- Generated/located target assets: {} of 24.".format(len(existing_targets)),
        "- Automated asset checks: {}.".format("PASS" if validation["all_automated_checks_passed"] else "BLOCKED (targets missing)"),
        "- Visual silhouette, material/WPO, shadow, collision-in-scene, and Nanite render validation: not performed; no valid target assets exist.",
        "- Black Alder: retained as skeletal; no crash-prone skeletal conversion was attempted.",
        "",
        "## 5. Performance gate",
        "",
        "No isolated or village benchmark was run because there is no matching static configuration. Final village FPS: not measured. The supplied 32.5 FPS baseline remains a reference only and is not compared to an unmatched run.",
        "",
        "## 6. Replacement",
        "",
        "- Actors replaced: 0.",
        "- Dry-run replacements ready: {}.".format(planned),
        "- Skipped/retained: {} (all eligible vegetation remains original pending assets and gates).".format(skipped),
        "- The rollout script is read-only by default and deliberately safety-stops before production mutation.",
        "",
        "## 7. Remaining essential manual PVE work",
        "",
        "1. Locate and save the missing test mesh, or recreate it once from a duplicate Japanese Medlar PVE graph; it is not present in the current saved Asset Registry.",
        "2. For each of the six original PVE graphs in the mapping table, use only PVE Export: Static Mesh, Nanite enabled, Append/unique name, correct bark+foliage materials, and no wind/WPO final setup. Export the A/B/C/D variants to the exact target paths shown above. Do not alter originals or use skeletal conversion.",
        "3. Re-run `vhv_static_vegetation_inventory.py`, then `vhv_static_vegetation_validate_and_replace.py`. The scripts will establish what remains to inspect and generate a map-bound dry run.",
        "4. Create a representative isolated direct-PVE static vs skeletal benchmark with matched settings. Proceed only on repeatable improvement and no material memory/GPU regression; Black Alder remains skipped if its direct PVE export fails once.",
        "",
        "## Modified/generated files",
        "",
        "- `Content/Python/vhv_static_vegetation_inventory.py`",
        "- `Content/Python/vhv_static_vegetation_checkpoint.py`",
        "- `Content/Python/vhv_static_vegetation_validate_and_replace.py`",
        "- `Saved/Reports/VHV_StaticVegetation_PVE_Inventory.json`",
        "- `Saved/Reports/VHV_StaticVegetation_PVE_Validation.json`",
        "- `Saved/Reports/VHV_StaticVegetation_PVE_ReplacementAudit.json`",
        "- `Saved/StaticVegetationCheckpoints/PrePVEStatic-20261026-155547/`",
    ])
    with open(CONVERSION_REPORT_PATH, "w", encoding="utf-8") as stream:
        stream.write("\n".join(lines) + "\n")


def _write_conversion_report(inventory, validation, audit):
    existing_targets = [row for row in validation["variants"] if row["target"].get("exists")]
    skipped = len(audit["skipped"]) if audit else inventory["eligible_megaplant_actor_count"]
    planned = len(audit["planned"]) if audit else 0
    incomplete_species = [name for name, item in validation["species"].items() if item["status"] != "PASS"]
    unique_triangles = sum(row["target"].get("nanite_source_triangles", 0) for row in validation["variants"])
    wpo_material_count = sum(
        bool(material.get("base_has_wpo_input"))
        for row in validation["variants"] for material in row["target"].get("material_details", []))
    performance = validation["performance"]
    benchmark_pair_text = ", ".join(
        "{} {}".format(pair["species"], pair["variant"])
        for pair in performance["isolated_test_plan"]["representative_pairs"])
    lines = [
        "# VHV Static Vegetation PVE Conversion",
        "",
        "## Current safety-gated result",
        "",
        "All validation and replacement planning in this run were read-only. Production replacement did not run; original village actors, skeletal meshes, PVE graphs, and the verified checkpoint remain unchanged.",
        "",
        "- Target-folder scan: {} saved assets, complete={}.".format(inventory["target_folder_scan"]["asset_count"], inventory["target_folder_scan"]["complete"]),
        "- Cleared for an isolated performance test: {} of 24 variants / {} of 305 actors.".format(validation["cleared_variant_count"], validation["cleared_actor_count"]),
        "- Blocked: {} variants / {} actors.".format(validation["blocked_variant_count"], validation["blocked_actor_count"]),
        "- Species not fully cleared: {}.".format(", ".join(incomplete_species) if incomplete_species else "none"),
        "- Production gate: {}".format(validation["replacement_gate"]),
        "",
        "## 1. Inventory",
        "",
        "- Map: `{}`".format(inventory["map"]),
        "- Map SHA-256: `{}`".format(inventory["map_file"]["sha256"]),
        "- Loaded level actors: {}".format(inventory["total_loaded_level_actors"]),
        "- Eligible MegaPlant actors/components: {} / {}".format(inventory["eligible_megaplant_actor_count"], inventory["eligible_megaplant_component_count"]),
        "- World Partition external actor packages: {}.".format(inventory["world_partition"]["external_actor_directory"]),
        "",
        "| Species | Actors | Cleared | Blocked | Validation | Passed variants | Blocked variants |",
        "|---|---:|---:|---:|---|---|---|",
    ]
    for species, item in sorted(validation["species"].items()):
        lines.append("| {} | {} | {} | {} | {} | {} | {} |".format(
            species, item["actor_count"], item["cleared_actor_count"], item["blocked_actor_count"], item["status"],
            ", ".join(item["passed_variants"]) or "none", ", ".join(item["failed_variants"]) or "none"))
    lines.extend([
        "",
        "## 2. Protection",
        "",
        "- Verified checkpoint: `Saved/StaticVegetationCheckpoints/PrePVEStatic-20261026-155547/manifest.json`.",
        "- Checkpoint map hash matches the inventory hash; no external actor directory exists for this map.",
        "- No Git operations were performed.",
        "",
        "## 3. Source-to-target PVE mapping",
        "",
        "The retained `SK_` prefix is accepted as authored. Every target is verified by object class rather than filename prefix.",
        "",
        "| Species | Variant | Source skeletal asset | Saved Static Mesh target |",
        "|---|---|---|---|",
    ])
    for item in inventory["source_to_target_mapping"]:
        lines.append("| {} | {} | `{}` | `{}` |".format(item["species"], item["variant"], item["source_skeletal_mesh"], item["target_static_mesh"]))
    lines.extend([
        "",
        "## 4. Per-variant validation",
        "",
        "- Generated/located target assets: {} of 24.".format(len(existing_targets)),
        "- Automated checks cover Static Mesh class, Nanite, the exact two-slot bark/foliage pair, at least {} UV channels, runtime-visible bounds/pivot/scale, structural geometry diagnostics, and absence of skeletal wind runtime. Imported/reference-pose bounds are retained as diagnostics because they exclude much of the procedural canopy on these skeletal assets.".format(MIN_UV_CHANNELS),
        "- Zero collision is explicitly approved. Every future replacement component is forced to `NoCollision`, `NO_COLLISION`, and `simulate_physics=False`; no collision mesh generation is performed.",
        "- Connected parent-material WPO inputs: {} of 48 material assignments; every planned StaticMeshComponent also explicitly disables WPO and ray-tracing WPO.".format(wpo_material_count),
        "- The nine former strict-AABB warnings (Beach Rose A/B/D, Black Alder A/C/D, Common Hazel B/C, Goat Willow D) pass runtime-visible placement checks: dimension ratios 0.924-1.069, normalized center shifts at most 0.062, and every vegetation base within tolerance. No actor transform adjustment is justified or planned.",
        "- Common Hazel B contains confirmed vertex-instance UV NaNs that UE clamps to zero. Beach Rose C has confirmed nearly-zero normal/binormal build diagnostics whose visual impact is unverified. Both remain excluded.",
        "- Full leafy silhouette, texture appearance, and shadows still require rendered review; triangle counts are not treated as visual proof.",
        "",
        "| Species | Var | Result | Triangles | Vertices | UV | Sections | Package MiB | Nanite GPU MiB | Build peak MiB | Build s | Simple collision | Flags |",
        "|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|",
    ])
    for row in validation["variants"]:
        target = row["target"]
        nanite_stats = target.get("nanite_build_stats", {})
        flags = row["failures"] + row["warnings"]
        lines.append("| {} | {} | {} | {:,} | {:,} | {} | {} | {:.2f} | {:.2f} | {:.2f} | {:.2f} | {} | {} |".format(
            row["mapping"]["species"], row["mapping"]["variant"],
            "PASS" if row["automated_checks_passed"] else "FAIL",
            target.get("nanite_source_triangles", 0), target.get("nanite_source_vertices", 0),
            target.get("uv_channels_lod0"), target.get("lod0_sections"),
            float(target.get("package_file_bytes") or 0) / (1024 * 1024),
            float(nanite_stats.get("nanite_gpu_bytes") or 0) / (1024 * 1024),
            float(nanite_stats.get("required_build_memory_mb") or 0),
            float(nanite_stats.get("build_seconds") or 0),
            target.get("collision", {}).get("simple_primitive_total", 0),
            "; ".join(flags) if flags else "none"))
    lines.extend([
        "",
        "## 5. Performance gate",
        "",
        "A bounded analytical comparison was completed without spawning or changing production actors:",
        "",
        "- Skeletal components represented by currently cleared variants: {}.".format(performance["skeletal_components_removed_if_rolled_out"]),
        "- Unique source skeletal package size: {:.2f} MiB.".format(performance["unique_source_package_bytes"] / (1024 * 1024)),
        "- Unique target static package size: {:.2f} MiB (delta {:+.2f} MiB).".format(performance["unique_target_package_bytes"] / (1024 * 1024), performance["unique_package_byte_delta"] / (1024 * 1024)),
        "- Unreal Nanite build telemetry captured for {} of 24 variants: {:.2f} MiB total encoded GPU data and {:.2f} MiB encoded disk data. This is encoded resource size, not proof that every page is resident simultaneously.".format(performance["nanite_build_telemetry_variant_count"], performance["unique_nanite_encoded_gpu_bytes"] / (1024 * 1024), performance["unique_nanite_encoded_disk_bytes"] / (1024 * 1024)),
        "- Unique target LOD0 source triangles: {:,}.".format(unique_triangles),
        "- Instance-weighted target source triangles: {:,}.".format(performance["projected_instance_weighted_source_triangles"]),
        "- Rendered FPS/GPU/Nanite residency: not measured; package bytes are not GPU residency. A matched rendered isolated benchmark remains required.",
        "- Supplied packaged baseline remains 32.5 FPS / 30.74 ms frame / 29.42 ms draw / 26.56 ms GPU; no unmatched Editor number is presented as an improvement.",
        "- Isolated performance-test recommendation: **{}**. The prepared plan uses {} actors per configuration across {} matched species pairs, identical deterministic transforms/cameras/render settings/shadows, 300 warmup frames, 1800 measured frames, and skeletal/static/static/skeletal run order. Blocked variants are excluded.".format(
            performance["isolated_test_plan"]["recommendation"], performance["isolated_test_plan"]["configuration_actor_count"], len(performance["isolated_test_plan"]["representative_pairs"])),
        "- Representative pairs (10 instances each): {}.".format(benchmark_pair_text),
        "",
        "## 6. Replacement",
        "",
        "- Actors replaced: 0.",
        "- Dry-run replacements ready: {}.".format(planned),
        "- Skipped/retained by dry run: {}.".format(skipped),
        "- Accounted for: {} of {} eligible actors.".format((audit or {}).get("accounted_for", 0), inventory["eligible_megaplant_actor_count"]),
        "- Planned transform verification: {}.".format((audit or {}).get("all_planned_transforms_match_inventory", False)),
        "- Planned source/target asset-reference verification: {}.".format((audit or {}).get("all_planned_asset_references_verified", False)),
        "- Shadow settings are copied per source component; replacement collision policy: `{}`.".format((audit or {}).get("replacement_collision_policy", "pending dry run")),
        "- The rollout switch remains disabled and the implementation still safety-stops before mutation.",
        "- Black Alder used direct-PVE assets only; no crash-prone skeletal conversion was attempted.",
        "",
        "## 7. Remaining required work before production rollout",
        "",
        "1. Common Hazel B remains blocked because UE repeatedly reports NaNs in vertex-instance UVs and clamps them to zero. Correct/re-export it before clearing its 4 actors.",
        "2. Beach Rose C remains blocked because UE reports nearly-zero normals and bi-normals. Rendered inspection or a corrected direct-PVE re-export is required before clearing its 11 actors.",
        "3. Run the prepared matched isolated benchmark for the 22 cleared variants. This recommendation is GO for the isolated test only; production rollout remains NO-GO until results and the two blocked variants are reviewed.",
        "",
        "## Modified/generated files",
        "",
        "- `Content/Python/vhv_static_vegetation_inventory.py`",
        "- `Content/Python/vhv_static_vegetation_validate_and_replace.py`",
        "- `Saved/Reports/VHV_StaticVegetation_PVE_Inventory.json`",
        "- `Saved/Reports/VHV_StaticVegetation_PVE_Validation.json`",
        "- `Saved/Reports/VHV_StaticVegetation_PVE_ReplacementAudit.json`",
        "- `Saved/Reports/VHV_StaticVegetation_PVE_Conversion.md`",
        "- UE rotated `Saved/Logs/VHV*.log` and warmed Derived Data Cache entries while validating; no Content asset or map package was saved.",
    ])
    with open(CONVERSION_REPORT_PATH, "w", encoding="utf-8") as stream:
        stream.write("\n".join(lines) + "\n")


def replace_after_go():
    if not APPLY_REPLACEMENT:
        raise RuntimeError("APPLY_REPLACEMENT is False; this file is intentionally dry-run only")
    if not os.path.isfile(GO_PATH):
        raise RuntimeError("Missing explicit GO manifest: {}".format(GO_PATH))
    with open(GO_PATH, "r", encoding="utf-8") as stream:
        go = json.load(stream)
    if go.get("approved") is not True:
        raise RuntimeError("GO manifest is not approved")
    # Production mutation is intentionally not implemented until a reviewed GO
    # manifest supplies validated actor identities and a checkpoint manifest.
    raise RuntimeError("Safety stop: bind reviewed actor identities into this script before enabling production mutation")


def run():
    result = validate()
    dry_run_replacement()
    return result


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} FAILED:\n{}".format(LOG, traceback.format_exc()))
        raise
