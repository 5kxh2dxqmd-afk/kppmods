#pragma once
#include "common/models.hpp"
#include <set>

namespace kppmods {
struct Package : Manifest {
    std::string root;
    bool operator==(const Package &) const = default;
};
BOOST_DESCRIBE_STRUCT(Package, (Manifest), (root))
inline Package tag_invoke(boost::json::value_to_tag<Package>, const km::Json &json) {
    return readRecord<Package>(json);
}
using Disabled = std::set<std::string>;
struct Job {
    std::map<std::string, std::string> env;
    std::vector<std::string> preloads, packages;
    boost::json::array plugins, scripts, statusbar;
    bool operator==(const Job &) const = default;
};
BOOST_DESCRIBE_STRUCT(Job, (), (env, preloads, packages, plugins, scripts, statusbar))
inline Job tag_invoke(boost::json::value_to_tag<Job>, const km::Json &json) {
    return readRecord<Job>(json);
}
struct Configuration {
    std::string firmware, root, ui_runtime;
    std::vector<Package> packages;
    boost::json::object settings, modules;
    std::map<std::string, Job> processes;
    km::Json development;
    bool operator==(const Configuration &) const = default;
};
BOOST_DESCRIBE_STRUCT(Configuration, (),
                      (firmware, root, ui_runtime, packages, settings, modules,
                       processes, development))
inline Configuration tag_invoke(boost::json::value_to_tag<Configuration>,
                                const km::Json &json) {
    return readRecord<Configuration>(json);
}
} // namespace kppmods
