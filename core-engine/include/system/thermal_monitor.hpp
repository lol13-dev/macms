// folder location: core-engine/include/system/thermal_monitor.hpp
#pragma once
#include "system/smc_controller.hpp"
#include <optional>

namespace macms::system {

// A UNIFIED struct CONTAINING everything the UI NEEDS in ONE PACKAGE.
struct ThermalData {
    double cpuTemp;
    double gpuTemp;
    int fanLeftRpm;
    int fanRightRpm;
};

class ThermalMonitor {

public:
    // TAKES a reference to the SMC Controller, so it doesn't OPEN MULTIPLE kernel CONNECTIONS
    explicit ThermalMonitor(SmcController& smc);
    
    // Special Destructor.
    ~ThermalMonitor() = default;

    ThermalMonitor(const ThermalMonitor&) = delete;
    ThermalMonitor& operator=(const ThermalMonitor&) = delete;

    // FETCHES all RELEVANT sensor data at ONCE.
    std::optional<ThermalData> getSystemThermals();

private:
    SmcController& smc_;

    // HELPER specifically for reading the GPU temperature logic.
    std::optional<double> getGpuTemperature();
};

} // end of namespace macms::system 