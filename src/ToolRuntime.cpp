#include "ToolRuntime.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <cstdio>

namespace outlaws {
namespace {
std::filesystem::path logPath() {
    wchar_t buffer[MAX_PATH]{};
    const auto len = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (!len || len >= std::size(buffer)) return L"OutlawsTools.log";
    std::filesystem::path p(buffer);
    p.replace_filename(L"OutlawsTools.log");
    return p;
}

void writeLine(const char* line, bool truncate = false) {
    std::ofstream log(logPath(), truncate ? (std::ios::out | std::ios::trunc) : (std::ios::out | std::ios::app));
    if (!log) return;
    log << line << "\n";
}

void writeStartupDiagnostics(const ToolRuntime& runtime) {
    std::ofstream log(logPath(), std::ios::out | std::ios::app);
    if (!log) return;

    const auto base = runtime.bindings.moduleBase;
    log << std::hex << std::uppercase << std::setfill('0');
    log << "moduleBase=0x" << base << "\n";
    log << "envRegistryFound=" << std::dec << (runtime.bindings.environmentRegistryFound ? 1 : 0) << "\n";
    log << std::hex;
    log << "envRegistryOwner=0x" << runtime.bindings.environmentRegistry.descriptorOwner() << "\n";
    log << "pointType=0x" << runtime.lights.pointTypeDescriptor() << "\n";
    log << "spotType=0x" << runtime.lights.spotTypeDescriptor() << "\n";
    log << "tubeType=0x" << runtime.lights.tubeTypeDescriptor() << "\n";
    log << "areaType=0x" << runtime.lights.areaTypeDescriptor() << "\n";
    log << "baseLightType=0x" << runtime.lights.baseTypeDescriptor() << "\n";
    if (base) {
        log << "spotNodeExecute=0x" << (base + LightSystem::SpotNodeExecuteRva) << "\n";
        log << "genericRendererCreate=0x" << (base + LightSystem::GenericRendererCreateRva) << "\n";
        log << "existingManagerCreate=0x" << (base + LightSystem::ExistingManagerCreateRva) << "\n";
    }
    log << "startupComplete=1\n";
}
}

ToolRuntime& ToolRuntime::instance() { static ToolRuntime g; return g; }

bool ToolRuntime::start() {
    if (m_running.exchange(true)) return true;

    writeLine("Outlaws Tools bootstrap entered", true);

    if (!bindings.initialize()) {
        writeLine("bindings.initialize failed");
        m_running = false;
        return false;
    }

    lights.bind(bindings.moduleBase);
    writeLine("bindings initialized");

    // Early injection is supported: if Snowdrop has not finished constructing the
    // Environment descriptor registry yet, retry a few times on this worker thread.
    if (!bindings.environmentRegistryFound) {
        for (int attempt = 1; attempt <= 3 && !bindings.environmentRegistryFound; ++attempt) {
            char line[64]{};
            std::snprintf(line, sizeof(line), "env registry retry %d/3", attempt);
            writeLine(line);
            Sleep(2000);
            bindings.environmentRegistryFound = bindings.environmentRegistry.locate(bindings.moduleBase);
        }
    }

    writeStartupDiagnostics(*this);
    return true;
}

void ToolRuntime::stop() {
    if (!m_running.exchange(false)) return;
    lights.bind(0);
    bindings.shutdown();
}
}
