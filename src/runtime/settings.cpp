#include "broker.hpp"
#include <cmath>

namespace kppmods::runtime {
namespace {
void validateSetting(const Json &schema, const Json &value) {
    std::string type = km::get<std::string>(schema, "type");
    if ((type == "boolean" && !value.is_bool()) ||
        (type == "string" && !value.is_string()) ||
        ((type == "number" || type == "integer") && !value.is_number())) {
        throw std::runtime_error("Setting type mismatch");
    }
    if (type == "integer" &&
        std::floor(value.to_number<double>()) != value.to_number<double>()) {
        throw std::runtime_error("Setting requires an integer");
    }
    if (value.is_number() &&
        ((km::contains(schema, "min") &&
          value.to_number<double>() < schema.at("min").to_number<double>()) ||
         (km::contains(schema, "max") &&
          value.to_number<double>() > schema.at("max").to_number<double>()))) {
        throw std::runtime_error("Setting out of range");
    }
}
} // namespace

Json Broker::updateSettings(const Json &body) {
    if (managing()) {
        throw std::runtime_error("Runtime activation is in progress");
    }
    std::string id = km::get<std::string>(body, "package");
    if (!config.settings.contains(id) || !km::field(body, "values").is_object()) {
        throw std::runtime_error("Invalid settings request");
    }
    auto candidate = config.settings.at(id).as_object();
    for (const auto &entry : body.at("values").as_object()) {
        if (!km::isLocalIdentifier(std::string(entry.key()))) {
            throw std::runtime_error("Invalid setting name");
        }
        const std::string name(entry.key());
        validateSetting(km::field(package(id).settingsSchema, name), entry.value());
        candidate[name] = entry.value();
    }
    saveJsonAtomically(data + "/" + id + ".json", candidate);
    config.settings[id] = candidate;
    event(id, "settings:changed", candidate);
    return candidate;
}

} // namespace kppmods::runtime
