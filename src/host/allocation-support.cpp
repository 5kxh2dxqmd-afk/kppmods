#include <cxxabi.h>
#include <exception>
#include <new>

// GCC's allocator headers call this out-of-line helper for an impossible array
// length. Kindle's libstdc++ has the standard exception type but not this newer
// helper export. Keep its exact semantics local to each DSO, retaining the
// device's shared C++ runtime and exception type identity.
namespace std {
__attribute__((noreturn, visibility("hidden"))) void __throw_bad_array_new_length() {
    throw bad_array_new_length();
}
} // namespace std

// GCC 14 emits this helper for noexcept cleanup. Keep it local to the injected
// DSO: exception ownership and termination still belong to Kindle's C++ runtime.
extern "C" __attribute__((noreturn, visibility("hidden"))) void
kppmods_call_terminate(void *) noexcept asm("__cxa_call_terminate");
extern "C" void kppmods_call_terminate(void *exception) noexcept {
    if (exception) {
        __cxxabiv1::__cxa_begin_catch(exception);
    }
    std::terminate();
}
