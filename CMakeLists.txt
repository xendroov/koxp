cmake_minimum_required(VERSION 3.20)
project(koxp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- MinHook ---
FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(minhook)

# --- ImGui ---
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.0
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)

# --- koxp.dll ---
set(DLL_SOURCES
    src/dllmain.cpp
    src/core/hooks.cpp
    src/game/player.cpp
    src/game/entity.cpp
    src/features/bot.cpp
    src/features/autoheal.cpp
    src/features/autoskill.cpp
    src/network/packet.cpp
    src/ui/d3d9hook.cpp
    src/ui/menu.cpp
    src/core/hotkeys.cpp
    ${IMGUI_SOURCES}
)

add_library(koxp SHARED ${DLL_SOURCES})

target_include_directories(koxp PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${minhook_SOURCE_DIR}/include
)

target_compile_definitions(koxp PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(koxp PRIVATE minhook ws2_32 winmm d3d9)

# --- injector.exe ---
add_executable(injector injector/main.cpp)
target_compile_definitions(injector PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
