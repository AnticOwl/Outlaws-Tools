cmake_minimum_required(VERSION 3.20)
project(OutlawsTools LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.5-docking.zip
)
FetchContent_MakeAvailable(imgui)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(OutlawsTools SHARED
    src/dllmain.cpp
    src/ToolRuntime.cpp
    src/GameBindings.cpp
    src/PatternScanner.cpp
    src/EnvironmentRegistry.cpp
    src/LightSystem.cpp
    src/EnvironmentSystem.cpp
    src/EnvironmentInspector.cpp
    src/PostProcessSystem.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(OutlawsTools PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(OutlawsTools PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    target_compile_options(OutlawsTools PRIVATE /W4 /permissive-)
endif()

target_link_libraries(OutlawsTools PRIVATE user32 gdi32 d3d11 dxgi dwmapi)
