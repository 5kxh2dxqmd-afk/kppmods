#pragma once
#include <boost/json.hpp>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace km {
using Json = boost::json::value;

// Optional fields are represented by null. Present values retain their JSON type;
// callers decide whether a field is optional or must pass schema conversion.
inline const Json &field(const Json &value, boost::json::string_view key) {
    static const Json absent;
    if (value.is_null()) {
        return absent;
    }
    const auto *found = value.as_object().if_contains(key);
    return found ? *found : absent;
}
template <class Value, class... Keys>
decltype(auto) field(Value &&value, boost::json::string_view key,
                     boost::json::string_view next, const Keys &...keys) {
    return field(field(std::forward<Value>(value), key), next, keys...);
}
inline const Json &field(const boost::json::object &value,
                         boost::json::string_view key) {
    static const Json absent;
    const auto *found = value.if_contains(key);
    return found ? *found : absent;
}
template <class T, class Value>
T get(const Value &value, boost::json::string_view key, T otherwise = {}) {
    const auto &member = field(value, key);
    return member.is_null() ? std::move(otherwise) : boost::json::value_to<T>(member);
}
inline bool contains(const Json &value, boost::json::string_view key) {
    return value.is_object() && value.as_object().contains(key);
}
inline const boost::json::array &arrayItems(const Json &value) {
    static const boost::json::array empty;
    return value.is_null() ? empty : value.as_array();
}
template <class... Keys>
const boost::json::array &arrayItems(const Json &value, boost::json::string_view key,
                                     const Keys &...keys) {
    return arrayItems(field(value, key, keys...));
}
Json parseJson(boost::json::string_view text);
} // namespace km
