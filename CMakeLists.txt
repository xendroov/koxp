cmake_minimum_required(VERSION 3.20)
project(koxp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
)

add_library(koxp SHARED ${DLL_SOURCES})

target_include_directories(koxp PRIVATE src external/minhook/include)
target_compile_definitions(koxp PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(koxp PRIVATE ws2_32 winmm)

# --- injector.exe ---
add_executable(injector injector/main.cpp)
target_compile_definitions(injector PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
