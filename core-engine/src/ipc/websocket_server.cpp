// folder location: core-engine/src/ipc/websocket_server.cpp
#include "ipc/websocket_server.hpp"
#include <iostream>

namespace macms::ipc {

WebSocketDaemon::WebSocketDaemon(system::ThermalMonitor& thermal, 
                                 system::ProfileManager& profile, 
                                 timer::SessionTracker& tracker)
    : thermal_(thermal), profile_(profile), tracker_(tracker) {}

WebSocketDaemon::~WebSocketDaemon() {
    stop();
}

bool WebSocketDaemon::start(int port) {
    if (isRunning_) return true;

    ix::initNetSystem();
    
    // Bind directly to localhost so external network devices cannot manipulate your hardware
    server_ = ix::WebSocketServer(port, "127.0.0.1");
    
    server_.setOnConnectionCallback([this](std::shared_ptr<ix::ConnectionState> connectionState, 
                                           ix::WebSocket& webSocket, 
                                           const ix::WebSocketMessagePtr& msg) {
        handleMessage(connectionState, webSocket, msg);
    });

    auto res = server_.listen();
    if (!res.first) {
        std::cerr << "[WebSocket] Failed to bind to port " << port << " - " << res.second << "\n";
        return false;
    }

    server_.start();
    isRunning_ = true;
    std::cout << "[WebSocket] IPC Server running on ws://127.0.0.1:" << port << "\n";
    return true;
}

void WebSocketDaemon::stop() {
    if (isRunning_) {
        server_.stop();
        ix::uninitNetSystem();
        isRunning_ = false;
    }
}

void WebSocketDaemon::broadcastMetrics() {
    if (!isRunning_ || server_.getClients().empty()) return;

    auto thermals = thermal_.getSystemThermals();
    if (!thermals) return;

    // Pack all our C++ data into a lightweight JSON payload for React
    nlohmann::json payload = {
        {"type", "metrics"},
        {"data", {
            {"cpuTemp", thermals->cpuTemp},
            {"gpuTemp", thermals->gpuTemp},
            {"fanLeftRpm", thermals->fanLeftRpm},
            {"fanRightRpm", thermals->fanRightRpm},
            {"profile", profile_.getProfileName()},
            {"sessionActive", tracker_.isSessionActive()},
            {"sessionDurationMin", tracker_.getActiveDurationMinutes()}
        }}
    };

    std::string msg = payload.dump();
    for (auto client : server_.getClients()) {
        client->sendText(msg);
    }
}

void WebSocketDaemon::handleMessage(std::shared_ptr<ix::ConnectionState> /*connectionState*/, 
                                    ix::WebSocket& webSocket, 
                                    const ix::WebSocketMessagePtr& msg) {
    if (msg->type == ix::WebSocketMessageType::Message) {
        try {
            auto json = nlohmann::json::parse(msg->str);
            if (json["type"] == "setProfile") {
                std::string p = json["profile"];
                if (p == "Silent") profile_.setProfile(system::PowerProfile::Silent);
                else if (p == "Normal") profile_.setProfile(system::PowerProfile::Normal);
                else if (p == "Performance") profile_.setProfile(system::PowerProfile::Performance);
                // We assume UI has logic to check if AC is connected before sending Maximum
                else if (p == "Maximum") profile_.setProfile(system::PowerProfile::Maximum, true); 
            }
        } catch (...) {
            // Silently swallow bad JSON so a UI glitch cannot crash the system daemon
        }
    }
}

} // end of namespace macms::ipc