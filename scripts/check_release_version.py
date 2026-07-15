#!/usr/bin/env python3

import argparse
import json
import re
import sys
from pathlib import Path

SEMVER_TAG = re.compile(
    r"^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$"
)


class VersionError(RuntimeError):
    pass


def version_from_tag(tag: str) -> str:
    if not SEMVER_TAG.fullmatch(tag):
        raise VersionError(f"invalid release tag: {tag!r}; expected vX.Y.Z or a semver prerelease")
    return tag[1:]


def read_library_json(path: Path) -> str:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise VersionError(f"missing version file: {path}") from error
    except json.JSONDecodeError as error:
        raise VersionError(f"invalid JSON in {path}: {error}") from error

    version = data.get("version")
    if not isinstance(version, str) or not version:
        raise VersionError(f"missing string version in {path}")
    return version


def read_library_properties(path: Path) -> str:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except FileNotFoundError as error:
        raise VersionError(f"missing version file: {path}") from error

    for line in lines:
        key, separator, value = line.partition("=")
        if separator and key.strip() == "version":
            version = value.strip()
            if version:
                return version
            break
    raise VersionError(f"missing version in {path}")


def validate_release_version(root: Path, tag: str) -> str:
    expected = version_from_tag(tag)
    versions = {
        "library.json": read_library_json(root / "library.json"),
        "library.properties": read_library_properties(root / "library.properties"),
    }

    mismatches = [
        f"{name}={version}"
        for name, version in versions.items()
        if version != expected
    ]
    if mismatches:
        joined = ", ".join(mismatches)
        raise VersionError(f"release tag {tag} requires version {expected}; found {joined}")

    return expected


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("tag", help="Git tag, for example v0.1.0")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args()

    try:
        version = validate_release_version(args.root, args.tag)
    except VersionError as error:
        print(f"release version validation failed: {error}", file=sys.stderr)
        return 1

    print(f"release version validation passed: {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
