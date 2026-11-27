set(TARGET_NAME MacMSD)

# Build the core engine as a console executable named MacMSD.
# The following source files contain the main entry point and all engine subsystems.
add_executable(${TARGET_NAME}
    src/main.cpp
    src/system/smc_controller.cpp
    src/system/thermal_monitor.cpp
    src/system/profile_manager.cpp
    src/system/power_monitor.cpp
    src/timer/session_tracker.cpp
    src/ipc/websocket_server.cpp
)

# Allow source files to include headers using paths relative to core-engine/include.
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link JSON parsing, WebSocket support, and libraries selected by the root CMakeLists.txt
# for the current platform (for example, Windows sockets and Apple system frameworks).
target_link_libraries(${TARGET_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    ixwebsocket
    ${PLATFORM_LIBS}
)