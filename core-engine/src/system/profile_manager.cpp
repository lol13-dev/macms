// folder structure: core-engine/src/system/profile_manager.cpp
#include "system/profile_manager.hpp"
#include <iostream>

namespace macms::system {

ProfileManager::ProfileManager(SmcController& smc, ThermalMonitor& thermal, PowerMonitor& power)
    : smc_(smc), thermal_(thermal), power_(power) {}

bool ProfileManager::setProfile(PowerProfile profile, bool /*isCharging*/) {
    // NEW, Safety lock: Prevent Maximum mode from draining the battery and overheating
    if (profile == PowerProfile::Maximum && !power_.isOnACPower()) {
        std::cerr << "[ProfileManager_WARNING] Cannot engage Maximum profile while on battery power.\n";
        return false;
    }

    currentProfile_ = profile;
    std::cout << "[ProfileManager] Profile switched to: " << getProfileName() << "\n";

    // Immediately apply the new fan rules
    enforceProfileRules();
    return true;
}

bool ProfileManager::isOnACPower() const {
    return power_.isOnACPower();
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

    // setFanSpeed() switches the fan to manual mode itself and logs any SMC failure.
    switch (currentProfile_) {
        case PowerProfile::Silent:
            if (thermals->cpuTemp < 75.0) {
                smc_.setFanSpeed(0, 2000);
                smc_.setFanSpeed(1, 2000);
            } else {
                smc_.setFanSpeed(0, 3200);
                smc_.setFanSpeed(1, 3200);
            }
            break;

        case PowerProfile::Normal:
            // FIXED: writing 0 to the target does NOT release control.
            // Switch the fans back to AUTOMATIC mode so macOS drives them again.
            smc_.releaseFanControl(0);
            smc_.releaseFanControl(1);
            break;

        case PowerProfile::Performance:
            if (thermals->cpuTemp > 60.0) {
                smc_.setFanSpeed(0, 4800);
                smc_.setFanSpeed(1, 4800);
            } else {
                // FIXED: below the threshold, give control back instead of
                // leaving the previous profile's RPM stuck on the fans.
                smc_.releaseFanControl(0);
                smc_.releaseFanControl(1);
            }
            break;

        case PowerProfile::Maximum:
            smc_.setFanSpeed(0, 5900);
            smc_.setFanSpeed(1, 5900);
            break;
    }
}

} // end of namespace macms::system