// folder location: core-engine/src/timer/session_tracker.cpp
#include "timer/session_tracker.hpp"
#include <iostream>

namespace macms::timer {

SessionTracker::SessionTracker(system::ProfileManager& profileManager)
    : profileManager_(profileManager) {}

void SessionTracker::update() {
    auto currentProfile = profileManager_.getCurrentProfile();
    
    // I only track durations for heavy-duty profiles
    if (currentProfile == system::PowerProfile::Performance || 
        currentProfile == system::PowerProfile::Maximum) {
        
        if (!tracking_) {
            tracking_ = true;
            sessionStartTime_ = std::chrono::steady_clock::now();
            std::cout << "[SessionTracker] High-performance session started.\n";
        } else {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::hours>(now - sessionStartTime_);
            
            if (duration >= kMaxSessionDuration) {
                std::cout << "[SessionTracker_WARNING] Safety limit reached (3 hours). Auto-shifting to Normal profile.\n";
                profileManager_.setProfile(system::PowerProfile::Normal);
                tracking_ = false;
            }
        }
    } else {
        if (tracking_) {
            std::cout << "[SessionTracker_NOTE] High-performance session ended.\n";
            tracking_ = false;
        }
    }
}

bool SessionTracker::isSessionActive() const {
    return tracking_;
}

int SessionTracker::getActiveDurationMinutes() const {
    if (!tracking_) return 0;
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::minutes>(now - sessionStartTime_).count();
}

} // end of namespace macms::timer