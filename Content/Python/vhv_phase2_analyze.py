"""Analyze matched VHV Phase 2 CSV captures and rank render/GPU costs."""

import argparse
import csv
import json
import math
import os
from statistics import fmean


CAPTURE_FRAMES = 2100
WARMUP_FRAMES = 300
MEASURED_FRAMES = 1800

CORE = {
    "frame_ms": "FrameTime",
    "game_thread_ms": "GameThreadTime",
    "render_thread_ms": "RenderThreadTime",
    "rhi_thread_ms": "RHIThreadTime",
    "gpu_ms": "GPUTime",
    "draw_calls": "RHI/DrawCalls",
    "primitives_drawn": "RHI/PrimitivesDrawn",
    "gpu_local_used_mb": "GPUMem/LocalUsedMB",
    "updated_shadow_maps": "LightCount/UpdatedShadowMaps",
    "shadow_cache_mb": "ShadowCacheUsageMB",
}


def percentile(values, fraction):
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def stats(values, sparse=False):
    if sparse:
        values = [value for value in values if value > 0.0]
    if not values:
        return {"average": 0.0, "p95": 0.0, "minimum": 0.0, "maximum": 0.0}
    return {
        "average": fmean(values),
        "p95": percentile(values, 0.95),
        "minimum": min(values),
        "maximum": max(values),
    }


def numeric(row, index):
    if index >= len(row) or not row[index]:
        return 0.0
    try:
        return float(row[index])
    except ValueError:
        return 0.0


def analyze(path):
    with open(path, "r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.reader(stream))
    header = rows[0]
    frame_index = header.index("FrameTime")
    valid = []
    for row in rows[1:]:
        if numeric(row, frame_index) <= 0.0:
            continue
        valid.append(row)
        if len(valid) == CAPTURE_FRAMES:
            break
    if len(valid) != CAPTURE_FRAMES:
        raise RuntimeError("{} has {} valid frames, expected {}".format(
            path, len(valid), CAPTURE_FRAMES))
    window = valid[WARMUP_FRAMES:WARMUP_FRAMES + MEASURED_FRAMES]

    core = {}
    for key, column in CORE.items():
        if column not in header:
            continue
        index = header.index(column)
        core[key] = stats(
            [numeric(row, index) for row in window],
            sparse=key in {"draw_calls", "primitives_drawn", "gpu_local_used_mb"},
        )
    core["average_fps"] = 1000.0 / core["frame_ms"]["average"]

    gpu = {}
    render_thread = {}
    for index, column in enumerate(header):
        if column.startswith("GPU/"):
            gpu[column] = stats([numeric(row, index) for row in window])
        elif column.startswith("Exclusive/RenderThread/"):
            render_thread[column] = stats([numeric(row, index) for row in window])
    gpu = dict(sorted(gpu.items(), key=lambda item: item[1]["average"], reverse=True))
    render_thread = dict(sorted(
        render_thread.items(), key=lambda item: item[1]["average"], reverse=True))
    return {
        "source": os.path.abspath(path),
        "capture_frames": CAPTURE_FRAMES,
        "warmup_frames": WARMUP_FRAMES,
        "measured_frames": MEASURED_FRAMES,
        "core": core,
        "gpu_ranked": gpu,
        "render_thread_ranked": render_thread,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="+")
    parser.add_argument("--output")
    args = parser.parse_args()
    result = {os.path.basename(path): analyze(path) for path in args.paths}
    # Preserve descending cost order in the ranked dictionaries.
    payload = json.dumps(result, indent=2)
    if args.output:
        os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
        with open(args.output, "w", encoding="utf-8") as stream:
            stream.write(payload + "\n")
    print(payload)


if __name__ == "__main__":
    main()
