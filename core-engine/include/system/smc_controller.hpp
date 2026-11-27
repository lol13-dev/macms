// folder location: core-engine/include/system/smc_controller.hpp
#pragma once

#include <string_view>
#include <optional>
#include <array>
#include <cstdint>

#ifdef __APPLE__
    #include <IOKit/IOKitLib.h>
#elif defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

namespace macms::system {

// [NOTE] Apple SMC keys are identified by fixed 4-byte FourCC-style names such as "F0Tg".
using SMCKey = std::array<char, 4>;

// [ADD] This structure carries an SMC value's byte size, type code, attributes, and payload.
struct SmcKeyData {
    uint32_t dataSize;
    uint32_t dataType;
    uint8_t dataAttributes;
    uint8_t bytes[32];
};

class SmcController {
public:
    // [ADD] Construction opens the platform SMC connection; destruction closes and restores fan state.
    SmcController();
    ~SmcController();

    // [NOTE] Copying is disabled because multiple objects could otherwise share or duplicate a kernel/device handle.
    SmcController(const SmcController&) = delete;
    SmcController& operator=(const SmcController&) = delete;

    // [ADD] Connect and disconnect manage the platform-specific AppleSMC connection handle.
    bool connect();
    void disconnect();

    // [NOTE] readKey returns metadata and raw bytes; writeKey sends them back to the SMC.
    std::optional<SmcKeyData> readKey(std::string_view keyName);
    bool writeKey(std::string_view keyName, const SmcKeyData& data);

    // [ADD] These convenience methods provide decoded temperature and fan-speed values.
    std::optional<double> getCpuTemperature();
    std::optional<int> getFanSpeed(int fanIndex);

    // [NOTE] setFanSpeed switches the fan to manual mode, writes the target RPM, and returns false on failure.
    bool setFanSpeed(int fanIndex, int targetRpm);

    // [NOTE] releaseFanControl returns the selected fan to automatic platform control.
    bool releaseFanControl(int fanIndex);

    // [ADD] dumpFanKeys is a diagnostic helper that prints key metadata and raw bytes.
    void dumpFanKeys();

private:
#ifdef __APPLE__
    // [NOTE] On macOS this handle represents the IOKit connection to AppleSMC.
    io_connect_t connection_{0};
#elif defined(_WIN32)
    // [NOTE] On Windows this handle represents the opened \\.\AppleSMC device.
    HANDLE connection_{INVALID_HANDLE_VALUE};
#endif

    // [ADD] isConnected checks whether the platform-specific connection handle is active.
    bool isConnected() const;

    // [NOTE] makeKey creates a 4-byte key; callSmc performs one SMC command through the platform driver.
    static SMCKey makeKey(std::string_view str);
    bool callSmc(uint8_t command, const SMCKey& key, const SmcKeyData& input, SmcKeyData& output);

    // [ADD] Internal helpers decode numeric values and manage per-fan or shared fan mode controls.
    std::optional<double> readNumber(std::string_view keyName);
    bool setFanManualMode(int fanIndex, bool manual);
    int getFanCount();
};

} // end of: namespace macms::system