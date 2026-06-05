#include <Prism/PatternScan.hpp>
#include <cstdio>
#include <cstring>
#include <windows.h>
#include <psapi.h>

namespace prism {

static FILE* logFile() {
    FILE* f = nullptr;
    fopen_s(&f, "C:\\prismloader.log", "a");
    return f;
}

PatternScanner& PatternScanner::get() {
    static PatternScanner instance;
    return instance;
}

bool PatternScanner::load() {
    if (baseAddr != 0) return true;

    if (!findGdBinary())
        return false;

    auto log = logFile();
    if (log) {
        fprintf(log, "[Prism] GD base: 0x%llx, code end: 0x%llx\n",
                (unsigned long long)baseAddr, (unsigned long long)codeEnd);
        fclose(log);
    }

    return true;
}

void PatternScanner::unload() {
    regions.clear();
    baseAddr = 0;
    codeEnd = 0;
}

bool PatternScanner::findGdBinary() {
    HANDLE process = GetCurrentProcess();
    HMODULE modules[256];
    DWORD needed;

    if (!EnumProcessModules(process, modules, sizeof(modules), &needed))
        return false;

    DWORD count = needed / sizeof(HMODULE);

    for (DWORD i = 0; i < count; ++i) {
        char name[MAX_PATH];
        if (!GetModuleBaseNameA(process, modules[i], name, MAX_PATH))
            continue;

        char lower[260];
        for (int j = 0; name[j]; ++j)
            lower[j] = (char)tolower(name[j]);
        lower[strlen(name)] = '\0';

        if (strstr(lower, "geometrydash") || strstr(lower, "libcocos2d")) {
            MODULEINFO info;
            if (GetModuleInformation(process, modules[i], &info, sizeof(info))) {
                baseAddr = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
                codeEnd = baseAddr + info.SizeOfImage;
                return true;
            }
        }
    }

    for (DWORD i = 0; i < count; ++i) {
        MODULEINFO info;
        if (GetModuleInformation(process, modules[i], &info, sizeof(info))) {
            char name[MAX_PATH];
            if (GetModuleBaseNameA(process, modules[i], name, MAX_PATH)) {
                if (strstr(name, ".exe")) {
                    baseAddr = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
                    codeEnd = baseAddr + info.SizeOfImage;
                    return true;
                }
            }
        }
    }

    return false;
}

bool PatternScanner::isIdSym(const char* name) {
    return name && (
        strstr(name, "cocos2d") ||
        strstr(name, "GeometryDash") ||
        strstr(name, "gd")
    );
}

std::optional<uintptr_t> PatternScanner::resolveSymbol(const char* symbol) {
    if (!load()) return std::nullopt;

    HANDLE process = GetCurrentProcess();
    HMODULE modules[256];
    DWORD needed;

    if (!EnumProcessModules(process, modules, sizeof(modules), &needed))
        return std::nullopt;

    DWORD count = needed / sizeof(HMODULE);

    for (DWORD i = 0; i < count; ++i) {
        FARPROC addr = GetProcAddress(modules[i], symbol);
        if (addr)
            return reinterpret_cast<uintptr_t>(addr);
    }

    return std::nullopt;
}

std::optional<uintptr_t> PatternScanner::findPattern(
    std::string_view pattern,
    uintptr_t start,
    uintptr_t end)
{
    if (!load()) return std::nullopt;

    if (start == 0) start = baseAddr;
    if (end == 0)   end = codeEnd;

    size_t patternLen = pattern.length();
    if (patternLen == 0) return std::nullopt;

    const unsigned char* code = reinterpret_cast<const unsigned char*>(start);
    size_t size = end - start;

    for (size_t i = 0; i < size - patternLen + 1; ++i) {
        bool found = true;
        for (size_t j = 0; j < patternLen; ++j) {
            if (pattern[j] != '?' && code[i + j] != static_cast<unsigned char>(pattern[j])) {
                found = false;
                break;
            }
        }
        if (found)
            return start + i;
    }

    return std::nullopt;
}

} // namespace prism
