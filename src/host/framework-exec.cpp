#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>
extern char **environ;

// The framework job rewrites LD_PRELOAD before executing its Java VM. Preserve
// the activation generation and its composed preloads at that exact boundary.
extern "C" __attribute__((visibility("default"))) int
execve(const char *path, char *const argv[], char *const envp[]) {
    using Exec = int (*)(const char *, char *const[], char *const[]);
    static Exec original = reinterpret_cast<Exec>(dlsym(RTLD_NEXT, "execve"));
    if (!original) {
        errno = ENOSYS;
        return -1;
    }
    if (strcmp(path, "/usr/java/bin/cvm") && strcmp(path, "/usr/java/bin/java")) {
        return original(path, argv, envp);
    }
    try {
        std::vector<std::string> environmentStrings;
        const char *extensionPreloads = nullptr, *stockPreloads = nullptr;
        for (size_t i = 0; envp[i]; ++i) {
            if (!strncmp(envp[i], "LD_PRELOAD=", 11)) {
                stockPreloads = envp[i] + 11;
            } else {
                environmentStrings.emplace_back(envp[i]);
            }
            if (!strncmp(envp[i], "KPPMOD_FRAMEWORK_PRELOAD=", 25)) {
                extensionPreloads = envp[i] + 25;
            }
        }
        // Shells normally preserve these. Some launch wrappers pass a reduced
        // environment; recover only our own keys, never unrelated variables.
        for (size_t i = 0; environ && environ[i]; ++i) {
            if (strncmp(environ[i], "KPPMOD_", 7)) {
                continue;
            }
            const char *equals = strchr(environ[i], '=');
            if (!equals) {
                continue;
            }
            size_t keySize = static_cast<size_t>(equals - environ[i]) + 1;
            bool exists = false;
            for (const auto &value : environmentStrings) {
                if (!value.compare(0, keySize, environ[i], keySize)) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                environmentStrings.emplace_back(environ[i]);
            }
        }
        if (!extensionPreloads) {
            extensionPreloads = getenv("KPPMOD_FRAMEWORK_PRELOAD");
        }
        if (!extensionPreloads || !*extensionPreloads) {
            return original(path, argv, envp);
        }
        std::string preload = "LD_PRELOAD=";
        preload += extensionPreloads;
        if (stockPreloads && *stockPreloads) {
            preload += ':';
            preload += stockPreloads;
        }
        environmentStrings.push_back(std::move(preload));
        std::vector<char *> environment;
        for (auto &value : environmentStrings) {
            environment.push_back(&value[0]);
        }
        environment.push_back(nullptr);
        return original(path, argv, environment.data());
    } catch (...) {
        errno = ENOMEM;
        return -1;
    }
}
