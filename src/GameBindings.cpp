#include "GameBindings.h"
#include <Windows.h>
#include <cstdio>

namespace outlaws {
bool GameBindings::initialize() {
    moduleBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!moduleBase) {
        initialized = false;
        return false;
    }

    environmentRegistryFound = environmentRegistry.locate(moduleBase);

    char buffer[192]{};
    if (environmentRegistryFound) {
        std::snprintf(buffer, sizeof(buffer), "[OutlawsTools] Env registry owner = 0x%llX\n",
            static_cast<unsigned long long>(environmentRegistry.descriptorOwner()));
    } else {
        std::snprintf(buffer, sizeof(buffer), "[OutlawsTools] Env registry owner not found yet.\n");
    }
    OutputDebugStringA(buffer);

    initialized = true;
    return true;
}

void GameBindings::shutdown() {
    environmentRegistryFound = false;
    environmentRegistry.setDescriptorOwner(0);
    initialized = false;
    moduleBase = 0;
}
}
