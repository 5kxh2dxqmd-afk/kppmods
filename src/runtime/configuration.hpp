#pragma once
#include "models.hpp"
namespace kppmods {
using km::Json;
Json mergeOptions(Json defaults, const Json &overrides);
Configuration buildConfiguration(const std::string &installationRoot,
                                 const std::vector<Package> &packages);
} // namespace kppmods
