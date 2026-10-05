cmake_minimum_required(VERSION 3.20)
project(OutlawsTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(OutlawsTools SHARED
    src/dllmain.cpp
    src/ToolRuntime.cpp
    src/GameBindings.cpp
    src/LightSystem.cpp
    src/EnvironmentSystem.cpp
    src/PostProcessSystem.cpp
)

target_include_directories(OutlawsTools PRIVATE src)
target_compile_definitions(OutlawsTools PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    target_compile_options(OutlawsTools PRIVATE /W4 /permissive-)
endif()
