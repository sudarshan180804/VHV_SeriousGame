"""Create a recoverable, manifest-verified checkpoint before any rollout.

This script copies the production map package and any applicable external actor
packages to Saved/StaticVegetationCheckpoints. It does not change Content.
"""

import datetime
import hashlib
import json
import os
import shutil
import traceback

import unreal


PROJECT = unreal.SystemLibrary.get_project_directory()
MAP_RELATIVE = os.path.join("Content", "VHV_Stuff", "Maps", "Lvl_Village_Main.umap")
EXTERNAL_RELATIVE = os.path.join("Content", "__ExternalActors__", "VHV_Stuff", "Lvl_Village_Main")
ROOT = os.path.join(PROJECT, "Saved", "StaticVegetationCheckpoints")
LOG = "[VHV Static Vegetation Checkpoint]"


def _sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _copy_with_record(source, destination):
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    shutil.copy2(source, destination)
    source_hash = _sha256(source)
    destination_hash = _sha256(destination)
    if source_hash != destination_hash:
        raise RuntimeError("Checkpoint hash mismatch: {}".format(source))
    return {"source": source, "checkpoint": destination, "bytes": os.path.getsize(source), "sha256": source_hash}


def run():
    map_source = os.path.join(PROJECT, MAP_RELATIVE)
    if not os.path.isfile(map_source):
        raise RuntimeError("Production map package is missing: {}".format(map_source))
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    checkpoint = os.path.join(ROOT, "PrePVEStatic-{}".format(stamp))
    if os.path.exists(checkpoint):
        raise RuntimeError("Refusing to overwrite checkpoint: {}".format(checkpoint))
    records = [_copy_with_record(map_source, os.path.join(checkpoint, MAP_RELATIVE))]
    external_source = os.path.join(PROJECT, EXTERNAL_RELATIVE)
    if os.path.isdir(external_source):
        for root, _, files in os.walk(external_source):
            for filename in sorted(files):
                source = os.path.join(root, filename)
                relative = os.path.relpath(source, PROJECT)
                records.append(_copy_with_record(source, os.path.join(checkpoint, relative)))
    manifest = {
        "created_utc": datetime.datetime.utcnow().isoformat() + "Z",
        "checkpoint": checkpoint,
        "map_relative": MAP_RELATIVE,
        "records": records,
        "verified": True,
        "note": "Content source packages were copied; original Git state and working files were not changed.",
    }
    with open(os.path.join(checkpoint, "manifest.json"), "w", encoding="utf-8") as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
    unreal.log("{} verified checkpoint: {} files at {}".format(LOG, len(records), checkpoint))
    return manifest


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} FAILED:\n{}".format(LOG, traceback.format_exc()))
        raise
