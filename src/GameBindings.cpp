#include "GameBindings.h"
#include <Windows.h>

namespace outlaws {
bool GameBindings::initialize() {
    moduleBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!moduleBase) {
        initialized = false;
        return false;
    }

    // Do NOT scan the entire process for the Environment registry during bootstrap.
    // That scan can stall for a long time in Outlaws. We first bring up the runtime
    // and light diagnostics; Environment discovery will be triggered separately.
    environmentRegistryFound = false;
    environmentRegistry.setDescriptorOwner(0);

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
