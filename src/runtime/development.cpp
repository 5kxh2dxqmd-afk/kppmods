#include "broker.hpp"
#include <unistd.h>

namespace kppmods::runtime {
std::string Broker::developmentDirectory() const {
    if (!config.development.is_object()) {
        throw std::runtime_error("No development session is active");
    }
    return runRoot + "/development";
}

Json Broker::saveDevelopmentResult(const Json &body) {
    saveJsonAtomically(developmentDirectory() + "/result.json", body);
    event(config.ui_runtime, "development:result", body);
    return body;
}

Json Broker::readDevelopmentUpdate(const Json &body) {
    const auto directory = developmentDirectory();
    const std::string file = directory + "/update.json";
    if (access(file.c_str(), F_OK)) {
        return boost::json::object{};
    }
    const std::string bytes = readFile(file);
    if (bytes.size() > 64 * 1024) {
        throw std::runtime_error("Development metadata exceeds 64 KiB");
    }
    Json update = km::parseJson(bytes);
    const std::string id = km::get<std::string>(update, "package");
    if (!config.settings.contains(id) ||
        !km::isLocalIdentifier(km::get<std::string>(update, "revision"))) {
        throw std::runtime_error("Invalid development package or revision");
    }
    if (km::get<std::string>(body, "after") ==
        km::get<std::string>(update, "revision")) {
        return boost::json::object{};
    }
    return update;
}

} // namespace kppmods::runtime
