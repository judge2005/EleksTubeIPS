#!/usr/bin/env python3
"""Checks that a release is complete and consistent before anyone can flash it.

CI runs this on every build. Standard library only, so it also runs on a laptop:

    python3 tools/check_release.py                       # the sources only
    python3 tools/check_release.py --tag v2.0.0          # and the tag matches the version
    python3 tools/check_release.py --files releases      # and these files are complete
    python3 tools/check_release.py --merge built --files dist
                                                         # first combine each clock's files
"""
import argparse
import filecmp
import json
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NOTES = os.path.join(".github", "release-notes.md")
FLASH_SIZES = {"4MB": 4 * 1024 * 1024, "8MB": 8 * 1024 * 1024}

problems = []


def problem(message):
    problems.append(message)


def read(*path):
    with open(os.path.join(ROOT, *path), encoding="utf-8") as f:
        return f.read()


def project_version():
    match = re.search(r'^version\s*=\s*"([^"]+)"', read("version.py"), re.M)
    if not match:
        sys.exit("version.py doesn't set a version")
    return match.group(1)


def clocks():
    return re.findall(r"^\[env:([^\]]+)\]", read("platformio.ini"), re.M)


def manifest_names():
    return sorted(n for n in os.listdir(os.path.join(ROOT, "web")) if re.fullmatch(r"esp-web-tools-manifest-.+\.json", n))


def merge(built, out):
    """Combines one directory per clock. Clocks share some files, which must then be the same file.

    Except the filesystem images: they hold the same pages, but gzip records when each build made them.
    """
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(out)
    for clock in sorted(os.listdir(built)):
        folder = os.path.join(built, clock)
        if not os.path.isdir(folder):
            continue
        for name in sorted(os.listdir(folder)):
            source = os.path.join(folder, name)
            target = os.path.join(out, name)
            if not os.path.exists(target):
                shutil.copyfile(source, target)
            elif not name.startswith("littlefs") and not filecmp.cmp(source, target, shallow=False):
                problem(f"{name} from {clock} differs from the same file built for another clock")


def check_tag(tag, version):
    if tag != f"v{version}":
        problem(f"Tag {tag} doesn't match version.py, which says {version}")


def check_docs_name_version(version):
    """Anything that tells people which files to download has to name this release's."""
    if os.path.exists(os.path.join(ROOT, "README.md")):
        readme = read("README.md")
        mentions = re.findall(r"firmware-[a-z0-9_]+-([0-9][^\s`]*?)\.bin", readme)
        mentions += re.findall(r"littlefs(?:8MB)?-([0-9][^\s`]*?)\.bin", readme)
        mentions += re.findall(r"/releases/tag/v([0-9][^\s)]*)", readme)
        mentions += re.findall(r"^## Installing ([0-9]\S*)", readme, re.M)
        for mention in sorted(set(mentions) - {version}):
            problem(f"README.md still names version {mention}, but version.py says {version}")

    notes = read(NOTES)
    for clock in clocks():
        if f"firmware-{clock}-$version$.bin" not in notes:
            problem(f"{NOTES} doesn't list {clock}")


def check_manifests(version):
    names = manifest_names()
    for clock in clocks():
        for suffix in ("", "-firmware-only"):
            name = f"esp-web-tools-manifest-{clock}{suffix}.json"
            if name not in names:
                problem(f"{clock} has no web/{name}")

    known = set(clocks())
    for name in names:
        clock = re.sub(r"^esp-web-tools-manifest-|(-firmware-only)?\.json$", "", name)
        if clock not in known:
            problem(f"web/{name} is for {clock}, which isn't in platformio.ini")
        try:
            json.loads(read("web", name))
        except json.JSONDecodeError as e:
            problem(f"web/{name} isn't valid JSON: {e}")


def check_files(folder, version):
    """Every file a manifest flashes exists, none overlaps the next, and all of them fit."""
    for clock in clocks():
        name = f"firmware-{clock}-{version}.bin"
        if not os.path.exists(os.path.join(folder, name)):
            problem(f"{name} is missing")

    for name in manifest_names():
        try:
            manifest = json.loads(read("web", name).replace("$version$", version))
        except json.JSONDecodeError:
            continue    # Already reported

        for build in manifest["builds"]:
            parts = sorted(build["parts"], key=lambda part: part["offset"])
            flash = FLASH_SIZES["8MB" if any("8MB" in part["path"] for part in parts) else "4MB"]
            end = 0
            for part in parts:
                path = os.path.join(folder, part["path"])
                if not os.path.exists(path):
                    problem(f"{name} flashes {part['path']}, which is missing")
                    continue
                if part["offset"] < end:
                    problem(f"{name}: {part['path']} at {part['offset']:#x} overlaps the part before it")
                end = part["offset"] + os.path.getsize(path)
                if end > flash:
                    problem(f"{name}: {part['path']} runs past the end of the flash")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tag", help="the tag being released, such as v2.0.0")
    parser.add_argument("--merge", metavar="DIR", help="combine DIR's folders, one per clock, into --files first")
    parser.add_argument("--files", metavar="DIR", help="the directory of release files to check")
    args = parser.parse_args()

    if args.merge and not args.files:
        parser.error("--merge needs --files")

    version = project_version()
    if args.tag:
        check_tag(args.tag, version)
    check_docs_name_version(version)
    check_manifests(version)
    if args.merge:
        merge(args.merge, args.files)
    if args.files:
        check_files(args.files, version)

    if problems:
        for message in problems:
            # GitHub Actions shows these on the run's summary. They read fine in a terminal too.
            print(f"::error::{message}")
        sys.exit(1)

    checked = f"{len(os.listdir(args.files))} files" if args.files else "the sources"
    print(f"Release {version}: {checked} OK, for {len(clocks())} clocks")


if __name__ == "__main__":
    main()
