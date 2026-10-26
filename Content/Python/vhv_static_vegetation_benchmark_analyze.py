"""Analyze the four guarded VHV vegetation CSV captures.

The CSV profiler appends a short thread-flush tail.  Each capture requested
exactly 2,100 frames, so this analyzer takes the first 2,100 valid frame rows,
discards the first 300, and measures exactly the following 1,800 frames.
"""

import csv
import json
import math
import os
from statistics import fmean


PROJECT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CSV_DIR = os.path.join(PROJECT, "Saved", "Profiling", "CSV")
REPORT_DIR = os.path.join(PROJECT, "Saved", "Reports")
OUTPUT = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Benchmark_Results.json")
PRODUCTION_OUTPUT = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Production_Benchmark_Results.json")
PACKAGED_OUTPUT = os.path.join(REPORT_DIR, "VHV_StaticVegetation_Packaged_Final_Benchmark_Results.json")
PACKAGED_CSV_DIR = os.path.join(
    PROJECT, "Saved", "Packaged", "VHV_StaticVegetation_Final", "Windows", "VHV", "Saved", "Profiling", "CSV")
CAPTURE_FRAMES = 2100
WARMUP_FRAMES = 300
MEASURED_FRAMES = 1800

RUNS = {
    "A1": ("VHV_Veg_A1", "skeletal"),
    "B1": ("VHV_Veg_B1", "static"),
    "B2": ("VHV_Veg_B2", "static"),
    "A2": ("VHV_Veg_A2", "skeletal"),
}

STATS = {
    "frame_ms": "FrameTime",
    "game_thread_ms": "GameThreadTime",
    "render_thread_ms": "RenderThreadTime",
    "rhi_thread_ms": "RHIThreadTime",
    "gpu_ms": "GPUTime",
    "nanite_visibility_ms": "GPU/NaniteVisBuffer",
    "nanite_base_pass_ms": "GPU/NaniteBasePass",
    "nanite_streaming_ms": "GPU/NaniteStreaming",
    "shadow_depths_ms": "GPU/ShadowDepths",
    "gpu_local_used_mb": "GPUMem/LocalUsedMB",
    "gpu_local_budget_mb": "GPUMem/LocalBudgetMB",
    "nanite_root_allocation_mb": "NaniteStreaming/RootAllocationMB",
    "nanite_root_data_mb": "NaniteStreaming/RootDataSizeMB",
    "nanite_visible_streaming_mb": "NaniteStreamingDetail/VisibleStreamingDataSizeMB",
    "nanite_streaming_pool_mb": "NaniteStreamingDetail/StreamingPoolSizeMB",
    "skeletal_component_ticks": "Ticks/SkeletalMeshComponent",
    "skeletal_actor_count": "ActorCount/SkeletalMeshActor",
    "animation_game_thread_ms": "Exclusive/GameThread/Animation",
    "draw_calls": "RHI/DrawCalls",
    "primitives_drawn": "RHI/PrimitivesDrawn",
}

# These render-thread counters are emitted at a lower cadence than every game
# frame; an all-zero row means "not sampled this frame", not zero usage/work.
ZERO_IS_MISSING = {
    "gpu_local_used_mb", "gpu_local_budget_mb", "draw_calls", "primitives_drawn"
}


def percentile(values, fraction):
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def summarize(values):
    return {
        "average": fmean(values),
        "p95": percentile(values, 0.95),
        "minimum": min(values),
        "maximum": max(values),
    }


def summarize_metric(key, values):
    if key in ZERO_IS_MISSING:
        values = [value for value in values if value > 0.0]
    return summarize(values)


