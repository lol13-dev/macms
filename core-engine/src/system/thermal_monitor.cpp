// core-engine/src/system/thermal_monitor.cpp
#include "system/thermal_monitor.hpp"
#include <iostream>

namespace macms::system {

ThermalMonitor::ThermalMonitor(SmcController& smc) : smc_(smc) {}

std::optional<ThermalData> ThermalMonitor::getSystemThermals() {
    ThermalData data{};

    auto cpuOpt = smc_.getCpuTemperature();
    auto gpuOpt = getGpuTemperature();
    auto fanLeftOpt = smc_.getFanSpeed(0); // Left Fan
    auto fanRightOpt = smc_.getFanSpeed(1); // Right Fan

    // Log exactly which sensors are failing on this specific logic board
    if (!cpuOpt) std::cerr << "[ThermalMonitor] SMC Read Failed: CPU Temp (TC0F)\n";
    if (!gpuOpt) std::cerr << "[ThermalMonitor] SMC Read Failed: GPU Temp (TG0D)\n";
    if (!fanLeftOpt) std::cerr << "[ThermalMonitor] SMC Read Failed: Left Fan (F0Ac)\n";
    if (!fanRightOpt) std::cerr << "[ThermalMonitor] SMC Read Failed: Right Fan (F1Ac)\n";

    // Fault tolerance: Send default 0s for broken sensors instead of aborting
    // This guarantees the JSON payload is sent and unlocks the React UI.
    data.cpuTemp = cpuOpt.value_or(0.0);
    data.gpuTemp = gpuOpt.value_or(0.0);
    data.fanLeftRpm = fanLeftOpt.value_or(0);
    data.fanRightRpm = fanRightOpt.value_or(0);

    return data; 
}

std::optional<double> ThermalMonitor::getGpuTemperature() {
    auto data = smc_.readKey("TG0D"); 
    if (!data) return std::nullopt;
    
    int16_t val = (data->bytes[0] << 8) | data->bytes[1];
    return val / 256.0;
}

} // namespace macms::system