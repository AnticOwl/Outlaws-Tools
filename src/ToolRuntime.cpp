#include "ToolRuntime.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>

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
    log.flush();
}

void writeStartupDiagnostics(const ToolRuntime& runtime) {
    std::ofstream log(logPath(), std::ios::out | std::ios::app);
    if (!log) return;

    const auto base = runtime.bindings.moduleBase;
    log << std::hex << std::uppercase << std::setfill('0');
    log << "moduleBase=0x" << base << "\n";
    log << "postHookTarget=0x" << (base ? base + PostProcessSystem::FloatSetterRva : 0) << "\n";
    log << "postEnvironmentSystem=0x" << runtime.post.environmentSystem() << "\n";
    log << std::dec;
    log << "postReady=" << (runtime.post.ready() ? 1 : 0) << "\n";
    log << "startupComplete=1\n";
    log.flush();
}
}

ToolRuntime& ToolRuntime::instance() {
    static ToolRuntime g;
    return g;
}

bool ToolRuntime::start() {
    if (m_running.exchange(true)) return true;

    writeLine("Outlaws Tools bootstrap entered", true);

    if (!bindings.initialize()) {
        writeLine("bindings.initialize failed");
        m_running = false;
        return false;
    }

    writeLine("bindings initialized");

    if (post.initialize(bindings.moduleBase)) {
        writeLine("post hook initialized");
    } else {
        writeLine("post hook initialization failed");
    }

    writeStartupDiagnostics(*this);
    return true;
}

void ToolRuntime::stop() {
    if (!m_running.exchange(false)) return;
    post.shutdown();
    bindings.shutdown();
}
}
