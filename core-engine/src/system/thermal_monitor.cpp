// core-engine/src/system/thermal_monitor.cpp
#include "system/thermal_monitor.hpp"
#include <iostream>

namespace macms::system {

ThermalMonitor::ThermalMonitor(SmcController& smc) : smc_(smc) {}

std::optional<ThermalData> ThermalMonitor::getSystemThermals() {
    ThermalData data{};

    // [ADDED] Queries SmcController (IOKit on macOS, IOCTL 0x220000 on Windows Boot Camp).
    auto cpuOpt = smc_.getCpuTemperature();   // [ADDED] Reads "TCXC" (CPU PECI package) or "TC0D" fallback.
    auto gpuOpt = getGpuTemperature();        // [ADDED] Reads "TG0D" (Radeon discrete GPU die sensor).
    auto fanLeftOpt = smc_.getFanSpeed(0);    // [ADDED] Reads "F0Ac" (Left cooling fan RPM).
    auto fanRightOpt = smc_.getFanSpeed(1);   // [ADDED] Reads "F1Ac" (Right cooling fan RPM).

    // Log exactly which sensors are failing on this specific logic board
    if (!cpuOpt) std::cerr << "[ThermalMonitor_STATUS!] SMC Read Failed: CPU Temp (TCXC/TC0D)\n";
    if (!gpuOpt) std::cerr << "[ThermalMonitor_STATUS!] SMC Read Failed: GPU Temp (TG0D)\n";
    if (!fanLeftOpt) std::cerr << "[ThermalMonitor_STATUS!] SMC Read Failed: Left Fan (F0Ac)\n";
    if (!fanRightOpt) std::cerr << "[ThermalMonitor_STATUS!] SMC Read Failed: Right Fan (F1Ac)\n";

    // Fault tolerance: Send default 0s for broken sensors instead of aborting
    // This guarantees the JSON payload is sent and unlocks the React UI.
    data.cpuTemp = cpuOpt.value_or(0.0);
    data.gpuTemp = gpuOpt.value_or(0.0);
    data.fanLeftRpm = fanLeftOpt.value_or(0);
    data.fanRightRpm = fanRightOpt.value_or(0);

    return data; 
}

std::optional<double> ThermalMonitor::getGpuTemperature() {
    // [ADDED] "TG0D" returns 2-byte sp78 fixed-point temperature on the 15-inch A1707 Radeon GPU.
    auto data = smc_.readKey("TG0D"); 
    if (!data) return std::nullopt;
    
    // [ADDED] Convert big-endian signed 8.8 fixed-point bytes into Celsius (high byte + low byte / 256).
    int16_t val = (data->bytes[0] << 8) | data->bytes[1];
    return val / 256.0;
}

} // end of namespace macms::system