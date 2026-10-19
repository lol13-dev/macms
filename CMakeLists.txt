cmake_minimum_required(VERSION 3.20)

# The LANGUAGES keyword is strictly required here to prevent configuration errors.
project(MacManagementSystem VERSION 1.0.0 LANGUAGES CXX OBJCXX)

# Forces CMake to generate compile_commands.json, which prevents false IDE linter errors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enforce strict C++20 standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Target both Intel (x86_64) and Apple Silicon (arm64) natively
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)

# High-Performance Compiler Flags for low latency
set(COMPILER_FLAGS
    "-Wall"
    "-Wextra"
    "-Wpedantic"
    "-O3"
    "-flto"
    "-fvisibility=hidden"
)
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${COMPILER_FLAGS}>")
add_compile_options("$<$<COMPILE_LANGUAGE:OBJCXX>:${COMPILER_FLAGS}>")

# ---------------------------------------------------------
# Dependency Management via FetchContent
# ---------------------------------------------------------
include(FetchContent)

# 1. nlohmann/json for high-performance JSON parsing
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# 2. ixwebsocket for lightweight C++ WebSocket IPC (no TLS needed for localhost)
FetchContent_Declare(ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.4
)
set(USE_TLS OFF CACHE BOOL "Disable TLS for local IPC" FORCE)
FetchContent_MakeAvailable(ixwebsocket)

# ---------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------
# We will uncomment this line exactly when the core-engine folder is ready.
add_subdirectory(core-engine)

# SORRY. THIS IS AI-GENERATED CODE DUE TO I WAS ERASING ALL OF THIS CODE.