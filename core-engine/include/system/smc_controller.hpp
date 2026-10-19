// folder location: core-engine/include/system/smc_controller.hpp
#pragma once

#include <string_view>
#include <optional>
#include <array>
#include <cstdint>
#include <IOKit/IOKitLib.h>

namespace macms::system {

// APPLE'S SMC uses 4-byte character arrays to IDENTIFY HARDWARE KEYS (e.g., "F0Tg")
using SMCKey = std::array<char, 4>;

// STANDARD struct required for PASSING DATA in and out of the AppleSMC driver.
struct SmcKeyData {
    uint32_t dataSize;
    uint32_t dataType;
    uint8_t dataAttributes;
    uint8_t bytes[32];
};

class SmcController {
public:
    // CONSTRUCTOR.
    SmcController();

    // DESTRUCTOR.
    ~SmcController();

    // SAFELY prevent copying TO PROTECT the kernel CONNECTION.
    SmcController(const SmcController&) = delete;
    SmcController& operator=(const SmcController&) = delete;

    // Connection Decision.
    bool connect();
    void disconnect();

    std::optional<SmcKeyData> readKey(std::string_view keyName);
    bool writeKey(std::string_view keyName, const SmcKeyData& data);

    std::optional<double> getCpuTemperature();
    std::optional<int> getFanSpeed(int fanIndex);
    bool setFanSpeed(int fanIndex, int targetRpm);

private:
    io_connect_t connection_{0}; // HANDLE for THE KERNEL CONNECTION.

    static SMCKey makeKey(std::string_view str);
    bool callSmc(uint8_t command, const SMCKey& key, const SmcKeyData& input, SmcKeyData& output);
};

} // end of: namespace macms::system