#pragma once
#include <boost/json.hpp>
#include <string>
namespace km::hermes {
boost::json::object discover(const std::string &bundle);
}
