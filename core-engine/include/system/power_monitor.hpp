// folder location: core-engine/include/system/power_monitor.hpp
#pragma once

namespace macms::system {

// READS the REAL power source from the macOS (IOKit Power Sources API).
class PowerMonitor {
public:
    // Constructor.
    PowerMonitor() = default;

    // Destructor.
    ~PowerMonitor() = default;

    PowerMonitor(const PowerMonitor&) = delete;
    PowerMonitor& operator=(const PowerMonitor&) = delete;

    // TRUE ONLY when macOS reports AC power (CHARGER PLUGGED IN)
    // [!] Failsafe: IF THE STATE CAN'T BE READ, returns FALSE (treated as battery).
    bool isOnACPower() const;
};

} // end of namespace macms::system