"""Stage a versioned release, verify uploaded bytes, then make it public.

Requires GitHub CLI and GH_TOKEN on a trusted, fully validated CI run. Never
replace an existing public release or move its tag to a different commit.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def gh(*args: str) -> str:
    return subprocess.run(["gh", *args], check=True, text=True,
                          capture_output=True).stdout


def inspect_package(path: Path, platform: str, commit: str, version: str) -> None:
    with zipfile.ZipFile(path) as archive:
        if archive.testzip() is not None:
            raise RuntimeError(f"Corrupt ZIP: {path.name}")
        module = archive.read("GeigerGenerator.clap")
        magic = b"MZ" if platform == "Windows" else b"\x7fELF"
        if not module.startswith(magic) or len(module) < 100_000:
            raise RuntimeError(f"Missing or incorrect {platform} binary")
        build = archive.read("BUILD.txt").decode("utf-8")
        for expected in (f"Commit: {commit}", f"Platform: {platform}", f"Version: {version}"):
            if expected not in build.splitlines():
                raise RuntimeError(f"Wrong build provenance: {path.name}: {expected}")
        if len(archive.read("licences/AGPL-3.0.txt")) < 30_000:
            raise RuntimeError("Full licence text missing")
        archive.read("docs/RELEASE_NOTES_1.0.md")


def verify_download(directory: Path, names: set[str], commit: str, version: str) -> None:
    files = {p.name for p in directory.iterdir() if p.is_file()}
    if files != names:
        raise RuntimeError(f"Release asset mismatch: {files ^ names}")
    checked = set()
    for line in (directory / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines():
        digest, name = line.split("  ", 1)
        if name not in names or name in checked or name == "SHA256SUMS.txt":
            raise RuntimeError("Unexpected or repeated checksum entry")
        actual = hashlib.sha256((directory / name).read_bytes()).hexdigest()
        if actual != digest:
            raise RuntimeError(f"Release checksum mismatch: {name}")
        checked.add(name)
    if checked != names - {"SHA256SUMS.txt"}:
        raise RuntimeError("Incomplete release checksum manifest")
    for platform in ("Windows", "Linux"):
        inspect_package(directory / f"GeigerGenerator-{platform}-CLAP.zip", platform, commit, version)
    with zipfile.ZipFile(directory / "GeigerGenerator-Audio-Demos.zip") as archive:
        if archive.testzip() is not None or len([n for n in archive.namelist() if n.endswith(".wav")]) != 20:
            raise RuntimeError("Audio example archive is incomplete")
        archive.read("PRESETS.txt")


def main() -> None:
    commit = os.environ["GITHUB_SHA"]
    repo = os.environ["GITHUB_REPOSITORY"]
    match = re.search(r"project\(GeigerGenerator VERSION (\d+\.\d+\.\d+)",
                      (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"))
    if match is None or re.fullmatch(r"[a-f0-9]{40}", commit) is None:
        raise RuntimeError("Invalid version or commit")
    version = match.group(1)
    tag = "v" + version
    stage = ROOT / "release"
    stage.mkdir(exist_ok=True)
    if any(stage.iterdir()):
        raise RuntimeError("Release staging directory must be empty")
    artifacts = ROOT / "artifacts"
    for platform in ("Windows", "Linux"):
        source = artifacts / f"GeigerGenerator-{platform}" / f"GeigerGenerator-{platform}-CLAP.zip"
        inspect_package(source, platform, commit, version)
        shutil.copy2(source, stage / source.name)
    source = artifacts / "GeigerGenerator-Linux" / "GeigerGenerator-Audio-Demos.zip"
    shutil.copy2(source, stage / source.name)
    for page, title in enumerate(("Field", "Circuit", "Speaker", "Behavior"), 1):
        source = artifacts / "GeigerGenerator-UI-Windows" / f"geiger-1120-page-{page}.png"
        if not source.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"):
            raise RuntimeError("Invalid editor screenshot")
        shutil.copy2(source, stage / f"GeigerGenerator-{title}.png")
    manifest = "".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n"
                       for p in sorted(stage.iterdir()))
    (stage / "SHA256SUMS.txt").write_text(manifest, encoding="utf-8")
    names = {p.name for p in stage.iterdir()}
    verify_download(stage, names, commit, version)
    notes = ROOT / "release-notes.md"
    notes.write_text((ROOT / "docs/RELEASE_NOTES_1.0.md").read_text(encoding="utf-8") +
                     f"\nSource commit: `{commit}`. Both platform build/validation jobs passed.\n"
                     f"\nValidation run: https://github.com/{repo}/actions/runs/{os.environ['GITHUB_RUN_ID']}\n",
                     encoding="utf-8")

    query = subprocess.run(["gh", "api", f"repos/{repo}/releases/tags/{tag}"],
                           text=True, capture_output=True)
    existing = None
    if query.returncode == 0:
        existing = json.loads(query.stdout)
        if existing["target_commitish"] != commit:
            raise RuntimeError(f"{tag} already targets a different commit. Bump the version; do not overwrite it.")
    elif "HTTP 404" not in query.stderr:
        raise RuntimeError(query.stderr)

    public = existing is not None and not existing["draft"]
    if existing is None:
        gh("release", "create", tag, *[str(p) for p in sorted(stage.iterdir())],
           "--draft", "--target", commit, "--title", f"Geiger Generator {version}",
           "--notes-file", str(notes))
    elif not public:
        gh("release", "upload", tag, *[str(p) for p in sorted(stage.iterdir())], "--clobber")

    with tempfile.TemporaryDirectory(prefix="geiger-release-check-") as folder:
        downloaded = Path(folder)
        gh("release", "download", tag, "--dir", str(downloaded))
        verify_download(downloaded, names, commit, version)
        if not public and (downloaded / "SHA256SUMS.txt").read_text(encoding="utf-8") != manifest:
            raise RuntimeError("Uploaded release does not match the validated build artifacts")
    if not public:
        gh("release", "edit", tag, "--draft=false", "--prerelease=false", "--latest")
    result = json.loads(gh("api", f"repos/{repo}/releases/tags/{tag}"))
    if result["draft"] or result["prerelease"] or result["target_commitish"] != commit:
        raise RuntimeError("Release publication was not confirmed")
    if {a["name"] for a in result["assets"] if a["state"] == "uploaded"} != names:
        raise RuntimeError("Published assets are incomplete")
    print(f"Published and verified {tag}: {result['html_url']}")


if __name__ == "__main__":
    main()
