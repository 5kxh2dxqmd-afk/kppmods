#pragma once
#include <string>
#include <vector>
namespace kppmods {
int runCommand(const std::vector<std::string> &args);
std::string captureCommand(const std::vector<std::string> &args);
} // namespace kppmods
