#!/usr/bin/env python3
"""Inventory KPP registration and feature-gate candidates without modifying bytecode.

Accepts Hermes bytecode or hbc-disassembler text. References are syntactic evidence,
not a call graph or proof that a gate is active, writable, or safe to override.
"""

import argparse
import ast
import json
import re
import shutil
import subprocess
import tempfile
from collections import defaultdict
from pathlib import Path

from disassembly import parse_bundle

PATTERNS = {
    "registration": re.compile(
        r"(?:[Ii]s(?:Device)?Registered|[Rr]egistration|[Uu]nregistered|"
        r"[Dd]eviceNotRegistered|[Cc]ontentNotSynced|hasActiveProfileToken)"
    ),
    "capability": re.compile(r"(?:[Cc]apability|^supports[A-Z]|^isB$)"),
    "experiment": re.compile(r"(?:[Ww]eblab|[Tt]reatment|^[A-Z][A-Z0-9_]+_\d{4,}$)"),
    "feature": re.compile(
        r"(?:^[Ii]s[A-Z].*(?:Enabled|Supported|Available|Locked)$|"
        r"^enable[A-Z].*Feature$|[Ff]eatureFlag|[Ff]eatureConfig)"
    ),
}
EXPERIMENT = re.compile(r"^[A-Z][A-Z0-9_]+_\d{4,}$")


def strings_in(body):
    """Include constant arrays/objects as well as string operands."""
    found = set()
    for line in body.splitlines():
        match = re.search(r"# (String|Array|Object): (.*)", line)
        if not match:
            continue
        # Decode individual quoted strings: buffer comments can also contain
        # JavaScript null/true values, which are not Python literals.
        for literal in re.findall(r"'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"", match[2]):
            try:
                found.add(ast.literal_eval(literal))
            except (ValueError, SyntaxError):
                continue
    return found


def inspect(disassembly, symbols=(), include_bodies=False):
    functions, factories = parse_bundle(disassembly)
    roots = set(factories.values())
    children = {
        fid: set(map(int, re.findall(r"function_id: (\d+)", body)))
        for fid, body in functions.items()
    }
    owners = defaultdict(set)
    for module_id, root in factories.items():
        pending, seen = [root], set()
        while pending:
            fid = pending.pop()
            if fid in seen:
                continue
            seen.add(fid)
            if fid not in functions:
                raise ValueError(f"Missing referenced function {fid}")
            owners[fid].add(module_id)
            pending.extend(child for child in children[fid] if child not in roots)

    records, experiments = [], defaultdict(list)
    for fid, body in functions.items():
        header = re.match(r'"(.*?)" of (\d+) bytes', body)
        if not header:
            raise ValueError(f"Unrecognized function header {fid}")
        name = header[1]
        strings = strings_in(body)
        candidates = strings | {name}
        categories = {
            category: sorted(value for value in candidates if pattern.search(value))
            for category, pattern in PATTERNS.items()
        }
        categories = {key: value for key, value in categories.items() if value}
        for key in sorted(strings):
            if EXPERIMENT.fullmatch(key):
                experiments[key].append(fid)
        if symbols:
            if not set(symbols) & candidates:
                continue
        elif not categories:
            continue
        record = {
            "function_id": fid,
            "name": name,
            "module_ids": sorted(owners[fid]),
            "categories": categories,
            "strings": sorted(strings),
            "children": sorted(children[fid]),
            "evidence": [
                line
                for line in body.splitlines()
                if line.startswith("==>")
                and (
                    any(symbol in line for symbol in symbols)
                    if symbols
                    else any(
                        pattern.search(line.split("#", 1)[-1])
                        for pattern in PATTERNS.values()
                    )
                )
            ],
        }
        if include_bodies:
            record["disassembly"] = f"=> [Function #{fid} {body}"
        records.append(record)
    return {
        "scope": "Static candidates; property references are not resolved calls. "
        "Module ownership follows lexical closures. No runtime availability "
        "or override safety is implied. IDs apply only to this input bundle.",
        "function_count": len(functions),
        "module_count": len(factories),
        "candidate_count": len(records),
        "experiments": dict(sorted(experiments.items())),
        "functions": records,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument(
        "--firmware", required=True, help="Version from the source device"
    )
    parser.add_argument("--device", required=True, help="Source device/model label")
    parser.add_argument("--disassembler", default=shutil.which("hbc-disassembler"))
    parser.add_argument(
        "--symbol", action="append", default=[], help="Exact name/string; repeatable"
    )
    parser.add_argument("--include-bodies", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r"\d+(?:\.\d+)+", args.firmware):
        parser.error("--firmware must be a dotted numeric release")
    try:
        with tempfile.TemporaryDirectory(prefix="kppmods-gates-") as directory:
            source = args.bundle
            with source.open("rb") as stream:
                is_text = stream.read(3) == b"=> "
            if not is_text:
                if not args.disassembler:
                    parser.error("Bytecode input requires --disassembler")
                source = Path(directory) / "bundle.hasm"
                subprocess.run(
                    [args.disassembler, str(args.bundle), str(source)],
                    check=True,
                    stdout=subprocess.DEVNULL,
                )
            result = inspect(source.read_text(), args.symbol, args.include_bodies)
        result.update(
            source=str(args.bundle.resolve()),
            firmware=args.firmware,
            device=args.device,
            symbols=args.symbol,
        )
        output = json.dumps(result, indent=2) + "\n"
        if args.output:
            args.output.write_text(output)
        else:
            print(output, end="")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
