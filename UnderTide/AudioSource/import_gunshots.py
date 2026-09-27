"""Import only the four generated gunshot WAVs into /Game/Audio.

Run from UnrealEditor-Cmd with -ExecutePythonScript=<this file>.
Existing assets are checked and never overwritten by default. Set the
UNDERTIDE_REIMPORT_GUNSHOTS=1 environment variable only after backing up
the four target assets.
"""

import os
from pathlib import Path

import unreal


SOURCE = Path(__file__).resolve().parent
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
REPLACE_EXISTING = os.environ.get("UNDERTIDE_REIMPORT_GUNSHOTS") == "1"

for weapon in ("AK", "M4", "MP5", "AA12"):
    name = f"Fire_{weapon}"
    wav = SOURCE / f"{name}.wav"
    target = f"/Game/Audio/{name}"
    if not wav.is_file():
        raise FileNotFoundError(wav)
    if ASSETS.does_asset_exist(target):
        asset = unreal.load_asset(target)
        if not isinstance(asset, unreal.SoundWave):
            raise TypeError(f"{target} exists but is not a SoundWave")
        if not REPLACE_EXISTING:
            print(f"Kept existing {target}")
            continue

    task = unreal.AssetImportTask()
    task.filename = str(wav)
    task.destination_path = "/Game/Audio"
    task.destination_name = name
    task.automated = True
    task.replace_existing = REPLACE_EXISTING
    task.save = True
    TOOLS.import_asset_tasks([task])
    asset = unreal.load_asset(target)
    if not isinstance(asset, unreal.SoundWave):
        raise RuntimeError(f"Failed to import {target}")
    print(f"Imported {target}")
