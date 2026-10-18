// Report published configuration and native diagnostics.
#include "diagnostics.hpp"
#include "configuration.hpp"
#include "runtime-files.hpp"
#include <regex>
namespace kppmods {
Json runtimeStatus(const Configuration &configuration, bool operationPending) {
    boost::json::object result;
    result["config"] = boost::json::value_from(configuration);
    result["operation_pending"] = operationPending;
    result["native"] = boost::json::array{};
    for (const auto &entry : std::filesystem::directory_iterator(runRoot + "/native")) {
        const auto name = entry.path().filename().string();
        static const std::regex receipt(R"((.+)-([0-9]+)\.status)");
        std::smatch identity;
        if (!std::regex_match(name, identity, receipt)) {
            continue;
        }
        const std::string process = identity[1], pid = identity[2];
        std::string environment;
        try {
            // /proc/TID also exists: an old PID's receipt must not describe a
            // live thread that later reused that number.
            if (readFile("/proc/" + pid + "/status").find("Tgid:\t" + pid + "\n") ==
                std::string::npos) {
                continue;
            }
            environment = readFile("/proc/" + pid + "/environ");
        } catch (...) {
            continue;
        }
        if (environment.find("KPPMOD_STATUS_DIR=" + runRoot + "/native") ==
                std::string::npos ||
            environment.find("KPPMOD_PROCESS=" + process) == std::string::npos) {
            continue;
        }
        auto contents = readFile(runRoot + "/native/" + name);
        auto latest = contents.rfind("\thost\tconfiguration ");
        if (latest != std::string::npos) {
            auto line = contents.rfind('\n', latest);
            if (line != std::string::npos) {
                contents.erase(0, line + 1);
            }
        }
        boost::json::array entries;
        int errors = 0;
        static const std::regex record(R"((?:^|\n)([0-7])\t([^\t\n]*)\t([^\n]*))");
        for (auto it = std::sregex_iterator(contents.begin(), contents.end(), record);
             it != std::sregex_iterator(); ++it) {
            const int priority = std::stoi((*it)[1]);
            errors += priority <= 3;
            entries.push_back(boost::json::object{{"priority", priority},
                                                  {"module", (*it)[2].str()},
                                                  {"message", (*it)[3].str()}});
        }
        boost::json::object diagnostics{{"process", process},
                                        {"pid", std::stoi(pid)},
                                        {"entries", std::move(entries)},
                                        {"errors", errors}};
        result["native"].as_array().push_back(Json(diagnostics));
    }
    return result;
}

} // namespace kppmods
