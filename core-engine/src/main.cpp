// folder location: core-engine/src/main.cpp

#include "system/smc_controller.hpp"
#include "system/thermal_monitor.hpp"
#include "system/profile_manager.hpp"
#include "system/power_monitor.hpp"
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
    macms::system::PowerMonitor power;
    macms::system::ProfileManager profile(smc, thermal,power);
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
        // [!] Failsafe WARNING: WRAP tick execution in try-catch to PREVENT hardware/IPC exceptions from CRASHING the loop.
        try {
            tracker.update();
            profile.enforceProfileRules();
            wsDaemon.broadcastMetrics();
        } catch (const std::exception& e) {
            // [!] Failsafe Warning: LOG hardware errors cleanly WITHOUT terminating process execution.
            std::cerr << "[MacMSα][!] Warning: Execption caught in main loop tick: " << e.what() << "\n";
        } catch (...) {
            // [!] Failsafe Warning: CATCH unhandled low-level hardware exceptions.
            std::cerr << "[MacMSDα][!] Warning: Unknown hardware caught in main loop tick.\n";
        }

        // [!] Failsafe Warning: Sleep MUST remain outside the try-catch block.
        // IF an EXCEPTION repeats, this guarantees the daemon still sleeps 1 second per cycle,
        // permanently preventing 100% CPU runaway and IDE lagging.
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n[MacMSDα] Shutting down cleanly...\n";

    // [!] Failsafe Warning: Safely restore SMC fans to automatic macOS thermal management 
    // before daemon exit so fans are never left stuck in manual RPM mode.
    profile.setProfile(macms::system::PowerProfile::Normal);
    
    wsDaemon.stop();
    return 0;
}