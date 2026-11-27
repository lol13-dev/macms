cmake_minimum_required(VERSION 3.20)

# Declare CXX only so Windows CMake does not fail searching for an Objective-C++ compiler
project(MacManagementSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enforce strict C++20 standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------
# Platform-Specific Configurations (Must run BEFORE FetchContent)
# ---------------------------------------------------------
if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architecture for Intel Mac" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)

    set(USE_TLS ON CACHE BOOL "Enable Apple Security TLS" FORCE)
    find_library(IOKIT_FW IOKit REQUIRED)
    find_library(FOUNDATION_FW Foundation REQUIRED)
    find_library(COREFOUNDATION_FW CoreFoundation REQUIRED)
    set(PLATFORM_LIBS ${IOKIT_FW} ${FOUNDATION_FW} ${COREFOUNDATION_FW} "-framework Security")

    set(COMPILER_FLAGS "-Wall" "-Wextra" "-Wpedantic" "-O3" "-flto" "-fvisibility=hidden")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${COMPILER_FLAGS}>")
    add_compile_options("$<$<COMPILE_LANGUAGE:OBJCXX>:${COMPILER_FLAGS}>")

elseif(WIN32)
    set(USE_TLS OFF CACHE BOOL "Disable TLS for local Windows IPC" FORCE)
    set(USE_ZLIB OFF CACHE BOOL "Disable zlib dependency on Windows" FORCE)
    set(PLATFORM_LIBS ws2_32 crypt32 bcrypt)

    if(MINGW)
        add_compile_options(-Wall -Wextra -O3)
    endif()
endif()

# ---------------------------------------------------------
# Dependency Management via FetchContent
# ---------------------------------------------------------
include(FetchContent)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.4
)
FetchContent_MakeAvailable(ixwebsocket)

# ---------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------
add_subdirectory(core-engine)