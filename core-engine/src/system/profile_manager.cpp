// folder structure: core-engine/src/system/profile_manager.cpp
#include "system/profile_manager.hpp"
#include <iostream>

namespace macms::system {

ProfileManager::ProfileManager(SmcController& smc, ThermalMonitor& thermal)
    : smc_(smc), thermal_(thermal) {}

bool ProfileManager::setProfile(PowerProfile profile, bool isCharging) {
    // Safety lock: Prevent Maximum mode from draining the battery and overheating
    if (profile == PowerProfile::Maximum && !isCharging) {
        std::cerr << "[ProfileManager_WARNING] Cannot engage Maximum profile while on battery power.\n";
        return false;
    }
    
    currentProfile_ = profile;
    std::cout << "[ProfileManager] Profile switched to: " << getProfileName() << "\n";
    
    // Immediately apply the new fan rules
    enforceProfileRules();
    return true;
}

PowerProfile ProfileManager::getCurrentProfile() const {
    return currentProfile_;
}

std::string ProfileManager::getProfileName() const {
    switch (currentProfile_) {
        case PowerProfile::Silent: return "Silent";
        case PowerProfile::Normal: return "Normal";
        case PowerProfile::Performance: return "Performance";
        case PowerProfile::Maximum: return "Maximum";
    }
    return "Unknown";
}

void ProfileManager::enforceProfileRules() {
    auto thermals = thermal_.getSystemThermals();
    if (!thermals) return;

    // Armoury Crate logic for dual-fan setups (Max RPM ~5900 on MacBook Pro 15")
    switch (currentProfile_) {
        case PowerProfile::Silent:
            // Cap fan speed low to prioritize silence over thermals
            if (thermals->cpuTemp < 75.0) {
                smc_.setFanSpeed(0, 2000);
                smc_.setFanSpeed(1, 2000);
            } else {
                smc_.setFanSpeed(0, 3200);
                smc_.setFanSpeed(1, 3200);
            }
            break;
            
        case PowerProfile::Normal:
            // Setting target to 0 releases manual override and returns control to macOS
            smc_.setFanSpeed(0, 0); 
            smc_.setFanSpeed(1, 0);
            break;
            
        case PowerProfile::Performance:
            // High baseline RPM to prevent thermal throttling under sustained load
            if (thermals->cpuTemp > 60.0) {
                smc_.setFanSpeed(0, 4800);
                smc_.setFanSpeed(1, 4800);
            }
            break;

        case PowerProfile::Maximum:
            // 100% RPM regardless of temperature
            smc_.setFanSpeed(0, 5900);
            smc_.setFanSpeed(1, 5900);
            break;
    }
}

} // end of namespace macms::system