#pragma once
#include "common/json.hpp"
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace km::files {
class FileDescriptor {
    int value_ = -1;

  public:
    FileDescriptor() = default;
    explicit FileDescriptor(int value) : value_(value) {}
    ~FileDescriptor() {
        reset();
    }
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;
    FileDescriptor(FileDescriptor &&other) noexcept
        : value_(std::exchange(other.value_, -1)) {}
    FileDescriptor &operator=(FileDescriptor &&other) noexcept {
        reset(std::exchange(other.value_, -1));
        return *this;
    }
    int get() const {
        return value_;
    }
    void reset(int value = -1) {
        if (value_ >= 0) {
            close(value_);
        }
        value_ = value;
    }
};

std::string readFile(const std::string &path);
boost::json::object readSettings(const std::string &path, boost::json::object defaults);
void writeFileAtomically(const std::string &path, const std::string &contents,
                         mode_t mode = 0644);
} // namespace km::files
