#include "EnvironmentRegistry.h"
#include <cstring>

namespace outlaws {
const EnvDescriptor* EnvironmentRegistry::descriptor(const KnownEnvDescriptor& known) const noexcept {
    if (!owner_) return nullptr;
    return reinterpret_cast<const EnvDescriptor*>(owner_ + known.descriptorOffset);
}

bool EnvironmentRegistry::validate(const KnownEnvDescriptor& known) const noexcept {
    const auto* d = descriptor(known);
    if (!d || d->id != known.id || !d->name) return false;
    return std::strcmp(d->name, known.name.data()) == 0;
}
}
