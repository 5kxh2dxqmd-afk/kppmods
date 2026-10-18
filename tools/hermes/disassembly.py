"""Parse Hermes disassembler output into functions and Metro registrations."""

import re


def parse_bundle(disassembly):
    """Return function bodies and Metro factory IDs for read-only analysis."""
    functions = {
        int(match[1]): match[2]
        for match in re.finditer(
            r"^=> \[Function #(\d+) ([\s\S]*?)(?=^=> \[Function #|\Z)",
            disassembly,
            re.M,
        )
    }
    if 0 not in functions:
        raise ValueError("No Hermes global function found")
    factories = {}
    for block in functions[0].split("# String: '__d' (Identifier)")[1:]:
        # Module factory and numeric ID are operands of the Metro __d call.
        registers = {}
        for line in block.splitlines():
            closure = re.search(
                r"<CreateClosure(?:LongIndex)?>: <Reg8: (\d+), Reg8: \d+, function_id: (\d+)>",
                line,
            )
            number = re.search(
                r"<LoadConst(?:UInt8|Int)>: <Reg8: (\d+), (?:UInt8|Imm32): (\d+)>", line
            )
            zero = re.search(r"<LoadConstZero>: <Reg8: (\d+)>", line)
            move = re.search(
                r"<Mov(?:Long)?>: <Reg(?:8|32): (\d+), Reg(?:8|32): (\d+)>", line
            )
            if closure:
                registers[int(closure[1])] = ("function", int(closure[2]))
            if number:
                registers[int(number[1])] = ("number", int(number[2]))
            if zero:
                registers[int(zero[1])] = ("number", 0)
            if move and int(move[2]) in registers:
                registers[int(move[1])] = registers[int(move[2])]
            if "<Call4>:" in line:
                args = [int(value) for value in re.findall(r"Reg8: (\d+)", line)]
                if len(args) == 6:
                    factory_reference, module_id = (
                        registers.get(args[3]),
                        registers.get(args[4]),
                    )
                    if (
                        factory_reference
                        and module_id
                        and factory_reference[0] == "function"
                        and module_id[0] == "number"
                    ):
                        factories[module_id[1]] = factory_reference[1]
                break
    if not factories:
        raise ValueError("No Metro module registrations resolved")
    return functions, factories
