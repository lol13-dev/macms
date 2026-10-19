// folder location: core-engine/include/ipc/websocket_server.hpp
#pragma once

#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>
#include "system/thermal_monitor.hpp"
#include "system/profile_manager.hpp"
#include "timer/session_tracker.hpp"
#include <atomic>
#include <memory>

namespace macms::ipc {

class WebSocketDaemon {
public:
    // TAKES references to my CORE MANAGERS to READ data AND APPLY UI commands.
    WebSocketDaemon(system::ThermalMonitor& thermal, system::ProfileManager& profile, timer::SessionTracker& tracker);

    // Destructor.
    ~WebSocketDaemon();

    WebSocketDaemon(const WebSocketDaemon&) = delete;
    WebSocketDaemon& operator=(const WebSocketDaemon&) = delete;

    // STARTS the local websocket SERVER (default port 8080)
    bool start(int port = 8080);
    void stop();

    // SCRAPES data from the monitors and broadcasts JSO to the React Frontend.
    void broadcastMetrics();

private:
    system::ThermalMonitor& thermal_;
    system::ProfileManager& profile_;
    timer::SessionTracker& tracker_;
    
    ix::WebSocketServer server_;
    std::atomic<bool> isRunning_{false};

    // CALLBACK to handle incoming profile changes from the React UI.
    void handleMessage(std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg);
};

} // end of namespace macms::ipc