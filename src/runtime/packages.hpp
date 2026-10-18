#pragma once
#include "models.hpp"
#include "runtime-files.hpp"
namespace kppmods {
using km::Json;
std::string packagePath(const std::string &base, const std::string &relative);
std::map<std::string, Package> installedPackages();
void setPackageEnabled(const std::map<std::string, Package> &installed,
                       Disabled &disabled, const std::string &id, bool enabled);
std::vector<Package> enabledPackages(const std::map<std::string, Package> &installed,
                                     const Disabled &disabled);
} // namespace kppmods
