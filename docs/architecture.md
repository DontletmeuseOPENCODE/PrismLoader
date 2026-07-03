# Architecture

PrismLoader consists of several cooperating layers. The full flow from GD
launch to running mods:

```
GD start
  │
  ▼
loads pthreadVCE3.dll  ←  our proxy (dllmain.cpp)
  │  • loads original pthreadVCE3_o.dll
  │  • forwards exports (.def)
  │  • spawns a thread, 500ms sleep
  ▼
prismInit()  (loader.cpp)
  │  • PatternScanner::load() — find GD/cocos2d in memory
  ▼
detectGdInit()  (gdinit.cpp)
  │  • hook CCEGLView::swapBuffers
  ▼
first frame
  │  • Loader::loadMods() → ModLoader::discoverMods()
  │  • MenuLayer::init hook → "Mods" button (ui/modmenu.cpp)
  ▼
mods running
```

## Proxy DLL (`loader/src/dllmain.cpp`)

The injection mechanism. GD loads `pthreadVCE3.dll` (our proxy) instead of the
original. `DllMain`:

1. Loads the original as `pthreadVCE3_o.dll`.
2. Forwards all pthread exports to the original via `.def` (generated from the
   original's `objdump` output by `scripts/gendef.sh`).
3. Spawns a separate thread that waits **500ms** (let GD finish init) and only
   then calls `prismInit()`.

The proxy additionally exports 4 loader symbols for mods:
`prism_hook`, `prism_unhook`, `prism_resolve_address`, `prism_log`.

## Pattern Scan (`loader/src/patternscan.cpp`)

Scans the process memory for the GD base and cocos2d (`libcocos2d.dll`).
`PatternScanner::load()` returns the base address from which the hook engine
and mod loader compute offsets. Log line: `[Prism] GD base: ...`.

## Hook Engine (`loader/src/hookengine.cpp`)

Inline memory detours:

- `VirtualProtect` → flip target function page to RWX
- 14-byte detour: `jmp [rip+0]` + 8-byte detour address
- `VirtualAlloc` for **trampolines** — original instructions + a return jmp, so
  calling the original still works
- `HookHandle { detour, trampoline, target }` — identifier for unhooking

API exposed to mods: `prism::hook()`, `prism::unhook()`,
`prism::resolveAddress()` (delegated through the proxy exports).

## Mod Loader (`loader/src/modloader.cpp`)

`ModLoader::discoverMods()`:

- **Flat** scan of `%USERPROFILE%/.prismloader/mods/*.dll` (subdirs ignored!)
- `LoadLibraryA` on each DLL
- `GetProcAddress("prismModInit")` → constructs a `prism::Mod` instance
- Parses `mod.json` (minimal JSON, no dependencies)
- Calls `entry.instance->onLoad()` — **required** in the mod lifecycle

Success log: `[Prism] Loaded mod: <id>`.

## Initialization (`loader/src/loader.cpp`)

`prismInit()`:

1. `PatternScanner::load()`
2. `detectGdInit()` → hook `CCEGLView::swapBuffers` (waits for the first frame)
3. On the first frame: `Loader::loadMods()` → `ModLoader::discoverMods()`

## UI: Mod Library button (`loader/src/ui/modmenu.cpp`)

Injects a "Mods" button into GD's main menu:

- Hook `MenuLayer::init` at offset `0x333a90` (GD 2.206 x64)
- Button in the bottom-right corner (`GJ_button_01.png`)
- On click → popup: `CCLayerColor` (dark panel) + `CCLabelBMFont` list of loaded
  mods + `GJ_closeBtn_001.png` to close
- z-order `9999` (above everything)

All **15 cocos2d symbols** (`sharedDirector`, `getWinSize`, create sprite/menu/
menuItem/label/layer, `addChild`/Z, `setPosition`/`Scale`/`ZOrder`/`Visible`,
`removeFromParent`, `getRunningScene`) are resolved dynamically via
`GetProcAddress` from `libcocos2d.dll`.

## SDK (`sdk/include/Prism/`)

The public API for modders (header-only from a mod's perspective):

| File | Contents |
| --- | --- |
| `Mod.hpp` | `prism::Mod` base class, `PRISM_MOD()` macro |
| `Hook.hpp` | `prism::hook()`/`unhook()`, `PRISM_HOOK()` macro |
| `Log.hpp` | `prism::log()`, `PRISM_LOG`/`PRISM_MOD_LOG` macros |
| `Event.hpp` | `prism::Event<Args...>` template |
| `Version.hpp` | loader version, `Version` struct |

Mods talk to the loader through the proxy exports (`getLoaderProc`); they do not
link statically against the loader.

## Directory structure

```
PrismLoader/
├── loader/              # libprismloader.so (Linux) + prismproxy.dll (Proton)
│   ├── src/             # dllmain, hookengine, patternscan, modloader, gdinit, log
│   │   └── ui/          # modmenu (Mod Library button)
│   └── include/Prism/   # Loader.hpp, HookEngine.hpp, PatternScan.hpp, ModLoader.hpp
├── sdk/include/Prism/   # Public API: Mod, Hook, Log, Event, Version
├── mods/example/        # Example mod (flat layout)
├── scripts/             # install-proton.sh, gendef.sh, setup.sh
├── cmake/               # mingw-w64-x86_64.cmake toolchain
└── docs/                # this documentation
```