#include "LightSystem.h"

namespace outlaws {
std::uintptr_t LightSystem::readTypeGlobal(std::uintptr_t rva) const noexcept {
    if (!moduleBase_ || !rva) return 0;
    return *reinterpret_cast<const std::uintptr_t*>(moduleBase_ + rva);
}

std::uintptr_t LightSystem::pointTypeDescriptor() const noexcept { return readTypeGlobal(PointTypeGlobalRva); }
std::uintptr_t LightSystem::spotTypeDescriptor() const noexcept { return readTypeGlobal(SpotTypeGlobalRva); }
std::uintptr_t LightSystem::tubeTypeDescriptor() const noexcept { return readTypeGlobal(TubeTypeGlobalRva); }
std::uintptr_t LightSystem::baseTypeDescriptor() const noexcept { return readTypeGlobal(BaseTypeGlobalRva); }
std::uintptr_t LightSystem::areaTypeDescriptor() const noexcept { return readTypeGlobal(AreaTypeGlobalRva); }

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
