#include <Prism/Mod.hpp>
#include <Prism/Log.hpp>
#include <cstdio>

class ExampleMod : public prism::Mod {
public:
    prism::ModInfo getInfo() const override {
        return {
            .id = "example",
            .name = "Example Mod",
            .version = "1.0.0",
            .author = "PrismLoader",
            .description = "An example mod for PrismLoader",
        };
    }

    void onLoad() override {
        PRISM_MOD_LOG(Info, "Example mod loaded! Hello from PrismLoader!");
    }

    void onUnload() override {
        PRISM_MOD_LOG(Info, "Example mod unloaded!");
    }
};

PRISM_MOD(ExampleMod)