def read_run(run_id, filename, configuration, validate_skeletal_count=True):
    path = filename if os.path.isabs(filename) else os.path.join(CSV_DIR, filename)
    with open(path, "r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.reader(stream))
    header = rows[0]
    optional_zero_columns = {"skeletal_actor_count"} if configuration == "static" else set()
    missing = [column for key, column in STATS.items()
               if column not in header and key not in optional_zero_columns]
    if missing:
        raise RuntimeError("{} missing required CSV columns: {}".format(run_id, missing))
    indices = {key: (header.index(column) if column in header else None)
               for key, column in STATS.items()}
    valid = []
    for row in rows[1:]:
        if len(row) <= indices["frame_ms"]:
            continue
        try:
            frame_ms = float(row[indices["frame_ms"]])
        except (ValueError, IndexError):
            continue
        if frame_ms <= 0:
            continue
        valid.append(row)
        if len(valid) == CAPTURE_FRAMES:
            break
    if len(valid) != CAPTURE_FRAMES:
        raise RuntimeError("{} has {} valid frames; expected {}".format(
            run_id, len(valid), CAPTURE_FRAMES))
    window = valid[WARMUP_FRAMES:WARMUP_FRAMES + MEASURED_FRAMES]
    values = {
        key: [float(row[index]) if index is not None and index < len(row) and row[index] else 0.0
              for row in window]
        for key, index in indices.items()
    }
    if validate_skeletal_count:
        expected_skeletal = 60.0 if configuration == "skeletal" else 0.0
        actor_counts = values["skeletal_actor_count"]
        if min(actor_counts) != expected_skeletal or max(actor_counts) != expected_skeletal:
            raise RuntimeError("{} skeletal actor count changed within measurement: {}..{}".format(
                run_id, min(actor_counts), max(actor_counts)))
    metrics = {key: summarize_metric(key, series) for key, series in values.items()}
    metrics["average_fps"] = 1000.0 / metrics["frame_ms"]["average"]
    return {
        "run": run_id,
        "configuration": configuration,
        "source": path,
        "capture_frames": CAPTURE_FRAMES,
        "discarded_warmup_frames": WARMUP_FRAMES,
        "measured_frames": len(window),
        "metrics": metrics,
        "_values": values,
    }


def aggregate(runs, configuration):
    selected = [run for run in runs.values() if run["configuration"] == configuration]
    combined = {
        key: [value for run in selected for value in run["_values"][key]]
        for key in STATS
    }
    metrics = {key: summarize_metric(key, series) for key, series in combined.items()}
    metrics["average_fps"] = 1000.0 / metrics["frame_ms"]["average"]
    return {
        "configuration": configuration,
        "runs": [run["run"] for run in selected],
        "measured_frames": sum(run["measured_frames"] for run in selected),
        "metrics": metrics,
    }


def pct_change(before, after):
    return (after - before) / before * 100.0


def main():
    runs = {run_id: read_run(run_id, *definition) for run_id, definition in RUNS.items()}
    skeletal = aggregate(runs, "skeletal")
    static = aggregate(runs, "static")
    a = skeletal["metrics"]
    b = static["metrics"]
    comparison = {
        "fps_percent": pct_change(a["average_fps"], b["average_fps"]),
        "frame_time_percent": pct_change(a["frame_ms"]["average"], b["frame_ms"]["average"]),
        "frame_time_p95_percent": pct_change(a["frame_ms"]["p95"], b["frame_ms"]["p95"]),
        "game_thread_percent": pct_change(a["game_thread_ms"]["average"], b["game_thread_ms"]["average"]),
        "render_thread_percent": pct_change(a["render_thread_ms"]["average"], b["render_thread_ms"]["average"]),
        "rhi_thread_percent": pct_change(a["rhi_thread_ms"]["average"], b["rhi_thread_ms"]["average"]),
        "gpu_percent": pct_change(a["gpu_ms"]["average"], b["gpu_ms"]["average"]),
        "nanite_visibility_percent": pct_change(
            a["nanite_visibility_ms"]["average"], b["nanite_visibility_ms"]["average"]),
        "shadow_depths_percent": pct_change(
            a["shadow_depths_ms"]["average"], b["shadow_depths_ms"]["average"]),
        "gpu_local_used_mb_delta": (
            b["gpu_local_used_mb"]["average"] - a["gpu_local_used_mb"]["average"]),
        "nanite_root_data_mb_delta": (
            b["nanite_root_data_mb"]["average"] - a["nanite_root_data_mb"]["average"]),
        "skeletal_actor_count_delta": (
            b["skeletal_actor_count"]["average"] - a["skeletal_actor_count"]["average"]),
        "skeletal_component_tick_delta": (
            b["skeletal_component_ticks"]["average"] - a["skeletal_component_ticks"]["average"]),
    }
    serializable_runs = {}
    for run_id, run in runs.items():
        serializable_runs[run_id] = {key: value for key, value in run.items() if key != "_values"}
    result = {
        "schema": 1,
        "method": {
            "order": ["A1", "B1", "B2", "A2"],
            "capture_frames_per_run": CAPTURE_FRAMES,
            "warmup_frames_discarded": WARMUP_FRAMES,
            "measurement_frames_per_run": MEASURED_FRAMES,
            "aggregate_frames_per_configuration": 3600,
            "fps_formula": "1000 / arithmetic mean FrameTime",
            "p95_method": "linear interpolation",
        },
        "runs": serializable_runs,
        "aggregate": {"skeletal": skeletal, "static": static},
        "comparison_static_vs_skeletal": comparison,
    }
    with open(OUTPUT, "w", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2, sort_keys=True)
    production_definitions = {
        "entrance": "VHV_Production_Post_Entrance",
        "market": "VHV_Production_Post_Market",
    }
    if all(os.path.isfile(os.path.join(CSV_DIR, name)) for name in production_definitions.values()):
        production_runs = {
            run_id: read_run(run_id, filename, "production_static", validate_skeletal_count=False)
            for run_id, filename in production_definitions.items()
        }
        serializable_production = {
            run_id: {key: value for key, value in run.items() if key != "_values"}
            for run_id, run in production_runs.items()
        }
        production_result = {
            "schema": 1,
            "environment": "UnrealEditor 5.7.4 -game, D3D12 SM6, 1920x1080, Epic, 100%, uncapped",
            "method": {
                "capture_frames_per_location": CAPTURE_FRAMES,
                "warmup_frames_discarded": WARMUP_FRAMES,
                "measurement_frames_per_location": MEASURED_FRAMES,
                "fps_formula": "1000 / arithmetic mean FrameTime",
                "p95_method": "linear interpolation",
            },
            "runs": serializable_production,
            "packaged_baseline_note": "Supplied entrance baseline is approximately 32.5 FPS, but it is a packaged capture and is not treated as a matched numeric comparison to Editor -game.",
        }
        with open(PRODUCTION_OUTPUT, "w", encoding="utf-8") as stream:
            json.dump(production_result, stream, indent=2, sort_keys=True)
    packaged_definitions = {
        "entrance": os.path.join(PACKAGED_CSV_DIR, "VHV_Packaged_Final_Entrance"),
        "market": os.path.join(PACKAGED_CSV_DIR, "VHV_Packaged_Final_Market"),
    }
    if all(os.path.isfile(path) for path in packaged_definitions.values()):
        packaged_runs = {
            run_id: read_run(run_id, path, "packaged_static", validate_skeletal_count=False)
            for run_id, path in packaged_definitions.items()
        }
        serializable_packaged = {
            run_id: {key: value for key, value in run.items() if key != "_values"}
            for run_id, run in packaged_runs.items()
        }
        baseline = {"fps": 32.5, "frame_ms": 30.74, "draw_ms": 29.42, "gpu_ms": 26.56,
                    "source": "user-supplied approximate packaged village entrance baseline"}
        entrance = packaged_runs["entrance"]["metrics"]
        packaged_result = {
            "schema": 1,
            "environment": "Windows Development packaged build, UE 5.7.4, D3D12 SM6, 1920x1080, Epic, 100%, uncapped",
            "method": {
                "capture_frames_per_location": CAPTURE_FRAMES,
                "warmup_frames_discarded": WARMUP_FRAMES,
                "measurement_frames_per_location": MEASURED_FRAMES,
                "fps_formula": "1000 / arithmetic mean FrameTime",
                "p95_method": "linear interpolation",
            },
            "runs": serializable_packaged,
            "supplied_packaged_entrance_baseline": baseline,
            "entrance_vs_supplied_baseline": {
                "fps_percent": pct_change(baseline["fps"], entrance["average_fps"]),
                "frame_time_percent": pct_change(baseline["frame_ms"], entrance["frame_ms"]["average"]),
                "render_vs_draw_percent": pct_change(baseline["draw_ms"], entrance["render_thread_ms"]["average"]),
                "gpu_percent": pct_change(baseline["gpu_ms"], entrance["gpu_ms"]["average"]),
                "caveat": "Baseline values are approximate and supplied externally; resolution/settings are treated as matched per task context but no raw baseline CSV was available for P95/RHI/VRAM comparison."
            },
        }
        with open(PACKAGED_OUTPUT, "w", encoding="utf-8") as stream:
            json.dump(packaged_result, stream, indent=2, sort_keys=True)
    print(json.dumps({"aggregate": result["aggregate"], "comparison": comparison}, indent=2))


if __name__ == "__main__":
    main()
