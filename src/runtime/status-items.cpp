#include "broker.hpp"
#include "common/lipc.hpp"
#include <climits>
#include <cstdlib>

namespace kppmods::runtime {
void Broker::resetStatusItems() {
    if (statusReset) {
        return;
    }
    sendStatusUpdates({"clear_dynamic"});
    statusItems.clear();
    statusReset = true;
}

void Broker::retryStatusReset() {
    if (statusReset || time(nullptr) < nextStatusReset ||
        (nextStatusReset && !config.settings.contains("org.kppmods.statusbar"))) {
        return;
    }
    nextStatusReset = time(nullptr) + 1;
    try {
        resetStatusItems();
    } catch (const std::exception &) {
    }
}

Json Broker::updateStatusItems(const Json &body) {
    const std::string owner = km::get<std::string>(body, "package");
    if (!config.settings.contains(owner) || !km::field(body, "items").is_array() ||
        km::arrayItems(body, "items").size() > 8) {
        throw std::runtime_error("Invalid status items");
    }
    std::vector<std::string> updates;
    std::set<std::string> ids;
    for (const auto &item : km::arrayItems(body, "items")) {
        std::string id = km::get<std::string>(item, "id"),
                    icon = km::get<std::string>(item, "icon"),
                    uri = km::get<std::string>(item, "uri");
        if (!km::isLocalIdentifier(id) || !ids.insert(id).second ||
            uri.compare(0, 6, "app://") ||
            uri.find_first_of("\t\r\n") != std::string::npos) {
            throw std::runtime_error("Invalid status item");
        }
        if (icon.compare(0, 7, "file://") == 0) {
            icon.erase(0, 7);
        }
        const std::string packageRoot = package(owner).root;
        if (icon.empty() || icon.find_first_of("\t\r\n") != std::string::npos ||
            icon.find('\0') != std::string::npos) {
            throw std::runtime_error("Invalid status icon path");
        }
        if (icon[0] != '/') {
            icon = packageRoot + "/" + icon;
        }
        char resolved[PATH_MAX];
        const std::string developmentRoot =
            config.root + "/development/packages/" + owner + "/";
        const bool resolvedIcon = realpath(icon.c_str(), resolved) != nullptr;
        const bool packageAsset =
            resolvedIcon && std::string(resolved).compare(0, packageRoot.size() + 1,
                                                          packageRoot + "/") == 0;
        const bool developmentAsset =
            resolvedIcon && config.development.is_object() &&
            std::string(resolved).compare(0, developmentRoot.size(), developmentRoot) ==
                0;
        if (!packageAsset && !developmentAsset) {
            throw std::runtime_error("Status icon must belong to its package");
        }
        updates.push_back("put\t" + owner + ":" + id + "\t" + resolved + "\t" + uri);
    }
    for (const auto &id : statusItems[owner]) {
        if (!ids.count(id)) {
            updates.push_back("remove\t" + owner + ":" + id);
        }
    }
    resetStatusItems();
    sendStatusUpdates(updates);
    statusItems[owner] = ids;
    boost::json::object response;
    response["accepted"] = true;
    return response;
}

void sendStatusUpdates(const std::vector<std::string> &updates) {
    auto lipc = km::ipc::connect();
    for (const auto &update : updates) {
        const LIPCcode result = km::ipc::api().LipcSetStringProperty(
            lipc.get(), "org.kppmods.statusbar", "update", update.c_str());
        if (result != LIPC_OK) {
            throw std::runtime_error("Statusbar update rejected: " +
                                     std::to_string(result));
        }
    }
}

} // namespace kppmods::runtime
