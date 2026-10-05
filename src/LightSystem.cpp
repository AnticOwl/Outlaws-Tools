#include "LightSystem.h"

namespace outlaws {
bool LightSystem::scan() { return false; }
bool LightSystem::refresh() { return false; }
LightHandle* LightSystem::selected() {
    return m_selected < m_lights.size() ? &m_lights[m_selected] : nullptr;
}
bool LightSystem::moveSelectedToCamera(float) { return false; }
bool LightSystem::cloneSelected() { return false; }
bool LightSystem::spawnPoint() { return false; }
bool LightSystem::spawnSpot() { return false; }
bool LightSystem::removeSelected() { return false; }
}
