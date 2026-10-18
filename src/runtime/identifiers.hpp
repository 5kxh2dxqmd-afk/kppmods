#pragma once
#include <algorithm>
#include <string_view>

namespace km {
inline bool isPackageId(std::string_view id) {
    const auto alnum = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    };
    return !id.empty() && id.size() < 128 && alnum(id.front()) &&
           std::all_of(
               id.begin(), id.end(),
               [&](char c) { return alnum(c) || c == '.' || c == '-'; });
}
inline bool isLocalIdentifier(std::string_view id) {
    return !id.empty() && id.size() < 128 && id.front() != '.' &&
           std::all_of(
               id.begin(), id.end(),
               [](char c) {
                   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
               });
}
} // namespace km
