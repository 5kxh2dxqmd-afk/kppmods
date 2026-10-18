// Read only the Hermes tables and instructions needed for Metro discovery.
#include "discovery.hpp"
#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace km::hermes {
namespace {
struct Field {
    unsigned offset, shift, bits;
};
enum class Meaning : unsigned char { Other, Function, String };
enum class Action : unsigned char {
    None,
    CreateClosure,
    LoadConstant,
    LoadZero,
    Move,
    Call
};
struct Operand {
    unsigned char width;
    Meaning meaning;
};
struct Opcode {
    unsigned char size, count;
    Action action;
    unsigned char put;
    Operand operands[8];
};
struct Format {
    unsigned version, headerSize, functionSize, overflowShift;
    Field small[4], large[2];
    const Opcode *ops;
    size_t opCount;
};
struct Signature {
    const char *name;
    const unsigned *bits;
    size_t size;
    bool required;
};
#include "tables.inc"
constexpr size_t needleCount = sizeof(needles) / sizeof(*needles);
using Bits = std::bitset<needleCount>;
struct Function {
    size_t offset, size;
    Bits strings;
    std::vector<unsigned> children;
    bool views = false;
};
struct Value {
    unsigned type = 0, value = 0;
};
class Bundle {
    std::string bytes;

  public:
    explicit Bundle(const std::string &path) {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) {
            throw std::runtime_error("Cannot open Hermes bundle: " + path);
        }
        const auto size = input.tellg();
        if (size < 0) {
            throw std::runtime_error("Cannot size Hermes bundle: " + path);
        }
        bytes.resize(static_cast<size_t>(size));
        input.seekg(0);
        if (!input.read(bytes.data(), bytes.size())) {
            throw std::runtime_error("Cannot read Hermes bundle: " + path);
        }
    }
    size_t size() const {
        return bytes.size();
    }
    void bounds(size_t offset, size_t length) const {
        if (offset > bytes.size() || length > bytes.size() - offset) {
            throw std::runtime_error("Truncated Hermes bundle");
        }
    }
    uint64_t integer(size_t offset, unsigned length) const {
        bounds(offset, length);
        uint64_t result = 0;
        for (unsigned i = 0; i < length; ++i) {
            result |= uint64_t(static_cast<unsigned char>(bytes[offset + i]))
                      << (i * 8);
        }
        return result;
    }
    unsigned field(size_t base, Field f) const {
        return (integer(base + f.offset, (f.shift + f.bits + 7) / 8) >> f.shift) &
               ((uint64_t(1) << f.bits) - 1);
    }
    bool matches(size_t offset, size_t length, bool utf16,
                 std::string_view text) const {
        bounds(offset, length * (utf16 ? 2 : 1));
        if (length != text.size()) {
            return false;
        }
        for (size_t i = 0; i < length; ++i) {
            if (bytes[offset + i * (utf16 ? 2 : 1)] != text[i] ||
                (utf16 && bytes[offset + i * 2 + 1])) {
                return false;
            }
        }
        return true;
    }
};
size_t align4(size_t n) {
    return (n + 3) & ~size_t(3);
}
} // namespace
boost::json::object discover(const std::string &path) {
    Bundle b(path);
    if (b.integer(0, 8) != UINT64_C(0x1f1903c103bc1fc6)) {
        throw std::runtime_error("Not Hermes bytecode");
    }
    const auto version = b.integer(8, 4);
    const auto found =
        std::find_if(std::begin(formats), std::end(formats),
                     [&](const Format &f) { return f.version == version; });
    if (found == std::end(formats)) {
        throw std::runtime_error("Unsupported Hermes bytecode version: " +
                                 std::to_string(version));
    }
    const auto &format = *found;
    if (b.integer(32, 4) != b.size()) {
        throw std::runtime_error("Hermes file length mismatch");
    }
    const size_t global = b.integer(36, 4), functionCount = b.integer(40, 4),
                 stringCount = b.integer(52, 4);
    const size_t overflowCount = b.integer(56, 4), storageSize = b.integer(60, 4);
    size_t cursor = format.headerSize;
    const auto table = [&](size_t count, size_t width) {
        b.bounds(cursor, 0);
        if (count > (b.size() - cursor) / width) {
            throw std::runtime_error("Truncated Hermes table");
        }
        const auto start = cursor;
        cursor += count * width;
        return start;
    };
    table(functionCount, format.functionSize);
    cursor = align4(cursor);
    table(b.integer(44, 4), 4); // String kinds: no need to expand their runs.
    table(b.integer(48, 4), 4); // Identifier hashes.
    const size_t stringTable = table(stringCount, 4);
    const size_t overflowTable = table(overflowCount, 8);
    const size_t storage = cursor;
    b.bounds(storage, storageSize);
    std::vector<int> stringBits(stringCount, -1);
    int defineBit = -1, viewsBit = -1;
    for (size_t n = 0; n < needleCount; ++n) {
        if (std::string_view(needles[n]) == "__d") {
            defineBit = n;
        }
        if (std::string_view(needles[n]) == "KPPViews") {
            viewsBit = n;
        }
    }
    for (size_t i = 0; i < stringCount; ++i) {
        const auto word = b.integer(stringTable + i * 4, 4);
        size_t offset = (word >> 1) & 0x7fffff, length = word >> 24;
        if (length == 255) {
            if (offset >= overflowCount) {
                throw std::runtime_error("Invalid Hermes string overflow index");
            }
            length = b.integer(overflowTable + offset * 8 + 4, 4);
            offset = b.integer(overflowTable + offset * 8, 4);
        }
        const size_t width = (word & 1) ? 2 : 1;
        if (offset > storageSize || length > (storageSize - offset) / width) {
            throw std::runtime_error("Invalid Hermes string range");
        }
        for (size_t n = 0; n < needleCount; ++n) {
            if (b.matches(storage + offset, length, word & 1, needles[n])) {
                stringBits[i] = n;
                break;
            }
        }
    }
    std::vector<Function> functions(functionCount);
    std::map<unsigned, unsigned> factories;
    std::map<unsigned, Value> registers;
    bool registering = false;
    if (global >= functionCount) {
        throw std::runtime_error("Invalid Hermes global function");
    }
    for (size_t i = 0; i < functionCount; ++i) {
        auto &f = functions[i];
        const size_t small = format.headerSize + i * format.functionSize;
        size_t header = small;
        const Field *fields = format.small;
        if (b.field(small, format.small[2])) {
            header =
                (uint64_t(b.field(small, format.small[3])) << format.overflowShift) |
                b.field(small, format.small[0]);
            fields = format.large;
        }
        f.offset = b.field(header, fields[0]);
        f.size = b.field(header, fields[1]);
        b.bounds(f.offset, f.size);
        for (size_t pc = f.offset; pc < f.offset + f.size;) {
            const auto opcode = b.integer(pc, 1);
            if (opcode >= format.opCount) {
                throw std::runtime_error("Invalid Hermes opcode");
            }
            const auto &op = format.ops[opcode];
            if (!op.size || op.size > f.offset + f.size - pc) {
                throw std::runtime_error("Truncated Hermes instruction");
            }
            std::array<unsigned, 8> values{};
            bool defines = false;
            size_t operandOffset = pc + 1;
            for (unsigned a = 0; a < op.count; ++a) {
                const auto &operand = op.operands[a];
                values[a] = b.integer(operandOffset, operand.width);
                operandOffset += operand.width;
                if (operand.meaning == Meaning::String) {
                    if (values[a] >= stringBits.size()) {
                        throw std::runtime_error("Invalid Hermes string operand");
                    }
                    const auto bit = stringBits[values[a]];
                    if (bit >= 0) {
                        f.strings.set(bit);
                        defines |= bit == defineBit;
                        f.views |= op.put && bit == viewsBit;
                    }
                } else if (operand.meaning == Meaning::Function) {
                    if (values[a] >= functionCount) {
                        throw std::runtime_error("Invalid Hermes closure operand");
                    }
                    f.children.push_back(values[a]);
                }
            }
            if (i == global) {
                if (defines) {
                    registers.clear();
                    registering = true;
                }
                if (registering) {
                    switch (op.action) {
                    case Action::None:
                        break;
                    case Action::CreateClosure:
                        registers[values[0]] = {1, values[2]};
                        break;
                    case Action::LoadConstant:
                        registers[values[0]] = {2, values[1]};
                        break;
                    case Action::LoadZero:
                        registers[values[0]] = {2, 0};
                        break;
                    case Action::Move:
                        registers[values[0]] = registers[values[1]];
                        break;
                    case Action::Call: {
                        auto factory = registers[values[3]],
                             module = registers[values[4]];
                        if (factory.type == 1 && module.type == 2) {
                            factories[module.value] = factory.value;
                        }
                        registering = false;
                        break;
                    }
                    }
                }
            }
            pc += op.size;
        }
    }
    if (factories.empty()) {
        throw std::runtime_error("No Metro module registrations found");
    }
    std::vector<bool> isFactory(functionCount, false);
    for (auto [module, factory] : factories) {
        (void)module;
        isFactory[factory] = true;
    }
    std::map<unsigned, Bits> moduleStrings;
    for (auto [module, factory] : factories) {
        Bits bits;
        std::vector<bool> seen(functionCount, false);
        std::vector<unsigned> pending{factory};
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            if (seen[current]) {
                continue;
            }
            seen[current] = true;
            bits |= functions[current].strings;
            for (auto child : functions[current].children) {
                if (!isFactory[child]) {
                    pending.push_back(child);
                }
            }
        }
        moduleStrings[module] = bits;
    }
    boost::json::object result;
    for (const auto &signature : signatures) {
        Bits required;
        for (size_t i = 0; i < signature.size; ++i) {
            required.set(signature.bits[i]);
        }
        unsigned count = 0, match = 0;
        for (auto [module, bits] : moduleStrings) {
            if ((bits & required) != required) {
                continue;
            }
            if (std::string_view(signature.name) == "views" &&
                !functions[factories.at(module)].views) {
                continue;
            }
            ++count;
            match = module;
        }
        if (count != 1 && signature.required) {
            throw std::runtime_error(std::string(signature.name) +
                                     ": expected one Hermes module match, found " +
                                     std::to_string(count));
        }
        if (count == 1) {
            result[signature.name] = match;
        }
    }
    return result;
}
} // namespace km::hermes
