// Child processes run without inheriting the runtime's preloads.
#include "process.hpp"
#include "child-process.hpp"
#include <boost/asio/read.hpp>

namespace kppmods {
int runCommand(const std::vector<std::string> &args) {
    process::asio::io_context context;
    auto child = process::spawn(context, args);
    child.wait();
    return process::exitCode(child);
}
std::string captureCommand(const std::vector<std::string> &args) {
    process::asio::io_context context;
    process::asio::readable_pipe output(context);
    auto child = process::spawn(context, args, {{}, output, {}});
    std::string text;
    boost::system::error_code error;
    process::asio::read(output, process::asio::dynamic_buffer(text), error);
    if (error != process::asio::error::eof) {
        throw boost::system::system_error(error);
    }
    child.wait();
    return text;
}
} // namespace kppmods
