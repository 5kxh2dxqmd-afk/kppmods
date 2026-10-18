#!/usr/bin/env python3
"""Package a CMake installation tree using the pinned eink-sdk packager."""

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONFIG = json.loads((ROOT / "release.json").read_text())


def run(*args, **kwargs):
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True, **kwargs)


def output(*args):
    return run(*args, stdout=subprocess.PIPE, text=True).stdout.strip()


def package_release(args):
    target = CONFIG["targets"][args.target]
    prefix = CONFIG["toolchains"][target["architecture"]]["prefix"]
    toolchain = args.toolchain.resolve()
    work = ROOT / "build" / args.target
    source = args.source.resolve()
    version = output(args.patchelf.resolve(), "--version").removeprefix("patchelf ")
    if version != CONFIG["tools"]["patchelf"]:
        raise ValueError(
            f"Expected patchelf {CONFIG['tools']['patchelf']}, found {version}"
        )
    sdk = args.sdk.resolve()
    packager = work / "sdk/support/scripts/build-opkg"
    packager.parent.mkdir(parents=True, exist_ok=True)
    # Read the committed packager, independent of the SDK checkout's working files.
    packager.write_text(
        output(
            "git",
            "-c",
            f"safe.directory={sdk.resolve()}",
            "-C",
            sdk,
            "show",
            CONFIG["sdk"]["revision"] + ":support/scripts/build-opkg",
        )
        + "\n"
    )
    packager.chmod(0o755)
    for patch in CONFIG["sdk"]["patches"]:
        run(
            "patch",
            "--batch",
            "--fuzz=0",
            "-p1",
            "-d",
            work / "sdk",
            "-i",
            ROOT / patch,
        )
    env = dict(
        os.environ,
        OPKG_ARCH=target["architecture"],
        OPKG_PATCHELF=str(args.patchelf.resolve()),
        OPKG_READELF=str(toolchain / "bin" / (prefix + "-readelf")),
        OPKG_STRIP=str(toolchain / "bin" / (prefix + "-strip")),
    )
    env.update(target["elf_policy"])
    packages = work / "ipks"
    if packages.exists():
        shutil.rmtree(packages)
    version = CONFIG["version"]
    sources = [("core", source)]
    for name in CONFIG["extensions"]:
        manifest = json.loads(
            (ROOT / "extensions" / name / "manifest.json").read_text()
        )
        path = source / "usr/lib/kppmods/extensions" / manifest["id"]
        installed = json.loads((path / "manifest.json").read_text())
        if (installed["id"], installed["version"]) != (
            manifest["id"],
            manifest["version"],
        ):
            raise ValueError(f"Reinstall selected extension {name} before packaging")
        sources.append(("extension", path))
    for kind, source in sources:
        run(
            sys.executable,
            ROOT / "tools/packaging/ipk.py",
            kind,
            source,
            packages,
            "--sdk",
            work / "sdk",
            "--version",
            version,
            env=env,
        )
    destination = ROOT / "dist" / version / args.target
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        shutil.rmtree(destination)
    packages.rename(destination)
    print(f"Release packages: {destination}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="CMake installation prefix")
    parser.add_argument("--target", choices=CONFIG["targets"], default="kindlehf")
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--patchelf", type=Path, required=True)
    args = parser.parse_args()
    try:
        package_release(args)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Packaging failed: {error}\n")


if __name__ == "__main__":
    main()
