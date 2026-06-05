#include <Prism/Loader.hpp>
#include <Prism/Log.hpp>
#include <cstdio>
#include <dlfcn.h>
#include <unistd.h>

static bool g_prismInitialized = false;

__attribute__((constructor(101))) void prismEntry() {
    auto* env = getenv("PRISM_LOADER");
    if (!env || env[0] != '1') {
        return;
    }

    FILE* log = fopen("/tmp/prismloader.log", "w");
    if (log) {
        fprintf(log, "[Prism] Loader v0.1.0\n");
        fprintf(log, "[Prism] PID: %d\n", getpid());
        fprintf(log, "[Prism] LD_PRELOAD detected, initializing...\n");
        fclose(log);
    }

    if (!prism::Loader::get().init()) {
        if (log) {
            log = fopen("/tmp/prismloader.log", "a");
            fprintf(log, "[Prism] FAILED to initialize loader!\n");
            fclose(log);
        }
        return;
    }

    g_prismInitialized = true;

    log = fopen("/tmp/prismloader.log", "a");
    if (log) {
        fprintf(log, "[Prism] Loader initialized successfully\n");
        fclose(log);
    }
}

__attribute__((destructor)) void prismExit() {
    if (g_prismInitialized) {
        prism::Loader::get().unloadMods();
    }
}
