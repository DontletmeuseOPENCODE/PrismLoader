#include <windows.h>
#include <cstdio>
#include <cstdarg>

namespace prism {

static const char* getLogPath() {
    static char path[MAX_PATH];
    static bool resolved = false;
    if (!resolved) {
        char tmp[MAX_PATH];
        DWORD len = GetEnvironmentVariableA("TEMP", tmp, MAX_PATH);
        if (len == 0)
            len = GetEnvironmentVariableA("TMP", tmp, MAX_PATH);
        if (len > 0) {
            snprintf(path, MAX_PATH, "%s\\prismloader.log", tmp);
        } else {
            strcpy(path, "C:\\prismloader.log");
        }
        resolved = true;
    }
    return path;
}

void logMsg(const char* fmt, ...) {
    char buf[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    HANDLE hFile = CreateFileA(getLogPath(), GENERIC_WRITE,
        FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        SetFilePointer(hFile, 0, nullptr, FILE_END);
        DWORD written;
        WriteFile(hFile, buf, (DWORD)strlen(buf), &written, nullptr);
        WriteFile(hFile, "\n", 1, &written, nullptr);
        CloseHandle(hFile);
    }
}

} // namespace prism
