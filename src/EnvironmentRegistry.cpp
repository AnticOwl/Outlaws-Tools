#include "EnvironmentRegistry.h"
#include <Windows.h>
#include <cstring>

namespace outlaws {
namespace {
bool readable(std::uintptr_t address, std::size_t size) noexcept {
    if (!address || !size) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const DWORD p = mbi.Protect & 0xFF;
    const bool canRead = p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
                         p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
    if (!canRead) return false;
    const auto regionBegin = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
    const auto regionEnd = regionBegin + mbi.RegionSize;
    return address >= regionBegin && address + size >= address && address + size <= regionEnd;
}
}

const EnvDescriptor* EnvironmentRegistry::descriptor(const KnownEnvDescriptor& known) const noexcept {
    if (!owner_) return nullptr;
    return reinterpret_cast<const EnvDescriptor*>(owner_ + known.descriptorOffset);
}

bool EnvironmentRegistry::validate(const KnownEnvDescriptor& known) const noexcept {
    const auto* d = descriptor(known);
    if (!d || !readable(reinterpret_cast<std::uintptr_t>(d), sizeof(*d)) || d->id != known.id || !d->name) return false;
    if (!readable(reinterpret_cast<std::uintptr_t>(d->name), known.name.size() + 1)) return false;
    return std::strcmp(d->name, known.name.data()) == 0;
}

bool EnvironmentRegistry::locate(std::uintptr_t moduleBase) noexcept {
    owner_ = 0;
    if (!moduleBase) return false;

    const auto exposureName = moduleBase + ExposureTarget.nameRva;
    const auto rainName = moduleBase + GameplayRain.nameRva;
    const auto fogName = moduleBase + GameplayFog.nameRva;
    const auto exposureNameOffset = ExposureTarget.descriptorOffset + offsetof(EnvDescriptor, name);

    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    auto cursor = reinterpret_cast<std::uintptr_t>(si.lpMinimumApplicationAddress);
    const auto maximum = reinterpret_cast<std::uintptr_t>(si.lpMaximumApplicationAddress);

    while (cursor < maximum) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<const void*>(cursor), &mbi, sizeof(mbi))) break;
        const auto regionBegin = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto regionEnd = regionBegin + mbi.RegionSize;

        const DWORD p = mbi.Protect & 0xFF;
        const bool canRead = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
            (p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
             p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY);

        if (canRead && mbi.RegionSize >= sizeof(std::uintptr_t)) {
            auto scan = (regionBegin + alignof(std::uintptr_t) - 1) & ~(alignof(std::uintptr_t) - 1);
            for (; scan + sizeof(std::uintptr_t) <= regionEnd; scan += sizeof(std::uintptr_t)) {
                if (*reinterpret_cast<const std::uintptr_t*>(scan) != exposureName || scan < exposureNameOffset) continue;

                const auto candidate = scan - exposureNameOffset;
                const auto* exposure = reinterpret_cast<const EnvDescriptor*>(candidate + ExposureTarget.descriptorOffset);
                const auto* rain = reinterpret_cast<const EnvDescriptor*>(candidate + GameplayRain.descriptorOffset);
                const auto* fog = reinterpret_cast<const EnvDescriptor*>(candidate + GameplayFog.descriptorOffset);
                if (!readable(reinterpret_cast<std::uintptr_t>(exposure), sizeof(*exposure)) ||
                    !readable(reinterpret_cast<std::uintptr_t>(rain), sizeof(*rain)) ||
                    !readable(reinterpret_cast<std::uintptr_t>(fog), sizeof(*fog))) continue;

                if (exposure->id == ExposureTarget.id && exposure->name == exposureName &&
                    rain->id == GameplayRain.id && rain->name == rainName &&
                    fog->id == GameplayFog.id && fog->name == fogName) {
                    owner_ = candidate;
                    return true;
                }
            }
        }

        if (regionEnd <= cursor) break;
        cursor = regionEnd;
    }
    return false;
}
}
