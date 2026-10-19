// folder location: core-engine/src/system/thermal_monitor.cpp
#include "system/thermal_monitor.hpp"

namespace macms::system {

ThermalMonitor::ThermalMonitor(SmcController& smc) : smc_(smc) {}

std::optional<ThermalData> ThermalMonitor::getSystemThermals() {
    ThermalData data{};

    auto cpuOpt = smc_.getCpuTemperature();
    auto gpuOpt = getGpuTemperature();
    auto fanLeftOpt = smc_.getFanSpeed(0); // Left Fan
    auto fanRightOpt = smc_.getFanSpeed(1); // Right Fan

    if (!cpuOpt || !fanLeftOpt) return std::nullopt;

    data.cpuTemp = *cpuOpt;
    data.gpuTemp = gpuOpt.value_or(0.0); // FALLBACK if no dedicated GPU exists
    data.fanLeftRpm = *fanLeftOpt;
    data.fanRightRpm = fanRightOpt.value_or(0); 

    return data;
}

std::optional<double> ThermalMonitor::getGpuTemperature() {
    // NOTE: TG0D is typically the GPU Die sensor on Intel MacBooks (like the A1707)
    auto data = smc_.readKey("TG0D"); 
    if (!data) return std::nullopt;
    
    int16_t val = (data->bytes[0] << 8) | data->bytes[1];
    return val / 256.0;
}

} // end of namespace macms::system