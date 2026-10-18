#pragma once
#include "json.hpp"
#include <dlfcn.h>
#include <memory>
#include <openlipc.h>
#include <stdexcept>

namespace km::ipc {
inline constexpr const char *service = "org.kppmods";
inline void check(LIPCcode code) {
    if (code != LIPC_OK) {
        throw std::runtime_error("LIPC error " + std::to_string(code));
    }
}
// Use the firmware library, with declarations supplied by OpenLIPC.
struct API {
    void *library = dlopen("liblipc.so.1", RTLD_NOW | RTLD_LOCAL);
    template <class T> T symbol(const char *name) {
        auto value = library ? dlsym(library, name) : nullptr;
        if (!value) {
            throw std::runtime_error(std::string("Missing LIPC API: ") + name);
        }
        return reinterpret_cast<T>(value);
    }
#define LIPC_API(name) decltype(&::name) name = symbol<decltype(&::name)>(#name)
    LIPC_API(LipcOpen);
    LIPC_API(LipcOpenNoName);
    LIPC_API(LipcClose);
    LIPC_API(LipcDynamicHasharrayNew);
    LIPC_API(LipcHasharrayDestroy);
    LIPC_API(LipcHasharrayAddHash);
    LIPC_API(LipcHasharrayGetString);
    LIPC_API(LipcHasharrayPutString);
    LIPC_API(LipcAccessHasharrayProperty);
    LIPC_API(LipcRegisterDynamicHasharrayProperty);
    LIPC_API(LipcSetStringProperty);
    LIPC_API(LipcSubscribeExt);
    LIPC_API(LipcCreateAndSendEvent);
#undef LIPC_API
};
inline API &api() {
    static API instance;
    return instance;
}
using Connection = std::unique_ptr<LIPC, decltype(&LipcClose)>;
inline Connection connect(const char *name = nullptr) {
    auto &a = api();
    Connection result(name ? a.LipcOpen(name) : a.LipcOpenNoName(), a.LipcClose);
    if (!result) {
        throw std::runtime_error("Cannot open LIPC connection");
    }
    return result;
}
using Hasharray = std::unique_ptr<LIPCha, decltype(&LipcHasharrayDestroy)>;
inline std::string get(LIPCha *array) {
    char *text = nullptr;
    check(api().LipcHasharrayGetString(array, 0, "json", &text));
    return text ? text : "";
}
inline Hasharray make(LIPC *connection, const Json &value) {
    const auto bytes = boost::json::serialize(value);
    auto &a = api();
    Hasharray array(a.LipcDynamicHasharrayNew(connection, bytes.size() + 1),
                    a.LipcHasharrayDestroy);
    if (!array) {
        throw std::runtime_error("Cannot allocate LIPC message");
    }
    size_t index;
    check(a.LipcHasharrayAddHash(array.get(), &index));
    check(a.LipcHasharrayPutString(array.get(), index, "json", bytes.c_str()));
    return array;
}
inline Json request(const std::string &operation, const Json &body) {
    auto connection = connect();
    auto &a = api();
    auto input = make(connection.get(),
                      boost::json::object{{"operation", operation}, {"body", body}});
    LIPCha *output = nullptr;
    const auto code = a.LipcAccessHasharrayProperty(connection.get(), service,
                                                    "request", input.get(), &output);
    Hasharray reply(output, a.LipcHasharrayDestroy);
    check(code);
    if (!reply) {
        throw std::runtime_error("Missing LIPC reply");
    }
    return parseJson(get(reply.get()));
}
} // namespace km::ipc
