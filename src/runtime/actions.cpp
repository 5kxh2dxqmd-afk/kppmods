// Execute declared actions as their configured account, track exit and enforce
// deadlines.
#include "broker.hpp"
#include "common/models.hpp"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <signal.h>

namespace kppmods {
namespace runtime {
Json Broker::launchAction(const Json &request) {
    const auto input = boost::json::value_to<ActionRequest>(request);
    const auto &id = input.package;
    const auto &name = input.action;
    const auto &p = package(id);
    if (!p.actions.count(name)) {
        throw std::runtime_error("Unknown declared action");
    }
    const auto &action = p.actions.at(name);
    auto args = action.argv;
    if (args.empty()) {
        throw std::runtime_error("Action has no argv");
    }
    if (args[0].empty() || args[0].find('\0') != std::string::npos) {
        throw std::runtime_error("Invalid executable");
    }
    std::string root = p.root;
    if (args[0][0] != '/') {
        char resolved[PATH_MAX];
        if (!realpath((root + "/" + args[0]).c_str(), resolved) ||
            std::string(resolved).compare(0, root.size() + 1, root + "/")) {
            throw std::runtime_error("Action executable escapes its package");
        }
        args[0] = resolved;
    }
    std::string payload = boost::json::serialize(input.args.value_or(Json()));
    for (auto &arg : args) {
        if (arg == "{args}") {
            arg = payload;
        }
    }
    CommandRequest command;
    command.package = id;
    command.argv = args;
    command.user = action.user;
    command.cwd = root;
    command.timeout = action.timeout;
    return launchCommand(command, name, payload);
}
} // namespace runtime
} // namespace kppmods
