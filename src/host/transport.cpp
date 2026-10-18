#include "common/json.hpp"
#include "common/lipc.hpp"
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <string>
#include <syslog.h>
#include <thread>
#include <typeinfo>

extern "C" bool kppmods_transport_enabled();
namespace {
using km::Json;
constexpr const char *invokeSymbol = "_ZN11reactNative12nativeModule12NativeModule6invo"
                                     "keExON5utils4json13JsonContainerEx";
#if _GLIBCXX_USE_CXX11_ABI
constexpr const char *serializeSymbol =
    "_ZNK5utils4json13JsonContainer6toJsonB5cxx11Ev";
constexpr const char *constructSymbol =
    "_ZN5utils4json13JsonContainerC1ERKNSt7__cxx1112basic_stringIcSt11char_"
    "traitsIcESaIcEEE";
constexpr const char *eventSymbol =
    "_ZN11reactNative12nativeModule21NativeModuleInterface12callJSModuleERKNSt7__"
    "cxx1112basic_stringIcSt11char_traitsIcESaIcEEES9_ON5utils4json13JsonContainerE";
#else
constexpr const char *serializeSymbol = "_ZNK5utils4json13JsonContainer6toJsonEv";
constexpr const char *constructSymbol = "_ZN5utils4json13JsonContainerC1ERKSs";
constexpr const char *eventSymbol =
    "_ZN11reactNative12nativeModule21NativeModuleInterface12callJSModuleERKSsS3_"
    "ON5utils4json13JsonContainerE";
#endif
using Invoke = void (*)(void *, void *, long long, void *, long long);
using Serialize = std::string (*)(const void *);
using Construct = void (*)(void *, const std::string &);
using Empty = void (*)(void *);
using Destroy = void (*)(void *);
using Event = void (*)(void *, const std::string &, const std::string &, void *);
struct NativeBindings {
    // KPP's Fabric library exports same-named methods for different object
    // layouts. Networking belongs to this legacy provider; never select its
    // methods through global lookup order.
    void *provider = dlopen("libReactNative.so", RTLD_NOW | RTLD_NOLOAD);
    Invoke original = reinterpret_cast<Invoke>(dlsym(provider, invokeSymbol));
    Serialize serialize =
        reinterpret_cast<Serialize>(dlsym(RTLD_NEXT, serializeSymbol));
    Construct construct =
        reinterpret_cast<Construct>(dlsym(RTLD_NEXT, constructSymbol));
    Empty empty =
        reinterpret_cast<Empty>(dlsym(RTLD_NEXT, "_ZN5utils4json13JsonContainerC1Ev"));
    Destroy destroy = reinterpret_cast<Destroy>(
        dlsym(RTLD_NEXT, "_ZN5utils4json13JsonContainerD1Ev"));
    Event event = reinterpret_cast<Event>(dlsym(provider, eventSymbol));
    bool ready() const {
        return provider && original && serialize && construct && empty && destroy &&
               event;
    }
};
NativeBindings &nativeBindings() {
    static NativeBindings *bindings = new NativeBindings;
    return *bindings;
}
struct Polymorphic {
    virtual ~Polymorphic() {}
};
struct Module {
    void *native;
    bool closing = false;
    unsigned pending = 0;
    std::once_flag subscribed;
    km::ipc::Connection connection{nullptr, km::ipc::api().LipcClose};
    explicit Module(void *native) : native(native) {}
};
struct Runtime {
    std::mutex mutex;
    pthread_cond_t drained = PTHREAD_COND_INITIALIZER;
    std::map<void *, std::shared_ptr<Module>> modules;
};
Runtime &runtime() {
    static auto *state = new Runtime;
    return *state;
}
void emit(void *module, const char *name, const Json &value) {
    // Opaque stock JsonContainer storage for the ARM32 ABI. Construct and destroy
    // it through stock methods rather than depending on a guessed C++ layout.
    alignas(4) unsigned char container[12];
    auto &bindings = nativeBindings();
    Json args = boost::json::array{};
    args.as_array().push_back(Json(std::string(name)));
    args.as_array().push_back(Json(value));
    bindings.construct(container, boost::json::serialize(args));
    try {
        bindings.event(module, std::string("RCTDeviceEventEmitter"),
                       std::string("emit"), container);
    } catch (...) {
        bindings.destroy(container);
        throw;
    }
    bindings.destroy(container);
}
LIPCcode changed(LIPC *, const char *, LIPCevent *, void *data) {
    auto &module = *static_cast<Module *>(data);
    std::lock_guard<std::mutex> lock(runtime().mutex);
    if (!module.closing) {
        try {
            emit(module.native, "kppmods:changed", boost::json::object{});
        } catch (...) {
            return LIPC_ERROR_INTERNAL;
        }
    }
    return LIPC_OK;
}
void run(std::shared_ptr<Module> module, int id, std::string operation, Json body) {
    Json result;
    try {
        std::call_once(module->subscribed, [&] {
            module->connection = km::ipc::connect();
            km::ipc::check(km::ipc::api().LipcSubscribeExt(module->connection.get(),
                                                           km::ipc::service, "changed",
                                                           changed, module.get()));
        });
        if (operation == "readFile") {
            // Development source stays in a local file, outside the LIPC payload.
            const auto path = km::get<std::string>(body, "path");
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            const auto size = file.tellg();
            if (!file || size < 0 || size > 2 * 1024 * 1024) {
                throw std::runtime_error("Cannot read development source: " + path);
            }
            std::string text(static_cast<size_t>(size), '\0');
            file.seekg(0);
            if (!file.read(text.data(), text.size())) {
                throw std::runtime_error("Incomplete development source");
            }
            result = boost::json::object{{"source", text}};
        } else {
            result = km::ipc::request(operation, body);
        }
    } catch (const std::exception &error) {
        result = boost::json::object{{"error", error.what()}};
    }
    auto &state = runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!module->closing) {
        try {
            emit(module->native, "kppmods:response", boost::json::array{id, result});
        } catch (...) {
            syslog(LOG_ERR, "kppmods: cannot deliver LIPC response");
        }
    }
    --module->pending;
    pthread_cond_broadcast(&state.drained);
}
} // namespace
// The stock Networking module supplies a stable asynchronous native-call entry.
// Our four-argument marker is intercepted before stock dispatch; no HTTP request
// or network event is constructed. Other calls retain their stock behavior.
extern "C" __attribute__((visibility("default"))) void
kppmods_invoke(void *, void *, long long, void *,
               long long) asm("_ZN11reactNative12nativeModule12NativeModule6invokeExON5"
                              "utils4json13JsonContainerEx");
