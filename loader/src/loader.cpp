#include <Prism/Loader.hpp>
#include <Prism/ModLoader.hpp>
#include <Prism/HookEngine.hpp>
#include <Prism/PatternScan.hpp>
#include <Prism/ModMenu.hpp>
#include <Log.hpp>
#include <windows.h>
#include <string>

namespace prism {

extern bool detectGdInit();

Loader& Loader::get() {
    static Loader instance;
    return instance;
}

Loader::~Loader() {
    unloadMods();
}

bool Loader::init() {
    char profile[MAX_PATH];
    DWORD len = GetEnvironmentVariableA("USERPROFILE", profile, MAX_PATH);
    if (len == 0)
        GetEnvironmentVariableA("HOME", profile, MAX_PATH);
    modsDir = std::string(profile) + "\\.prismloader\\mods";

    CreateDirectoryA(modsDir.c_str(), nullptr);

    if (!PatternScanner::get().load()) {
        logMsg("[Prism] PatternScanner failed");
        return false;
    }

    detectGdInit();
    installModMenuHook();

    logMsg("[Prism] Loader ready. Mods dir: %s", modsDir.c_str());

    return true;
}

void Loader::loadMods() {
    auto found = ModLoader::get().discoverMods(modsDir);
    for (auto& path : found) {
        ModLoader::get().loadMod(path);
    }

    logMsg("[Prism] Loaded %zu mod(s)", found.size());
}

void Loader::unloadMods() {
    // stub
}

PrismApi Loader::getApi() const {
    PrismApi api{};
    return api;
}

} // namespace prism

bool prismInit() {
    return prism::Loader::get().init();
}

void prismShutdown() {
    prism::Loader::get().unloadMods();
}
