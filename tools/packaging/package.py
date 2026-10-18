#!/usr/bin/env python3
"""Stage an extension; package-owned packaging/stage.py supplies optional payloads."""

import argparse
import json
import runpy
import shutil
import subprocess
import tempfile
from pathlib import Path
from types import SimpleNamespace


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package")
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--source", type=Path, help="External extension source directory"
    )
    parser.add_argument("--epub-runtime", type=Path)
    parser.add_argument(
        "--architecture", choices=("kindlepw2", "kindlehf"), default="kindlehf"
    )
    parser.add_argument("--toolkit", type=Path)
    parser.add_argument("--rsvg-convert", default=shutil.which("rsvg-convert"))
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[2]
    source = (
        args.source.resolve()
        if args.source
        else repository / "extensions" / args.package
    )
    if not source.is_dir():
        parser.error("Unknown package")
    if args.output.exists():
        parser.error("Output must be a new directory")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="kppmods-stage-", dir=args.output.parent
    ) as temporary:
        output = Path(temporary) / "payload"
        shutil.copytree(
            source,
            output,
            ignore=shutil.ignore_patterns(
                "native", "node_modules", ".git", "__pycache__", "stage.py"
            ),
        )
        stage = source / "packaging/stage.py"
        if stage.exists():
            context = SimpleNamespace(
                source=source, output=output, repository=repository, options=args
            )
            try:
                runpy.run_path(str(stage))["stage"](context)
            except (ValueError, FileNotFoundError) as error:
                parser.error(str(error))
        manifest_path = output / "manifest.json"
        manifest = json.loads(manifest_path.read_text())
        if len(manifest.get("ui", [])) > 1:
            parser.error("An extension supports at most one UI entry")
        entries = []
        for entry in manifest.get("ui", []):
            compiled = str(Path(entry).with_suffix(".js"))
            subprocess.run(
                [
                    "node",
                    str(repository / "tools/build/build-extension.cjs"),
                    str(source / entry),
                    manifest["id"],
                    str(output / compiled),
                    str(source),
                ],
                check=True,
            )
            if compiled != entry:
                (output / entry).unlink()
            entries.append(compiled)
        if entries:
            manifest["ui"] = entries
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
        output.rename(args.output)
    print(args.output)


if __name__ == "__main__":
    main()
