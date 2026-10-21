# folder location: core-engine/CMakeLists.txt
set(TARGET_NAME MacMSD)

# I LOCATE essential macOS system frameworks for LOW-LEVEL hardware access.
find_library(IOKIT_FW IOKit REQUIRED)
find_library(FOUNDATION_FW Foundation REQUIRED)
find_library(COREFOUNDATION_FW CoreFoundation REQUIRED)

# DEFINE the C++ EXECUTABLE (I list the source files I will BUILD next).
add_executable(${TARGET_NAME}
    src/main.cpp
    src/system/smc_controller.cpp
    src/system/thermal_monitor.cpp
    src/system/profile_manager.cpp
    src/system/power_monitor.cpp
    src/timer/session_tracker.cpp
    src/ipc/websocket_server.cpp
)

# SETS the base include path, so `#include "system/smc_controller.hpp"` WORKS NATIVELY.
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# LINK ALL dependencies to the DAEMON.
target_link_libraries(${TARGET_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    ixwebsocket
    ${IOKIT_FW}
    ${FOUNDATION_FW}
    ${COREFOUNDATION_FW}
)