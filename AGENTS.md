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
- Skanuje `%USERPROFILE%\.prismloader\mods\*.dll`
- `LoadLibrary`/`GetProcAddress("prismModInit")`
- Parsuje `mod.json` (JSON minimalny, bez zależności)

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

## Debug

Logi w Proton C: drive:
```bash
find ~/.steam/steam/steamapps/compatdata/322170 -name prismloader.log
```

## Struktura

```
PrismLoader/
├── loader/              # libprismloader.so (Linux) + DLL (Proton)
│   ├── src/             # dllmain, hookengine, patternscan, modloader, gdinit, log, ui
│   └── include/Prism/   # Loader.hpp, HookEngine.hpp, PatternScan.hpp, ModLoader.hpp
├── sdk/include/Prism/   # Publiczne API: Mod.hpp, Hook.hpp, Log.hpp, Event.hpp, Version.hpp
├── mods/example/        # Przykładowy mod
├── scripts/             # install-proton.sh, gendef.sh
├── cmake/               # mingw-w64-x86_64.cmake toolchain
└── AGENTS.md
```

## Konwencje

- C++23, bez exceptions, bez RTTI
- Windows API zamiast std::filesystem (kompatybilność MinGW)
- Logowanie przez `prism::logMsg()` (zapis do pliku w %TEMP%)
- Wszystkie funkcje loadera w `namespace prism`
- Mod `PRISM_MOD(modClass)` makro eksportujące `prismModInit`/`prismModExit`
- Brak zależności zewnętrznych (poza WinAPI/psapi)
