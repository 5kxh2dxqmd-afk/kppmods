#pragma once
#include "common/json.hpp"
namespace kppmods {
km::Json installedApps();
void launchApp(const std::string &script);
} // namespace kppmods
