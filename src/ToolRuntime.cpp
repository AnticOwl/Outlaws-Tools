#include "ToolRuntime.h"
namespace outlaws {
ToolRuntime& ToolRuntime::instance() { static ToolRuntime g; return g; }
bool ToolRuntime::start() {
    if (m_running.exchange(true)) return true;
    if (!bindings.initialize()) { m_running = false; return false; }
    return true;
}
void ToolRuntime::stop() {
    if (!m_running.exchange(false)) return;
    bindings.shutdown();
}
}
