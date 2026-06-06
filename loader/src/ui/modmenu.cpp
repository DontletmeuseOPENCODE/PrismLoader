// PrismLoader - Mod Menu UI
// Hooks MenuLayer::init to inject a "Mod Library" button (like Geode) using raw Cocos2d API.

#ifdef _WIN32

#include <Prism/HookEngine.hpp>
#include <Prism/PatternScan.hpp>
#include <Prism/ModLoader.hpp>
#include <Log.hpp>
#include <windows.h>
#include <psapi.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

namespace prism {

// ─── Cocos2d forward declarations ───────────────────────────────────────────

struct CCPoint { float x, y; };
struct CCSize  { float width, height; };
struct CCRect  { CCPoint origin; CCSize size; };
struct ccColor4B { unsigned char r, g, b, a; };
struct ccColor3B { unsigned char r, g, b; };

struct CCObject {
    virtual ~CCObject() {}
    void* vt2[10]; // padding for vtable slots we don't know
    unsigned int m_uAutoReleaseCount;
    unsigned int m_uReference;
    unsigned int m_uID;
    void* m_pUserObject;
};

// Function pointer types from libcocos2d.dll
using FnCCLabelBMFontCreate = void* (*)(const char* str, const char* fntFile);
using FnCCLabelTTFCreate    = void* (*)(const char* text, const char* font, float size);
using FnCCSpriteCreate      = void* (*)(const char* filename);
using FnCCMenuCreate        = void* (*)(void* item, ...);
using FnCCMenuCreateEmpty   = void* (*)();
using FnCCMenuItemSpriteCreate = void* (*)(void* normalSprite, void* selectedSprite, void* target, void(*selector)(void*, void*));
using FnCCNodeAddChild      = void (*)(void* node, void* child);
using FnCCNodeAddChildZ     = void (*)(void* node, void* child, int z);
using FnCCNodeSetPosition   = void (*)(void* node, CCPoint pos);
using FnCCNodeSetAnchorPoint = void (*)(void* node, CCPoint anchor);
using FnCCNodeSetScale      = void (*)(void* node, float scale);
using FnCCNodeGetContentSize = CCSize (*)(void* node);
using FnCCNodeSetZOrder     = void (*)(void* node, int z);
using FnCCLayerColorCreate  = void* (*)(const ccColor4B& color, float w, float h);
using FnCCDirectorShared    = void* (*)();
using FnCCDirectorGetScene  = void* (*)(void* director);
using FnCCDirectorGetSize   = CCSize (*)(void* director);

// GD-specific: FLAlertLayer::show() — used for simple popups
// We'll do our own custom popup layer instead.

// ─── Symbol resolver helpers ─────────────────────────────────────────────────

static HMODULE g_cocos = nullptr;

static HMODULE getCocos() {
    if (!g_cocos) {
        g_cocos = GetModuleHandleA("libcocos2d.dll");
    }
    return g_cocos;
}

template<typename T>
static T sym(const char* name) {
    HMODULE h = getCocos();
    if (!h) return nullptr;
    return reinterpret_cast<T>(GetProcAddress(h, name));
}

// ─── Popup layer ─────────────────────────────────────────────────────────────

// We'll hook MenuLayer by scanning for a known byte pattern in GeometryDash.exe.
// Address 0x333a90 (from Geode bindings, relative to exe base) is a known good offset
// for MenuLayer::init in GD 2.206 Win64.

static constexpr uintptr_t k_MenuLayerInitOffset = 0x333a90;

// Forward declare our detour
using MenuLayerInitFn = bool(__thiscall*)(void* self);
static MenuLayerInitFn g_originalMenuLayerInit = nullptr;

// ─── Prism button callback ────────────────────────────────────────────────────

// We create a lightweight CCLayerColor popup overlay with a list of mod names.
static void onPrismButtonClicked(void* target, void* sender) {
    logMsg("[PrismUI] Mod Library button clicked");

    // Get CCDirector
    auto sharedDirectorFn = sym<FnCCDirectorShared>("?sharedDirector@CCDirector@cocos2d@@SAPEAV12@XZ");
    auto getSceneFn       = sym<FnCCDirectorGetScene>("?getRunningScene@CCDirector@cocos2d@@QEAAPEAVCC""Scene@2@XZ");
    auto getWinSizeFn     = sym<FnCCDirectorGetSize>("?getWinSize@CCDirector@cocos2d@@QEBA?AVCCSize@2@XZ");
    auto layerColorCreateFn = sym<FnCCLayerColorCreate>("?create@CCLayerColor@cocos2d@@SAPEAV12@AEBU_ccColor4B@2@MM@Z");
    auto addChildZFn      = sym<FnCCNodeAddChildZ>("?addChild@CCNode@cocos2d@@UEAAXPEAV12@H@Z");
    auto setPosFn         = sym<FnCCNodeSetPosition>("?setPosition@CCNode@cocos2d@@UEAAXAEBVCCPoint@2@@Z");
    auto labelCreateFn    = sym<FnCCLabelBMFontCreate>("?create@CCLabelBMFont@cocos2d@@SAPEAV12@PEBD0@Z");
    auto setScaleFn       = sym<FnCCNodeSetScale>("?setScale@CCNode@cocos2d@@UEAAXM@Z");
    auto getCSizeFn       = sym<FnCCNodeGetContentSize>("?getContentSize@CCNode@cocos2d@@UEBAAEBVCCSize@2@XZ");

    if (!sharedDirectorFn || !getSceneFn || !getWinSizeFn || !layerColorCreateFn) {
        logMsg("[PrismUI] Failed to resolve cocos2d functions for popup");
        return;
    }

    void* director = sharedDirectorFn();
    if (!director) return;

    void* scene = getSceneFn(director);
    if (!scene) return;

    CCSize winSize = getWinSizeFn(director);

    // Create a dark semi-transparent background panel
    ccColor4B bgColor = {0, 0, 0, 180};
    void* bg = layerColorCreateFn(bgColor, winSize.width * 0.6f, winSize.height * 0.7f);
    if (!bg) return;

    // Center it on screen
    CCPoint bgPos = { winSize.width * 0.2f, winSize.height * 0.15f };
    setPosFn(bg, bgPos);

    // Add label: "PrismLoader - Mods"
    if (labelCreateFn && setScaleFn) {
        void* title = labelCreateFn("PrismLoader - Mod Library", "bigFont.fnt");
        if (title) {
            CCSize bgSize = getCSizeFn ? getCSizeFn(bg) : CCSize{0,0};
            setPosFn(title, { bgSize.width * 0.5f, bgSize.height - 30.0f });
            setScaleFn(title, 0.5f);
            addChildZFn(bg, title, 2);
        }

        // List mods
        const auto& mods = ModLoader::get().getMods();
        if (mods.empty()) {
            void* noMods = labelCreateFn("No mods loaded.", "bigFont.fnt");
            if (noMods) {
                setPosFn(noMods, { getCSizeFn(bg).width * 0.5f, getCSizeFn(bg).height * 0.5f });
                setScaleFn(noMods, 0.4f);
                addChildZFn(bg, noMods, 2);
            }
        } else {
            float yOff = getCSizeFn(bg).height - 65.0f;
            for (size_t i = 0; i < mods.size() && i < 10; ++i) {
                const auto& m = mods[i];
                std::string info = (m.enabled ? "[ON]  " : "[OFF] ");
                info += m.id;
                if (m.instance) {
                    auto mi = m.instance->getInfo();
                    info += " - ";
                    info += mi.name.data();
                    info += " v";
                    info += mi.version.data();
                }
                void* lbl = labelCreateFn(info.c_str(), "bigFont.fnt");
                if (lbl) {
                    setPosFn(lbl, { 20.0f, yOff });
                    setScaleFn(lbl, 0.3f);
                    addChildZFn(bg, lbl, 2);
                    yOff -= 24.0f;
                }
            }
        }
    }

    // Add to scene at high z-order
    addChildZFn(scene, bg, 1000);

    logMsg("[PrismUI] Mod Library popup shown (%zu mod(s))", ModLoader::get().getMods().size());
}

// ─── MenuLayer::init hook ──────────────────────────────────────────────────

static bool __fastcall menuLayerInitDetour(void* self) {
    if (!g_originalMenuLayerInit(self))
        return false;

    logMsg("[PrismUI] MenuLayer::init hooked - injecting Mod Library button");

    // Resolve cocos2d functions
    auto spriteCreateFn    = sym<FnCCSpriteCreate>("?create@CCSprite@cocos2d@@SAPEAV12@PEBD@Z");
    auto menuItemCreateFn  = sym<FnCCMenuItemSpriteCreate>(
        "?create@CCMenuItemSprite@cocos2d@@SAPEAV12@PEAVCCNode@2@0PEAVCCObject@2@P842@EAAX1@Z@Z");
    auto menuCreateEmptyFn = sym<FnCCMenuCreateEmpty>("?create@CCMenu@cocos2d@@SAPEAV12@XZ");
    auto addChildFn        = sym<FnCCNodeAddChild>("?addChild@CCNode@cocos2d@@UEAAXPEAV12@@Z");
    auto setPosFn          = sym<FnCCNodeSetPosition>("?setPosition@CCNode@cocos2d@@UEAAXAEBVCCPoint@2@@Z");
    auto setScaleFn        = sym<FnCCNodeSetScale>("?setScale@CCNode@cocos2d@@UEAAXM@Z");
    auto setZFn            = sym<FnCCNodeSetZOrder>("?setZOrder@CCNode@cocos2d@@UEAAXH@Z");
    auto sharedDirectorFn  = sym<FnCCDirectorShared>("?sharedDirector@CCDirector@cocos2d@@SAPEAV12@XZ");
    auto getWinSizeFn      = sym<FnCCDirectorGetSize>("?getWinSize@CCDirector@cocos2d@@QEBA?AVCCSize@2@XZ");

    if (!spriteCreateFn || !menuItemCreateFn || !menuCreateEmptyFn ||
        !addChildFn || !setPosFn || !sharedDirectorFn || !getWinSizeFn) {
        logMsg("[PrismUI] Missing cocos2d symbols for button injection");
        return true;
    }

    void* director = sharedDirectorFn();
    if (!director) return true;
    CCSize winSize = getWinSizeFn(director);

    // Create button sprite using GJ_button_01.png
    void* normalSprite   = spriteCreateFn("GJ_button_01.png");
    void* selectedSprite = spriteCreateFn("GJ_button_01.png");
    if (!normalSprite || !selectedSprite) {
        logMsg("[PrismUI] Could not load GJ_button_01.png");
        return true;
    }

    // Scale selected to 90% to give press feedback
    if (setScaleFn)
        setScaleFn(selectedSprite, 0.9f);

    // Create menu item
    void* menuItem = menuItemCreateFn(normalSprite, selectedSprite, nullptr,
        reinterpret_cast<void(*)(void*, void*)>(onPrismButtonClicked));
    if (!menuItem) {
        logMsg("[PrismUI] Failed to create menu item");
        return true;
    }

    // Create menu
    void* menu = menuCreateEmptyFn();
    if (!menu) return true;

    addChildFn(menu, menuItem);

    // Position button in bottom-right corner
    CCPoint menuPos = { winSize.width - 50.0f, 50.0f };
    setPosFn(menu, menuPos);
    setPosFn(menuItem, { 0.0f, 0.0f });

    if (setScaleFn) setScaleFn(menu, 0.75f);
    if (setZFn) setZFn(menu, 100);

    // Add to MenuLayer (self is a CCNode)
    addChildFn(self, menu);

    logMsg("[PrismUI] Mod Library button injected at (%.0f, %.0f)", menuPos.x, menuPos.y);
    return true;
}

// ─── Installation ──────────────────────────────────────────────────────────

void installModMenuHook() {
    // Get GeometryDash.exe base address
    HMODULE gdExe = GetModuleHandleA(nullptr);
    if (!gdExe) {
        // Try by name
        gdExe = GetModuleHandleA("GeometryDash.exe");
    }
    if (!gdExe) {
        logMsg("[PrismUI] Could not get GeometryDash.exe handle");
        return;
    }

    // Apply known offset for MenuLayer::init (GD 2.206 Win64)
    auto target = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(gdExe) + k_MenuLayerInitOffset);

    Hook hook;
    bool ok = HookEngine::get().installHook(target, reinterpret_cast<void*>(menuLayerInitDetour), &hook);
    if (ok) {
        g_originalMenuLayerInit = reinterpret_cast<MenuLayerInitFn>(hook.trampoline);
        logMsg("[PrismUI] MenuLayer::init hook installed at %p", target);
    } else {
        logMsg("[PrismUI] Failed to install MenuLayer::init hook at %p", target);
    }
}

} // namespace prism

#endif // _WIN32
