#pragma once
#include "models.hpp"
namespace kppmods {
using km::Json;
Json runtimeStatus(const Configuration &configuration, bool operationPending);
} // namespace kppmods
