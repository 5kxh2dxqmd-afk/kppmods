#include "filesystem.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace km::files {
std::string readFile(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot read " + path);
    }
    std::string contents(std::istreambuf_iterator<char>(input), {});
    if (input.bad()) {
        throw std::runtime_error("Read failed: " + path);
    }
    return contents;
}
boost::json::object readSettings(const std::string &path,
                                 boost::json::object defaults) {
    if (std::filesystem::exists(path)) {
        const auto persisted = parseJson(readFile(path));
        for (const auto &entry : persisted.as_object()) {
            defaults[entry.key()] = entry.value();
        }
    }
    return defaults;
}
void writeFileAtomically(const std::string &path, const std::string &contents,
                         mode_t mode) {
    std::string temporary = path + ".XXXXXX";
    km::files::FileDescriptor file(mkstemp(temporary.data()));
    const auto fail = [&] {
        throw std::runtime_error("Cannot publish " + path + ": " + strerror(errno));
    };
    if (file.get() < 0) {
        fail();
    }
    try {
        if (fcntl(file.get(), F_SETFD, FD_CLOEXEC) || fchmod(file.get(), mode)) {
            fail();
        }
        size_t offset = 0;
        while (offset < contents.size()) {
            const auto count =
                write(file.get(), contents.data() + offset, contents.size() - offset);
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count <= 0) {
                fail();
            }
            offset += count;
        }
        if (fsync(file.get())) {
            fail();
        }
        file.reset();
        if (rename(temporary.c_str(), path.c_str())) {
            fail();
        }
    } catch (...) {
        unlink(temporary.c_str());
        throw;
    }
    auto parent = std::filesystem::path(path).parent_path();
    if (parent.empty()) {
        parent = ".";
    }
    km::files::FileDescriptor directory(
        open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (directory.get() < 0 || fsync(directory.get())) {
        fail();
    }
}
} // namespace km::files
