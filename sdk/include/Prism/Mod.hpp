#pragma once
#include <string_view>
#include <cstdint>

namespace prism {

struct ModInfo {
    std::string_view id;
    std::string_view name;
    std::string_view version;
    std::string_view author;
    std::string_view description;
};

class Mod {
public:
    Mod() = default;
    virtual ~Mod() = default;

    virtual ModInfo getInfo() const = 0;
    virtual void onLoad()  {}
    virtual void onUnload() {}

    Mod(const Mod&) = delete;
    Mod& operator=(const Mod&) = delete;
};

} // namespace prism

#define PRISM_MOD(modClass) \
    extern "C" { \
        prism::Mod* prismModInit() { return new modClass(); } \
        void prismModExit(prism::Mod* mod) { delete mod; } \
    }
