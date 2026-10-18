// Persistence and LIPC delivery used by the management broker.
#include "broker.hpp"
namespace kppmods {
namespace runtime {
void saveJsonAtomically(const std::string &path, const Json &value) {
    km::files::writeFileAtomically(path, boost::json::serialize(value), 0600);
}

} // namespace runtime
} // namespace kppmods
