// Registration answers are local to KPP; no account store or service is changed.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <kppmods/kppmods.h>
#include <link.h>
#include <openlipc.h>
#include <sys/mman.h>
#include <syslog.h>
#include <unistd.h>

namespace {
bool inKpp() {
    static const bool result = [] {
        char executable[1024];
        const auto n = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
        if (n < 0) {
            return false;
        }
        executable[n] = 0;
        const auto *name = std::strrchr(executable, '/');
        return name && (!std::strcmp(name + 1, "KPPMainAppV2") ||
                        !std::strcmp(name + 1, "KPPMainApp"));
    }();
    return result;
}

// Identify Ignite's boolean registration accessor by its null-provider diagnostic
// and the Thumb forwarding stub. Never rely on a firmware address.
struct RegistrationGate {
    unsigned char *address = nullptr;
    unsigned matches = 0;
    bool hasProvider = false, hasMethod = false;
};
int findGate(dl_phdr_info *image, size_t, void *opaque) {
    if (image->dlpi_name && *image->dlpi_name) {
        return 0;
    }
    auto &gate = *static_cast<RegistrationGate *>(opaque);
    constexpr char diagnostic[] = "IsRegistered";
    constexpr char provider[] = "CustomerInfoProviderNative";
    for (unsigned i = 0; i < image->dlpi_phnum; ++i) {
        const auto &p = image->dlpi_phdr[i];
        if (p.p_type != PT_LOAD || !(p.p_flags & PF_R)) {
            continue;
        }
        const auto *begin =
            reinterpret_cast<const char *>(image->dlpi_addr + p.p_vaddr);
        const auto *end = begin + p.p_memsz;
        gate.hasProvider |=
            std::search(begin, end, provider, provider + sizeof(provider)) != end;
        gate.hasMethod |=
            std::search(begin, end, diagnostic, diagnostic + sizeof(diagnostic)) != end;
    }
    const auto readable = [&](uintptr_t address, size_t length) {
        for (unsigned i = 0; i < image->dlpi_phnum; ++i) {
            const auto &p = image->dlpi_phdr[i];
            const auto begin = image->dlpi_addr + p.p_vaddr;
            if (p.p_type == PT_LOAD && (p.p_flags & PF_R) && address >= begin &&
                length <= p.p_memsz && address - begin <= p.p_memsz - length) {
                return true;
            }
        }
        return false;
    };
    for (unsigned i = 0; i < image->dlpi_phnum; ++i) {
        const auto &p = image->dlpi_phdr[i];
        if (p.p_type != PT_LOAD || (p.p_flags & (PF_R | PF_X)) != (PF_R | PF_X)) {
            continue;
        }
        auto *begin = reinterpret_cast<unsigned char *>(image->dlpi_addr + p.p_vaddr);
        for (size_t offset = 0; offset + 52 <= p.p_memsz; offset += 2) {
            auto *code = begin + offset;
            const auto half = [&](size_t at) {
                uint16_t value;
                std::memcpy(&value, code + at, 2);
                return value;
            };
            if (half(0) != 0xb510 || half(2) != 0x6844 ||
                (half(4) & 0xfd07) != 0xb904) {
                continue; // cbnz r4
            }
            const size_t forward =
                8 + ((half(4) >> 3) & 31) * 2 + ((half(4) >> 9) & 1) * 64;
            if (forward > 40 || half(forward) != 0x6823 ||
                half(forward + 2) != 0x4620 || half(forward + 4) != 0x691b ||
                half(forward + 6) != 0x4798 || half(forward + 8) != 0xe7f9) {
                continue;
            }
            // Decode literal-load/add-PC pairs rather than fixing the logger's
            // argument registers or instruction positions across firmware builds.
            bool methodFound = false, providerFound = false;
            for (size_t at = 6; at < forward; at += 2) {
                const auto add = half(at);
                if ((add & 0xfff8) != 0x4478) {
                    continue;
                }
                const auto reg = add & 7;
                for (size_t load = 6; load < at; load += 2) {
                    const auto instruction = half(load);
                    if ((instruction & 0xf800) != 0x4800 ||
                        ((instruction >> 8) & 7) != reg) {
                        continue;
                    }
                    const auto literal =
                        ((reinterpret_cast<uintptr_t>(code + load) + 4) &
                         ~uintptr_t(3)) +
                        (instruction & 255) * 4;
                    if (!readable(literal, 4)) {
                        continue;
                    }
                    uint32_t relative;
                    std::memcpy(&relative, reinterpret_cast<void *>(literal), 4);
                    const auto text =
                        reinterpret_cast<uintptr_t>(code + at + 4) + relative;
                    methodFound |= readable(text, sizeof(diagnostic)) &&
                                   !std::memcmp(reinterpret_cast<void *>(text),
                                                diagnostic, sizeof(diagnostic));
                    providerFound |= readable(text, sizeof(provider)) &&
                                     !std::memcmp(reinterpret_cast<void *>(text),
                                                  provider, sizeof(provider));
                }
            }
            if (!methodFound || !providerFound) {
                continue;
            }
            gate.address = code;
            ++gate.matches;
        }
    }
    return 1;
}
int patchGate(const kppmod_api_v2 *api, const kppmod_context_v2 *context) {
    RegistrationGate gate;
    dl_iterate_phdr(findGate, &gate);
    if (!gate.hasProvider || !gate.hasMethod) {
        api->log(context, LOG_INFO,
                 "Ignite registration accessor is absent on this firmware");
        return 0;
    }
    if (gate.matches != 1) {
        api->log(context, LOG_ERR,
                 "Native registration accessor missing or ambiguous; no patch applied");
        return 95;
    }
    if (api->claim_hook(context, "registration.ignite", gate.address)) {
        return 16;
    }
    const auto pageSize = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
    const auto page = reinterpret_cast<uintptr_t>(gate.address) & ~(pageSize - 1);
    const auto end = (reinterpret_cast<uintptr_t>(gate.address) + 4 + pageSize - 1) &
                     ~(pageSize - 1);
    if (mprotect(reinterpret_cast<void *>(page), end - page,
                 PROT_READ | PROT_WRITE | PROT_EXEC)) {
        return 13;
    }
    constexpr uint16_t registered[] = {0x2001, 0x4770}; // movs r0, #1; bx lr
    std::memcpy(gate.address, registered, sizeof(registered));
    __builtin___clear_cache(reinterpret_cast<char *>(gate.address),
                            reinterpret_cast<char *>(gate.address + 4));
    if (mprotect(reinterpret_cast<void *>(page), end - page, PROT_READ | PROT_EXEC)) {
        return 13;
    }
    return 0;
}

} // namespace

