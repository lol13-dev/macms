// folder location: core-engine/src/main.cpp

#include "system/smc_controller.hpp"
#include "system/thermal_monitor.hpp"
#include "system/profile_manager.hpp"
#include "timer/session_tracker.hpp"
#include "ipc/websocket_server.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

std::atomic<bool> g_running{true};

void signalHandler(int /*signum*/) {
    g_running = false;
}

int main() {
    // CATCH CTRL + C to cleanly SHUTDOWN the daemon.
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "[MacMSDα] Starting Mac Management System Daemon...\n";

    // Step 1: INITIALIZE the Low-Level driver.
    macms::system::SmcController smc;
    if (!smc.connect()) {
        std::cerr << "[MacMSDα] FATAL_ERROR: Failed to connect to AppleSMC! (ARE YOU RUNNING WITH sudo ?)\n";
        return 1;
    }

    // Step 2: INITIALIZE Managers.
    macms::system::ThermalMonitor thermal(smc);
    macms::system::ProfileManager profile(smc, thermal);
    macms::timer::SessionTracker tracker(profile);
    macms::ipc::WebSocketDaemon wsDaemon(thermal, profile, tracker);

    // Step 3: START IPC Bridge.
    if (!wsDaemon.start(8080)) {
        std::cerr << "[MacMSD] FATAL: Failed to start WebSocket daemon on port 8080!\n";
        return 1;
    }
    
    std::cout << "[MacMSDα] Daemon running natively. Press Ctrl + C to TERMINATE.\n";

    // Step 4: ULTRA-LOW Overhead Main Loop (1 Tick Per Second)
    while (g_running) {
        tracker.update();
        profile.enforceProfileRules();
        wsDaemon.broadcastMetrics();
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n[MacMSDα] Shutting down cleanly...\n";
    wsDaemon.stop();
    return 0;
}