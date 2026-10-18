#!/usr/bin/env python3
"""Resolve KPP Metro modules by semantic strings in Hermes bytecode, without firmware offsets.

Development reference using the hermes-dec submodule parser.
On-device activation uses the native scanner in kppmod.
Accepts Hermes bytecode or disassembler output for development.
Only unique matches are emitted; ambiguous or missing required surfaces abort activation.
"""

import argparse
import json
import re
import sys
from pathlib import Path

from disassembly import parse_bundle
from signatures import REQUIRED, SIGNATURES


def resolve(disassembly):
    functions, factories = parse_bundle(disassembly)
    factory_ids = set(factories.values())

    def collect_function_strings(function_id):
        seen, strings, pending = set(), set(), [function_id]
        while pending:
            current = pending.pop()
            if current in seen:
                continue
            seen.add(current)
            body = functions[current]
            strings.update(re.findall(r"# String: '([^']+)'", body))
            pending.extend(
                int(nested_id)
                for nested_id in re.findall(r"function_id: (\d+)", body)
                if int(nested_id) not in factory_ids
            )
        return strings

    strings = {
        module_id: collect_function_strings(function_id)
        for module_id, function_id in factories.items()
    }
    result, errors = {}, []
    for name, signature in SIGNATURES.items():
        matches = [
            module_id for module_id, found in strings.items() if signature <= found
        ]
        if name == "views":
            matches = [
                module_id
                for module_id in matches
                if re.search(
                    r"<PutById[^\n]+# String: 'KPPViews'",
                    functions[factories[module_id]],
                )
            ]
        if len(matches) != 1 and name in REQUIRED:
            errors.append("%s: expected one semantic match, found %s" % (name, matches))
        elif len(matches) == 1:
            result[name] = matches[0]
    if errors:
        raise ValueError("\n".join(errors))
    return result


def resolve_bytecode(bundle):
    # The upstream parser owns bytecode versions, operands and file layout.
    sys.path.insert(
        0, str(Path(__file__).resolve().parents[2] / "vendor/hermes-dec/src")
    )
    from hermes_dec.parsers.hbc_bytecode_parser import parse_hbc_bytecode
    from hermes_dec.parsers.hbc_file_parser import HBCReader
    from hermes_dec.parsers.hbc_opcodes.def_classes import OperandMeaning

    wanted = set().union(*SIGNATURES.values())
    functions, factories, exports_views = {}, {}, set()
    reader = HBCReader()
    with bundle.open("rb") as stream:
        reader.read_whole_file(stream)
        registers, registering = {}, False
        for index, header in enumerate(reader.function_headers):
            strings, children = set(), set()
            for op in parse_hbc_bytecode(header, reader):
                name = op.inst.name
                values = [
                    getattr(op, "arg%d" % (i + 1)) for i in range(len(op.inst.operands))
                ]
                op_strings = set()
                for operand, value in zip(op.inst.operands, values):
                    if operand.operand_meaning == OperandMeaning.string_id:
                        text = reader.strings[value]
                        op_strings.add(text)
                        if text in wanted:
                            strings.add(text)
                    elif operand.operand_meaning == OperandMeaning.function_id:
                        children.add(value)
                if name.startswith("PutById") and "KPPViews" in op_strings:
                    exports_views.add(index)
                if index != reader.header.globalCodeIndex:
                    continue
                if "__d" in op_strings:
                    registers, registering = {}, True
                if not registering:
                    continue
                if name in ("CreateClosure", "CreateClosureLongIndex"):
                    registers[values[0]] = ("function", values[2])
                elif name in ("LoadConstUInt8", "LoadConstInt"):
                    registers[values[0]] = ("number", values[1])
                elif name == "LoadConstZero":
                    registers[values[0]] = ("number", 0)
                elif name in ("Mov", "MovLong"):
                    registers[values[0]] = registers.get(values[1])
                elif name == "Call4":
                    factory, module = registers.get(values[3]), registers.get(values[4])
                    if (
                        factory
                        and module
                        and factory[0] == "function"
                        and module[0] == "number"
                    ):
                        factories[module[1]] = factory[1]
                    registering = False
            functions[index] = (strings, children)
    if not factories:
        raise ValueError("No Metro module registrations resolved")
    factory_ids = set(factories.values())
    module_strings = {}
    for module, factory in factories.items():
        found, seen, pending = set(), set(), [factory]
        while pending:
            current = pending.pop()
            if current in seen:
                continue
            seen.add(current)
            strings, children = functions[current]
            found.update(strings)
            pending.extend(children - factory_ids - seen)
        module_strings[module] = found
    result = {}
    for name, signature in SIGNATURES.items():
        matches = [
            module
            for module, strings in module_strings.items()
            if signature <= strings
            and (name != "views" or factories[module] in exports_views)
        ]
        if len(matches) != 1 and name in REQUIRED:
            raise ValueError(
                "%s: expected one semantic match, found %s" % (name, matches)
            )
        if len(matches) == 1:
            result[name] = matches[0]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    with args.bundle.open("rb") as stream:
        is_text = stream.read(3) == b"=> "
    modules = (
        resolve(args.bundle.read_text()) if is_text else resolve_bytecode(args.bundle)
    )
    output = {"modules": modules}
    text = json.dumps(output, sort_keys=True, indent=2) + "\n"
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
