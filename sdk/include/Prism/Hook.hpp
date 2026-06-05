#pragma once
#include <cstdint>
#include <functional>
#include <optional>

namespace prism {

enum class HookResult {
    Ok,
    AlreadyHooked,
    AddressNotFound,
    PermissionError,
};

struct HookHandle {
    void* detour;
    void* trampoline;
    void* target;
};

using HookCallback = std::function<void()>;

bool hook(void* target, void* detour, HookHandle* out = nullptr);
bool unhook(HookHandle* handle);

std::optional<uintptr_t> resolveAddress(const char* symbol);

} // namespace prism

#define PRISM_HOOK(ret, name, args...) \
    ret name##_Detour(args); \
    using name##_Fn = ret(*)(args); \
    name##_Fn name = nullptr; \
    \
    __attribute__((constructor)) void name##_HookInit() { \
        auto addr = prism::resolveAddress(#name); \
        if (addr) { \
            prism::HookHandle h; \
            prism::hook((void*)*addr, (void*)name##_Detour, &h); \
            name = (name##_Fn)h.trampoline; \
        } \
    } \
    ret name##_Detour(args)
