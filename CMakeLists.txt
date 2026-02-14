cmake_minimum_required(VERSION 3.16.0)

# uhhhh.. project params!
set(project_name "colored-chat")

# Build For ARM
set(CMAKE_C_COMPILER "armv7l-unknown-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "armv7l-unknown-linux-gnueabihf-g++")
set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_SYSTEM_PROCESSOR "arm")

# Start Project
project(${project_name})

# Include SDK
include("$ENV{HOME}/.minecraft-pi/sdk/lib/minecraft-pi-reborn/sdk/sdk.cmake")

# Build
add_library(${project_name} SHARED src/main.cpp)
target_link_libraries(${project_name} mods reborn-patch symbols)

# Fix binary and put it in the mods folder because i am lazy >:3c
add_custom_command(TARGET ${project_name}
        POST_BUILD
        COMMAND patchelf --remove-rpath lib${project_name}.so
        )
add_custom_command(TARGET ${project_name}
        POST_BUILD
        COMMAND mv lib${project_name}.so ~/.minecraft-pi/mods/lib${project_name}.so
        )
add_custom_command(TARGET ${project_name}
        POST_BUILD
        COMMAND appimage-run ~/Desktop/mcpi/minecraft-pi-reborn-3.0.0-amd64.AppImage 
        )