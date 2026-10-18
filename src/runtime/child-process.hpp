#pragma once
#include <array>
#include <boost/asio/io_context.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>
#include <map>
#include <optional>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace kppmods::process {
namespace asio = boost::asio;
namespace bp = boost::process::v2;
using Environment = std::map<std::string, std::string>;
bp::process spawn(asio::io_context &context, const std::vector<std::string> &argv,
                  bp::process_stdio stdio = {}, const std::string &cwd = "",
                  const std::string &user = "", Environment environment = {});
int exitCode(const bp::process &child);

// Captured commands own a process group and drain both streams even after their
// retention limit is reached. The broker pumps their executors under its mutex.
struct Command {
    asio::io_context context;
    struct Output {
        asio::readable_pipe pipe;
        std::array<char, 4096> buffer;
        std::string text;
    };
    Output out{asio::readable_pipe(context), {}, {}},
        err{asio::readable_pipe(context), {}, {}};
    bp::process child;
    asio::steady_timer deadline{context};
    std::optional<int> result;
    bool timedOut = false, cancelled = false, truncated = false;

    Command(const std::vector<std::string> &argv, const std::string &cwd,
            const std::string &user, Environment environment, int timeout);
    ~Command();
    bool poll();
    void cancel();

  private:
    void read(Output &output);
    void killGroup();
};
} // namespace kppmods::process
