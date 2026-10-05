#include "GameBindings.h"
#include <Windows.h>

namespace outlaws {
bool GameBindings::initialize() {
    moduleBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    initialized = moduleBase != 0;
    return initialized;
}

void GameBindings::shutdown() {
    initialized = false;
    moduleBase = 0;
}
}
