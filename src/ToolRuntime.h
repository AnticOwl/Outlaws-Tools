#pragma once
#include "GameBindings.h"
#include "LightSystem.h"
#include "EnvironmentSystem.h"
#include "EnvironmentInspector.h"
#include "Dx12Overlay.h"
#include "PostProcessSystem.h"
#include <atomic>

namespace outlaws {
class ToolRuntime {
public:
    static ToolRuntime& instance();
    bool start();
    void stop();
    bool running() const { return m_running.load(); }

    GameBindings bindings;
    LightSystem lights;
    EnvironmentSystem environment;
    EnvironmentInspector inspector;
    Dx12Overlay overlay;
    PostProcessSystem post;
private:
    std::atomic_bool m_running{};
};
}
