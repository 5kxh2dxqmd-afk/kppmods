#include "child-process.hpp"
#include <algorithm>
#include <array>
#include <boost/asio/io_context.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/process/v2/environment.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/start_dir.hpp>
#include <boost/process/v2/stdio.hpp>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdexcept>
#include <sys/wait.h>

namespace kppmods::process {
namespace {
struct Account {
    const std::string &user;
    uid_t uid;
    gid_t gid;
    boost::system::error_code on_exec_setup(bp::posix::default_launcher &,
                                            const std::filesystem::path &,
                                            const char *const *) {
        if (!user.empty() && setsid() < 0) {
            return boost::system::error_code(errno, boost::system::generic_category());
        }
        for (int sig : {SIGTERM, SIGINT, SIGPIPE}) {
            signal(sig, SIG_DFL);
        }
        sigset_t mask;
        sigemptyset(&mask);
        if (sigprocmask(SIG_SETMASK, &mask, nullptr) ||
            (!user.empty() && geteuid() == 0 &&
             (initgroups(user.c_str(), gid) || setgid(gid) || setuid(uid)))) {
            return boost::system::error_code(errno, boost::system::generic_category());
        }
        return {};
    }
};
} // namespace
bp::process spawn(asio::io_context &context, const std::vector<std::string> &argv,
                  bp::process_stdio stdio, const std::string &cwd,
                  const std::string &user, Environment environment) {
    if (argv.empty() || argv.front().empty()) {
        throw std::runtime_error("Expected a nonempty command");
    }
    for (const auto &arg : argv) {
        if (arg.find('\0') != std::string::npos) {
            throw std::runtime_error("NUL in command argument");
        }
    }
    Account account{user, geteuid(), getegid()};
    if (!user.empty()) {
        const auto *accountInfo = getpwnam(user.c_str());
        if (!accountInfo || (geteuid() != 0 && geteuid() != accountInfo->pw_uid)) {
            throw std::runtime_error("Cannot run as " + user);
        }
        account.uid = accountInfo->pw_uid;
        account.gid = accountInfo->pw_gid;
        environment["HOME"] = accountInfo->pw_dir;
        environment["USER"] = user;
    }
    for (char **entry = environ; *entry; ++entry) {
        std::string value(*entry);
        const auto split = value.find('=');
        environment.try_emplace(value.substr(0, split), value.substr(split + 1));
    }
    for (const char *key : {"LD_PRELOAD", "KPPMOD_CONFIG", "KPPMOD_PROCESS"}) {
        environment.erase(key);
    }
    std::vector<std::string> env;
    for (const auto &[key, value] : environment) {
        env.push_back(key + "=" + value);
    }
    auto executable = std::filesystem::path(argv[0]);
    if (!executable.is_absolute()) {
        executable = bp::environment::find_executable(argv[0]);
    }
    return bp::process(
        context, executable, std::vector<std::string>(argv.begin() + 1, argv.end()),
        stdio, bp::process_environment(env),
        bp::process_start_dir(cwd.empty() ? std::filesystem::current_path()
                                          : std::filesystem::path(cwd)),
        account);
}
int exitCode(const bp::process &child) {
    const int status = child.native_exit_code();
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
Command::Command(const std::vector<std::string> &argv, const std::string &cwd,
                 const std::string &user, Environment environment, int timeout)
    : child(spawn(context, argv, {nullptr, out.pipe, err.pipe}, cwd, user,
                  std::move(environment))) {
    read(out);
    read(err);
    deadline.expires_after(std::chrono::seconds(timeout));
    deadline.async_wait([this](boost::system::error_code error) {
        if (!error && child.running()) {
            timedOut = true;
            killGroup();
        }
    });
}
void Command::killGroup() {
    if (child.id() > 1) {
        kill(-child.id(), SIGKILL);
    }
}
Command::~Command() {
    if (!result) {
        killGroup();
        boost::system::error_code error;
        child.wait(error);
    }
}
void Command::read(Output &output) {
    output.pipe.async_read_some(
        asio::buffer(output.buffer),
        [this, &output](boost::system::error_code error, size_t count) {
            const auto keep = std::min(count, size_t(65536) - output.text.size());
            output.text.append(output.buffer.data(), keep);
            truncated |= keep != count;
            if (!error) {
                read(output);
            }
        });
}
bool Command::poll() {
    if (result) {
        return false;
    }
    context.restart();
    for (int i = 0; i < 64 && context.poll_one(); ++i) {
    }
    if (child.running()) {
        return false;
    }
    // Foreground commands must not leave descendants or inherited pipes behind.
    killGroup();
    deadline.cancel();
    context.restart();
    for (int i = 0; i < 64 && context.poll_one(); ++i) {
    }
    out.pipe.close();
    err.pipe.close();
    result = exitCode(child);
    return true;
}
void Command::cancel() {
    if (!result) {
        cancelled = true;
        killGroup();
    }
}
} // namespace kppmods::process
