# PrismLoader

Mod loader dla Geometry Dash na Linuxa (Steam/Proton).

## Stack

- **Loader**: C++23, cross-kompilowany MinGW (`x86_64-w64-mingw32-g++`) jako Windows DLL
- **Launcher** (opcjonalnie): C++23, natywny Linux CLI
- **SDK**: Headery C++ dla modderów
- **Build**: CMake (Linux) + ręczna kompilacja MinGW (proton)

## Architektura

### Proxy DLL (`loader/src/dllmain.cpp`)
Loader wstrzykiwany przez podmianę DLL który GD faktycznie ładuje. Obecnie target: `pthreadVCE3.dll`.

Przepływ:
1. GD ładuje `pthreadVCE3.dll` → nasz proxy
2. `DllMain` ładuje oryginał (`pthreadVCE3_o.dll`)
3. Wszystkie eksporty forwardowane do oryginału (`.def` file)
4. Osobny wątek inicjalizuje loadera po 500ms

### Inicjalizacja (`loader/src/loader.cpp`)
1. `prismInit()` → `PatternScanner::load()` (szuka GD/cocos2d w pamięci)
2. `detectGdInit()` → hookuje `CCEGLView::swapBuffers`
3. Przy pierwszej klatce → `Loader::loadMods()` → `ModLoader::discoverMods()`

### Hook Engine (`loader/src/hookengine.cpp`)
- `VirtualProtect` + detour (jmp [rip+0])
- `VirtualAlloc` na trampoliny
- 14-bajtowe detoury

### Mod Loader (`loader/src/modloader.cpp`)
- Skanuje `%USERPROFILE%\.prismloader\mods\*.dll` (flat layout, nie subdirs)
- `LoadLibrary`/`GetProcAddress("prismModInit")`
- Parsuje `mod.json` (JSON minimalny, bez zależności)
- Po załadowaniu wywołuje `entry.instance->onLoad()` — **wymagane** dla mod lifecycle

### UI: Mod Library button (`loader/src/ui/modmenu.cpp`)
- Hook `MenuLayer::init` pod offsetem `0x333a90` (GD 2.206 x64)
- Wstrzykuje przycisk "Mods" w prawym dolnym rogu (GJ_button_01.png)
- Po kliknięciu otwiera popup z listą załadowanych modów
- Popup: ciemny panel CCLayerColor + CCLabelBMFont lista + GJ_closeBtn_001.png close
- z-order 9999 żeby siedzieć nad wszystkim
- Wszystkie 15 symboli cocos2d (sharedDirector, getWinSize, sprite/menu/menuItem/label/layer create, addChild/Z, setPosition/Scale/ZOrder/Visible, removeFromParent, getRunningScene) resolve'owane przez GetProcAddress z libcocos2d.dll

## Instalacja

```bash
./scripts/install-proton.sh [/path/to/Geometry/Dash]
```

Build + backup DLL + instalacja. Uruchom GD normalnie z Steam.

## Building tylko DLL (bez reinstalacji)

```bash
x86_64-w64-mingw32-g++ -std=c++23 -shared \
    -o build-proton/prismproxy.dll \
    loader/src/dllmain.cpp loader/src/hookengine.cpp \
    loader/src/patternscan.cpp loader/src/modloader.cpp \
    loader/src/gdinit.cpp loader/src/loader.cpp \
    loader/src/ui/modmenu.cpp loader/src/log.cpp \
    -Iloader/include -Isdk/include -lpsapi \
    -static-libgcc -static-libstdc++ \
    build-proton/prismproxy.def \
    -Wl,--enable-stdcall-fixup
```

## Mod layout (flat!)

Mods NIE są w subdirs. Wszystkie `*.dll` + `mod.json` w jednym katalogu:

```
%USERPROFILE%/.prismloader/mods/
├── example_mod.dll
└── mod.json          # opisuje example_mod.dll
```

`discoverMods()` robi flat `*.dll` scan katalogu — subdirs są ignorowane.

## Budowanie modów

```cmake
if(MINGW)
    add_library(example_mod SHARED src/main.cpp)
    # Self-contained: statyczne linkowanie MinGW runtime
    # (inaczej LoadLibraryA rzuci 126 — brak libgcc_s_seh-1.dll)
    target_link_options(example_mod PRIVATE
        -static -static-libgcc -static-libstdc++
    )
else()
    add_library(example_mod MODULE src/main.cpp)
endif()
```

## Konwencje

- C++23, bez exceptions, bez RTTI
- Windows API zamiast std::filesystem (kompatybilność MinGW)
- Logowanie przez `prism::logMsg()` (zapis do pliku w %TEMP% → `prismloader.log`)
- Wszystkie funkcje loadera w `namespace prism`
- Mod `PRISM_MOD(modClass)` makro eksportujące `prismModInit`/`prismModExit`
- Brak zależności zewnętrznych (poza WinAPI/psapi)
- **`PRISM_HOOK` macro**: `prism::hook((void*)addr, ...)` — przekazujemy adres, nie dereferujemy (`(void*)*addr` jest bugiem)

## Debug

Logi w Proton C: drive:
```bash
find ~/.steam/steam/steamapps/compatdata/322170 -name prismloader.log
tail -f /home/szym/.local/share/Steam/steamapps/compatdata/322170/pfx/drive_c/users/steamuser/AppData/Local/Temp/prismloader.log
```

Kluczowe logi do szukania:
- `[Prism] GD base: ...` — pattern scan zaladowal
- `[Prism] MenuLayer::init hook installed at ...` — hook aktywny
- `[PrismUI] Mod Library button injected at ...` — przycisk dziala
- `[Prism] Loaded mod: <id>` — mod zaladowany + onLoad fired

## Status (2026-06-06)

- ✅ Proxy DLL wstrzykuje się, ładuje oryginalne pthreadVCE3 eksporty
- ✅ Pattern scanner znajduje GD base
- ✅ Hook engine działa (14-byte detoury z trampolinami)
- ✅ Mod loader: flat scan, LoadLibrary, onLoad fire
- ✅ Mod SDK działa end-to-end: onLoad message w logu po launch
- ✅ Mod menu button: 15 cocos2d symboli verified, detour target 0x333a90, trampoline w DLL
- ⚠️ Live test: GD crashuje PRZED MenuLayer::init z `CAPIJobRequestUserStats - Server response failed 2` (Steam API/Proton runtime, nie nasz bug) — przycisk nie był jeszcze wizualnie potwierdzony w grze

## Struktura

```
PrismLoader/
├── loader/              # libprismloader.so (Linux) + DLL (Proton)
│   ├── src/             # dllmain, hookengine, patternscan, modloader, gdinit, log
│   │   └── ui/          # modmenu (Mod Library button)
│   └── include/Prism/   # Loader.hpp, HookEngine.hpp, PatternScan.hpp, ModLoader.hpp
├── sdk/include/Prism/   # Publiczne API: Mod.hpp, Hook.hpp, Log.hpp, Event.hpp, Version.hpp
├── mods/example/        # Przykładowy mod (flat layout)
├── scripts/             # install-proton.sh, gendef.sh
├── cmake/               # mingw-w64-x86_64.cmake toolchain
└── AGENTS.md
```
