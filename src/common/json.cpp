#include "json.hpp"
#include <boost/json/src.hpp>

namespace km {
Json parseJson(boost::json::string_view text) {
    boost::json::parse_options options;
    options.max_depth = 128;
    options.numbers = boost::json::number_precision::precise;
    return boost::json::parse(text, {}, options);
}
} // namespace km
