cmake_minimum_required(VERSION 3.20)
project(koxp CXX)

set(CMAKE_CXX_STANDARD 17)

# 32-bit zorunlu (KO x86)
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "32-bit build gerekli: cmake -A Win32")
endif()

# koxp.dll — oyun içine inject edilir
add_library(koxp SHARED
    src/dllmain.cpp
    src/game/player.cpp
)
target_include_directories(koxp PRIVATE src)
target_compile_definitions(koxp PRIVATE WIN32_LEAN_AND_MEAN)
