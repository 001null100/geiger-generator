"""Package tested binaries, documentation and dependency licence notices."""
from pathlib import Path
import hashlib
import os
import shutil
import sys
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
# Use the checked-in, verbatim licence. Packaging must work offline and must
# not depend on a third-party website accepting a hosted runner's request.
# read_text normalizes checkout line endings before validating the source text.
text = (root / "licences" / "AGPL-3.0.txt").read_text(encoding="utf-8").encode("utf-8")
expected = "0d96a4ff68ad6d4b6f1f30f713b18d5184912ba8dd389f86aa7710db079abcb0"
if hashlib.sha256(text).hexdigest() != expected:
    raise RuntimeError("Bundled AGPL licence text failed its integrity check")
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
