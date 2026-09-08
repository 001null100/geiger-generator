"""Package tested binaries, documentation and dependency licence notices."""
from pathlib import Path
import os
import shutil
import sys
import urllib.request
import zipfile

root = Path(__file__).resolve().parents[1]
build = root / "build"
platform = sys.argv[1]
claps = list((build / "clap").glob("GeigerGenerator.clap"))
if len(claps) != 1:
    raise RuntimeError("Expected exactly one built CLAP module")
stage = root / "package"
shutil.rmtree(stage, ignore_errors=True)
stage.mkdir()
shutil.copy2(claps[0], stage / claps[0].name)
for name in ("README.md", "LICENSE", "THIRD_PARTY_NOTICES.md"):
    shutil.copy2(root / name, stage / name)
shutil.copytree(root / "docs", stage / "docs")
licences = stage / "licences"
licences.mkdir()
with urllib.request.urlopen("https://www.gnu.org/licenses/agpl-3.0.txt", timeout=60) as response:
    text = response.read()
if b"GNU AFFERO GENERAL PUBLIC LICENSE" not in text:
    raise RuntimeError("Unexpected AGPL licence response")
(licences / "AGPL-3.0.txt").write_bytes(text)
for dep in ("null_clap", "clap", "clap_helpers", "juce"):
    source = build / "_deps" / (dep + "-src")
    for file in source.iterdir():
        if file.is_file() and file.name.lower().startswith(("license", "licence", "copying")):
            shutil.copy2(file, licences / (dep + "-" + file.name))
    if dep == "juce":
        for module in ("juce_core", "juce_data_structures", "juce_events", "juce_graphics", "juce_gui_basics"):
            for file in (source / "modules" / module).rglob("*"):
                if file.is_file() and (file.name.lower().startswith(("license", "licence", "copying")) or file.name.lower() == "readme"):
                    rel = file.relative_to(source)
                    target = licences / "juce" / rel
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(file, target)
(stage / "BUILD.txt").write_text("Commit: " + os.getenv("GITHUB_SHA", "local") + "\nPlatform: " + platform + "\n", encoding="utf-8")
dist = root / "dist"
dist.mkdir(exist_ok=True)
with zipfile.ZipFile(dist / f"GeigerGenerator-{platform}-CLAP.zip", "w", zipfile.ZIP_DEFLATED) as archive:
    for file in stage.rglob("*"):
        if file.is_file():
            archive.write(file, file.relative_to(stage))
print("Packaged", platform)
