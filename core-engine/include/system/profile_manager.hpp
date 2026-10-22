// folder location: core-engine/include/system/profile_manager.hpp
#pragma once

#include "system/smc_controller.hpp"
#include "system/thermal_monitor.hpp"
#include "system/power_monitor.hpp"
#include <atomic>
#include <string>

namespace macms::system {

// TYPE of Modes
enum class PowerProfile {
    Silent,
    Normal,
    Performance,
    Maximum
};

class ProfileManager {
public:
    // Special Constructor.
    ProfileManager(SmcController& smc, ThermalMonitor& thermal, PowerMonitor& power);

    // Special Destructor.
    ~ProfileManager() = default;
    
    ProfileManager(const ProfileManager&) = delete;
    ProfileManager& operator=(const ProfileManager&) = delete;

    // ATTEMPTS to switch the PROFILE.
    bool setProfile(PowerProfile profile, bool isCharging = false);

    PowerProfile getCurrentProfile() const;
    std::string getProfileName() const;

    // USED by the UI payload so the frontend can LOCK the maximum button on Battery.
    bool isOnACPower() const;

    // THIS WILL BE CALLED periodically in the main LOOP to dynamically adjust
    // hardware targets BASED on the active profile's rules.
    void enforceProfileRules();

private:
    SmcController& smc_;
    ThermalMonitor& thermal_;
    PowerMonitor& power_;

    // [!!] Failsafe Watchdog State.
    int overloadTicks_;
    void checkEmergencyFailsafe();

    // WRITTEN by the WebSocket thread AND the main loop thread, SO IT MUST BE atomic.
    std::atomic<PowerProfile> currentProfile_{PowerProfile::Normal};
};

} // end of namespace macms::system 