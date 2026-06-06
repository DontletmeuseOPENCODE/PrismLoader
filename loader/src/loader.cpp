#include <Prism/Loader.hpp>
#include <Prism/ModLoader.hpp>
#include <Prism/HookEngine.hpp>
#include <Prism/PatternScan.hpp>
#include <Prism/ModMenu.hpp>
#include <Log.hpp>

#ifdef _WIN32
#include <windows.h>
#include <string>
#else
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <cstring>
#include <string>
#endif

namespace prism {

#ifdef _WIN32
extern bool detectGdInit();
#endif

Loader& Loader::get() {
    static Loader instance;
    return instance;
}

Loader::~Loader() {
    unloadMods();
}

bool Loader::init() {
#ifdef _WIN32
    char profile[MAX_PATH];
    DWORD len = GetEnvironmentVariableA("USERPROFILE", profile, MAX_PATH);
    if (len == 0)
        GetEnvironmentVariableA("HOME", profile, MAX_PATH);
    modsDir = std::string(profile) + "\\.prismloader\\mods";
    CreateDirectoryA(modsDir.c_str(), nullptr);
#else
    const char* profile = getenv("HOME");
    if (!profile) profile = "/tmp";
    modsDir = std::string(profile) + "/.prismloader/mods";

    // Best-effort mkdir -p for ~/.prismloader/mods.
    std::string prismDir = std::string(profile) + "/.prismloader";
    mkdir(prismDir.c_str(), 0755);
    mkdir(modsDir.c_str(), 0755);
#endif

    if (!PatternScanner::get().load()) {
        logMsg("[Prism] PatternScanner failed");
        return false;
    }

#ifdef _WIN32
    detectGdInit();
    installModMenuHook();
#endif

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
    auto& loader = ModLoader::get();
    const auto& entries = loader.getMods();
    for (const auto& m : entries) {
        loader.unloadMod(m.id);
    }
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
