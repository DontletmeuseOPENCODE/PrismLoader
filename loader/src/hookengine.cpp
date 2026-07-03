#include <Prism/HookEngine.hpp>
#include <Log.hpp>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#endif

namespace prism {

HookEngine& HookEngine::get() {
    static HookEngine instance;
    return instance;
}

HookEngine::~HookEngine() {
    std::lock_guard lock(mutex);
    for (auto& [addr, hook] : hooks) {
        if (hook.active)
            restoreOriginal(hook);
    }
}

#ifdef _WIN32
static bool setMemoryProtection(void* addr, size_t size, DWORD prot) {
    DWORD oldProt;
    return VirtualProtect(addr, size, prot, &oldProt) != FALSE;
}

static void flushICache(void* addr, size_t size) {
    FlushInstructionCache(GetCurrentProcess(), addr, size);
}
#else
static bool setMemoryProtection(void* addr, size_t size, int prot) {
    return mprotect(addr, size, prot) == 0;
}

static void flushICache(void* addr, size_t size) {
    __builtin___clear_cache(reinterpret_cast<char*>(addr),
                            reinterpret_cast<char*>(addr) + size);
}
#endif

bool HookEngine::installHook(void* target, void* detour, Hook* out) {
    std::lock_guard lock(mutex);

    if (hooks.count(target)) {
        logMsg("[Prism] Hook already exists at %p", target);
        return false;
    }

    Hook hook;
    hook.target = target;
    hook.detour = detour;
    hook.active = false;

    hook.originalBytes.resize(14);
    memcpy(hook.originalBytes.data(), target, 14);

    hook.trampoline = createTrampoline(target, 14);
    if (!hook.trampoline) {
        logMsg("[Prism] Failed to create trampoline for %p", target);
        return false;
    }

    if (!applyDetour(hook)) {
#ifdef _WIN32
        VirtualFree(hook.trampoline, 0, MEM_RELEASE);
#else
        munmap(hook.trampoline, 64);
#endif
        return false;
    }

    hooks[target] = hook;

    if (out)
        *out = hook;

    return true;
}

bool HookEngine::removeHook(void* target) {
    std::lock_guard lock(mutex);
    auto it = hooks.find(target);
    if (it == hooks.end())
        return false;

    bool result = restoreOriginal(it->second);
    it->second.active = false;
    hooks.erase(it);
    return result;
}

bool HookEngine::removeHook(Hook* hook) {
    if (!hook) return false;
    return removeHook(hook->target);
}

void* HookEngine::createTrampoline(void* target, int numPrefixBytes) {
#ifdef _WIN32
    void* trampoline = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline)
        return nullptr;
#else
    void* trampoline = mmap(nullptr, 64, PROT_READ | PROT_WRITE | PROT_EXEC,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (trampoline == MAP_FAILED)
        return nullptr;
#endif

    auto* code = static_cast<uint8_t*>(trampoline);

    memcpy(code, target, numPrefixBytes);
    code += numPrefixBytes;

    code[0] = 0xFF;
    code[1] = 0x25;
    code[2] = 0x00;
    code[3] = 0x00;
    code[4] = 0x00;
    code[5] = 0x00;

    void* jumpTarget = static_cast<uint8_t*>(target) + numPrefixBytes;
    memcpy(code + 6, &jumpTarget, sizeof(void*));

#ifdef _WIN32
    DWORD old;
    VirtualProtect(trampoline, 64, PAGE_EXECUTE_READ, &old);
#endif

    return trampoline;
}

bool HookEngine::applyDetour(Hook& hook) {
    if (hook.active) return true;

#ifdef _WIN32
    constexpr int protRW = PAGE_READWRITE;
    constexpr int protRX = PAGE_EXECUTE_READ;
#else
    constexpr int protRW = PROT_READ | PROT_WRITE;
    constexpr int protRX = PROT_READ | PROT_EXEC;
#endif

    if (!setMemoryProtection(hook.target, 14, protRW)) {
        logMsg("[Prism] setMemoryProtection(RW) failed");
        return false;
    }

    auto* code = static_cast<uint8_t*>(hook.target);

    code[0] = 0xFF;
    code[1] = 0x25;
    code[2] = 0x00;
    code[3] = 0x00;
    code[4] = 0x00;
    code[5] = 0x00;
    memcpy(code + 6, &hook.detour, sizeof(void*));

    flushICache(code, 14);

    setMemoryProtection(hook.target, 14, protRX);

    hook.active = true;
    return true;
}

bool HookEngine::restoreOriginal(Hook& hook) {
    if (!hook.active) return true;

#ifdef _WIN32
    constexpr int protRW = PAGE_READWRITE;
    constexpr int protRX = PAGE_EXECUTE_READ;
#else
    constexpr int protRW = PROT_READ | PROT_WRITE;
    constexpr int protRX = PROT_READ | PROT_EXEC;
#endif

    if (!setMemoryProtection(hook.target, 14, protRW))
        return false;

    memcpy(hook.target, hook.originalBytes.data(), 14);
    flushICache(hook.target, 14);

    setMemoryProtection(hook.target, 14, protRX);
    hook.active = false;

    return true;
}

} // namespace prism

#include <Prism/Hook.hpp>
#include <Prism/PatternScan.hpp>

extern "C" {

__attribute__((visibility("default")))
bool prism_hook(void* target, void* detour, prism::HookHandle* out) {
    prism::Hook hook;
    bool success = prism::HookEngine::get().installHook(target, detour, &hook);
    if (success && out) {
        out->target = hook.target;
        out->detour = hook.detour;
        out->trampoline = hook.trampoline;
    }
    return success;
}

__attribute__((visibility("default")))
bool prism_unhook(prism::HookHandle* handle) {
    if (!handle) return false;
    return prism::HookEngine::get().removeHook(handle->target);
}

__attribute__((visibility("default")))
void* prism_resolve_address(const char* symbol) {
    auto addr = prism::PatternScanner::get().resolveSymbol(symbol);
    return addr ? reinterpret_cast<void*>(*addr) : nullptr;
}

}
