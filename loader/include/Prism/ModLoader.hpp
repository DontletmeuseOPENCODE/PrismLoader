#pragma once
#include <string_view>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <Prism/Mod.hpp>

namespace prism {

class ModLoader {
public:
    using ModFactory = std::unique_ptr<Mod>(*)();

    static ModLoader& get();

    bool loadMod(const std::string& path);
    bool unloadMod(const std::string& id);
    std::vector<std::string> discoverMods(const std::string& directory);

    void enableMod(const std::string& id);
    void disableMod(const std::string& id);

private:
    ModLoader() = default;

    struct ModEntry {
        std::string id;
        std::string path;
        void* handle;
        std::unique_ptr<Mod> instance;
        bool enabled;
    };

    std::vector<ModEntry> mods;

    bool parseMetadata(const std::string& path, std::string& id);
};

} // namespace prism
