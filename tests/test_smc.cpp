// tests/test_smc.cpp
#include "system/smc_controller.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    macms::system::SmcController smc;
    if (!smc.connect()) return 1;

    if (auto temp = smc.getCpuTemperature()) {
        std::cout << "[Test] CPU Temp: " << *temp << " C\n";
    }
    if (auto fan0 = smc.getFanSpeed(0)) {
        std::cout << "[Test] Fan 0 Speed: " << *fan0 << " RPM\n";
    }
    if (auto fan1 = smc.getFanSpeed(1)) {
        std::cout << "[Test] Fan 1 Speed: " << *fan1 << " RPM\n";
    }

    std::cout << "[Test] Testing manual fan write (3500 RPM)...\n";
    if (smc.setFanSpeed(0, 4500)) {
        std::cout << "[Test] Write succeeded! Releasing fan control...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        smc.releaseFanControl(0);
    }

    return 0;
}