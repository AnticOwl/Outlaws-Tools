#include "EnvironmentSystem.h"
namespace outlaws {
bool EnvironmentSystem::read(EnvironmentState&) const { return false; }
bool EnvironmentSystem::apply(const EnvironmentState&) { return false; }
}
