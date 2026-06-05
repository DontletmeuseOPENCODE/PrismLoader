#include <Prism/ModLoader.hpp>
#include <Prism/Mod.hpp>
#include <Log.hpp>
#include <cstdio>
#include <cstring>
#include <string>
#include <windows.h>

namespace prism {

static std::string getModsDirectory() {
    char path[MAX_PATH];
    DWORD len = GetEnvironmentVariableA("USERPROFILE", path, MAX_PATH);
    if (len == 0)
        GetEnvironmentVariableA("HOME", path, MAX_PATH);
    return std::string(path) + "\\.prismloader\\mods";
}

ModLoader& ModLoader::get() {
    static ModLoader instance;
    return instance;
}

bool ModLoader::parseMetadata(const std::string& path, std::string& id) {
    auto dirEnd = path.find_last_of("\\/");
    if (dirEnd == std::string::npos) return false;

    std::string dir = path.substr(0, dirEnd);
    std::string metaPath = dir + "\\mod.json";

    FILE* f = fopen(metaPath.c_str(), "r");
    if (!f) return false;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* pos = strstr(line, "\"id\"");
        if (pos) {
            char* colon = strchr(pos, ':');
            if (colon) {
                char* start = strchr(colon + 1, '"');
                if (start) {
                    char* end = strchr(start + 1, '"');
                    if (end) {
                        *end = '\0';
                        id = start + 1;
                        fclose(f);
                        return true;
                    }
                }
            }
        }
    }

    fclose(f);
    return false;
}

bool ModLoader::loadMod(const std::string& path) {
    std::string id;
    if (!parseMetadata(path, id)) {
        auto lastSlash = path.find_last_of("\\/");
        auto lastDot = path.find_last_of('.');
        if (lastSlash != std::string::npos) {
            id = path.substr(lastSlash + 1, lastDot - lastSlash - 1);
        } else {
            id = path.substr(0, lastDot);
        }
    }

    for (auto& m : mods) {
        if (m.id == id) {
            logMsg("[Prism] Mod '%s' already loaded", id.c_str());
            return false;
        }
    }

    HMODULE handle = LoadLibraryA(path.c_str());
    if (!handle) {
        logMsg("[Prism] Failed to load mod '%s': %lu", path.c_str(), GetLastError());
        return false;
    }

    auto initFn = reinterpret_cast<Mod* (*)()>(GetProcAddress(handle, "prismModInit"));
    if (!initFn) {
        logMsg("[Prism] Mod '%s' has no prismModInit", path.c_str());
        FreeLibrary(handle);
        return false;
    }

    Mod* mod = initFn();
    if (!mod) {
        FreeLibrary(handle);
        return false;
    }

    ModEntry entry;
    entry.id = id;
    entry.path = path;
    entry.handle = handle;
    entry.instance.reset(mod);
    entry.enabled = true;

    mods.push_back(std::move(entry));

    logMsg("[Prism] Loaded mod: %s (%s)", id.c_str(), path.c_str());

    return true;
}

bool ModLoader::unloadMod(const std::string& id) {
    for (auto it = mods.begin(); it != mods.end(); ++it) {
        if (it->id == id) {
            it->instance->onUnload();
            it->instance.reset();

            auto exitFn = reinterpret_cast<void(*)(Mod*)>(GetProcAddress(
                (HMODULE)it->handle, "prismModExit"));
            if (exitFn) exitFn(nullptr);

            FreeLibrary((HMODULE)it->handle);

            logMsg("[Prism] Unloaded mod: %s", id.c_str());

            mods.erase(it);
            return true;
        }
    }
    return false;
}

std::vector<std::string> ModLoader::discoverMods(const std::string& directory) {
    std::vector<std::string> found;

    std::string search = directory + "\\*.dll";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(search.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE)
        return found;

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string fullPath = directory + "\\" + findData.cFileName;
            found.push_back(fullPath);
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return found;
}

void ModLoader::enableMod(const std::string& id) {
    for (auto& m : mods) {
        if (m.id == id) {
            m.enabled = true;
            m.instance->onLoad();
            break;
        }
    }
}

void ModLoader::disableMod(const std::string& id) {
    for (auto& m : mods) {
        if (m.id == id) {
            m.instance->onUnload();
            m.enabled = false;
            break;
        }
    }
}

} // namespace prism