extern "C" void kppmods_invoke(void *result, void *module, long long method,
                               void *arguments, long long callId) {
    auto &bindings = nativeBindings();
    if (kppmods_transport_enabled() && bindings.ready() && method == 0 &&
        !strcmp(typeid(*reinterpret_cast<Polymorphic *>(module)).name(),
                "N11reactNative12nativeModule10NetworkingE")) {
        const auto args = km::parseJson(bindings.serialize(arguments));
        const auto &items = km::arrayItems(args);
        if (items.size() == 4 && items[0].is_string() &&
            items[0].as_string() == "kppmods") {
            bindings.empty(result);
            const int id = items[1].to_number<int>();
            auto &state = runtime();
            std::lock_guard<std::mutex> lock(state.mutex);
            try {
                auto &entry = state.modules[module];
                if (!entry) {
                    entry = std::make_shared<Module>(module);
                }
                if (entry->closing) {
                    return;
                }
                if (entry->pending >= 8) {
                    throw std::runtime_error("Too many LIPC requests");
                }
                ++entry->pending;
                try {
                    std::thread(run, entry, id, std::string(items[2].as_string()),
                                items[3])
                        .detach();
                } catch (...) {
                    --entry->pending;
                    throw;
                }
            } catch (const std::exception &error) {
                emit(module, "kppmods:response",
                     boost::json::array{id,
                                        boost::json::object{{"error", error.what()}}});
            }
            return;
        }
    }
    if (bindings.original) {
        bindings.original(result, module, method, arguments, callId);
    } else if (bindings.empty) {
        bindings.empty(result);
    }
}
extern "C" __attribute__((visibility("default"))) void kppmods_shutdown(void *) asm(
    "_ZN11reactNative12nativeModule21NativeModuleInterface8shutdownEv");
extern "C" void kppmods_shutdown(void *module) {
    using Shutdown = void (*)(void *);
    const bool networking =
        !strcmp(typeid(*reinterpret_cast<Polymorphic *>(module)).name(),
                "N11reactNative12nativeModule10NetworkingE");
    static auto original = reinterpret_cast<Shutdown>(dlsym(
        RTLD_NEXT, "_ZN11reactNative12nativeModule21NativeModuleInterface8shutdownEv"));
    static auto legacy = reinterpret_cast<Shutdown>(
        dlsym(nativeBindings().provider,
              "_ZN11reactNative12nativeModule21NativeModuleInterface8shutdownEv"));
    auto &state = runtime();
    std::shared_ptr<Module> entry;
    if (networking) {
        std::unique_lock<std::mutex> lock(state.mutex);
        const auto found = state.modules.find(module);
        if (found != state.modules.end()) {
            entry = found->second;
            entry->closing = true;
            while (entry->pending) {
                pthread_cond_wait(&state.drained, state.mutex.native_handle());
            }
        }
    }
    // Closing LIPC drains its callbacks; do not hold their mutex while waiting.
    if (entry) {
        entry->connection.reset();
    }
    auto shutdown = networking ? legacy : original;
    if (shutdown) {
        shutdown(module);
    }
    if (entry) {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.modules.erase(module);
    }
}
