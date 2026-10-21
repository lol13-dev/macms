// core-engine/src/ipc/websocket_server.cpp
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
    server_ = std::make_unique<ix::WebSocketServer>(port, "127.0.0.1");
    
    // Explicitly bind the message listener to the connection event
    server_->setOnConnectionCallback([this](std::weak_ptr<ix::WebSocket> webSocket, 
                                            std::shared_ptr<ix::ConnectionState> connectionState) {
        auto ws = webSocket.lock();
        if (ws) {
            std::cout << "\n[MacMSD] >>> React UI Connected! (" << connectionState->getRemoteIp() << ")\n";
            
            ws->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Message) {
                    std::cout << "[MacMSD] >>> INCOMING CLICK FROM UI: " << msg->str << "\n";
                    handleMessage(msg);
                }
                else if (msg->type == ix::WebSocketMessageType::Error) {
                    std::cerr << "[MacMSD] WebSocket Error: " << msg->errorInfo.reason << "\n";
                }
            });
        }
    });

    auto res = server_->listen();
    if (!res.first) {
        std::cerr << "[MacMSD] FATAL: Failed to bind to port " << port << " - " << res.second << "\n";
        return false;
    }

    server_->start();
    isRunning_ = true;
    std::cout << "[MacMSD] IPC Server running on ws://127.0.0.1:" << port << "\n";
    return true;
}

void WebSocketDaemon::stop() {
    if (isRunning_) {
        if (server_) server_->stop();
        ix::uninitNetSystem();
        isRunning_ = false;
    }
}

void WebSocketDaemon::broadcastMetrics() {
    if (!isRunning_ || !server_ || server_->getClients().empty()) return;

    auto thermals = thermal_.getSystemThermals();
    if (!thermals) return;

    nlohmann::json payload = {
        {"type", "metrics"},
        {"data", {
            {"cpuTemp", thermals->cpuTemp},
            {"gpuTemp", thermals->gpuTemp},
            {"fanLeftRpm", thermals->fanLeftRpm},
            {"fanRightRpm", thermals->fanRightRpm},
            {"profile", profile_.getProfileName()},
            {"sessionActive", tracker_.isSessionActive()},
            {"sessionDurationMin", tracker_.getActiveDurationMinutes()},
            {"onACPower", profile_.isOnACPower()}
        }}
    };

    std::string msg = payload.dump();
    for (auto client : server_->getClients()) {
        client->sendText(msg);
    }
}

void WebSocketDaemon::handleMessage(const ix::WebSocketMessagePtr& msg) {
    try {
        auto json = nlohmann::json::parse(msg->str);
        if (json["type"] == "setProfile") {
            std::string p = json["profile"];
            std::cout << "[MacMSD] >>> Executing hardware profile switch to: " << p << "\n";
            
            if (p == "Silent") profile_.setProfile(system::PowerProfile::Silent);
            else if (p == "Normal") profile_.setProfile(system::PowerProfile::Normal);
            else if (p == "Performance") profile_.setProfile(system::PowerProfile::Performance);
            else if (p == "Maximum") profile_.setProfile(system::PowerProfile::Maximum); // [!] ,true deleted.
        }
    } catch (const std::exception& e) {
        std::cerr << "[MacMSD] >>> FATAL JSON ERROR: " << e.what() << "\n";
    }
}

} // end of namespace macms::ipc