extern "C" __attribute__((visibility("default"))) bool libreg_isRegistered() {
    if (inKpp()) {
        return true;
    }
    const auto original =
        reinterpret_cast<bool (*)()>(dlsym(RTLD_NEXT, "libreg_isRegistered"));
    return original && original();
}

extern "C" __attribute__((visibility("default"))) LIPCcode LipcGetIntProperty(
    LIPC *connection, const char *source, const char *property, int *value) {
    if (inKpp() && source && property && value &&
        !std::strcmp(source, "com.lab126.amazonRegistrationService") &&
        !std::strcmp(property, "isRegistered")) {
        *value = 1;
        return LIPC_OK;
    }
    const auto original = reinterpret_cast<decltype(&LipcGetIntProperty)>(
        dlsym(RTLD_NEXT, "LipcGetIntProperty"));
    return original ? original(connection, source, property, value)
                    : LIPC_ERROR_INTERNAL;
}

extern "C" __attribute__((visibility("default"))) int
kppmod_init_v2(const kppmod_api_v2 *api, const kppmod_context_v2 *context) {
    if (!inKpp()) {
        return 22;
    }
    if (api->claim_hook(context, "registration.lipc",
                        reinterpret_cast<void *>(&LipcGetIntProperty)) ||
        api->claim_hook(context, "registration.libreg",
                        reinterpret_cast<void *>(&libreg_isRegistered))) {
        return 16;
    }
    if (const auto error = patchGate(api, context)) {
        return error;
    }
    api->log(context, LOG_INFO, "KPP native registration checks overridden");
    return 0;
}
