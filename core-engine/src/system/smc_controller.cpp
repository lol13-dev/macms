// core-engine/src/system/smc_controller.cpp
#include "system/smc_controller.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <string>
#include <algorithm>

namespace macms::system {

constexpr uint8_t kSMCUserClientCmd = 2;
constexpr uint8_t kSMCReadKey       = 5;
constexpr uint8_t kSMCWriteKey      = 6;
constexpr uint8_t kSMCGetKeyInfo    = 9;

namespace {

constexpr uint32_t fourCC(const char (&s)[5]) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(s[0])) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(s[1])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(s[2])) << 8)  |
            static_cast<uint32_t>(static_cast<uint8_t>(s[3]));
}

constexpr uint32_t kTypeFpe2 = fourCC("fpe2"); // unsigned 14.2 fixed point, big-endian (older Macs)
constexpr uint32_t kTypeFlt  = fourCC("flt ");  // 32-bit float, little-endian (2016+ Macs)
constexpr uint32_t kTypeUi8  = fourCC("ui8 ");
constexpr uint32_t kTypeUi16 = fourCC("ui16");

// Decodes any numeric SMC value we care about, based on its REAL type.
std::optional<double> decodeNumber(const SmcKeyData& d) {
    if (d.dataType == kTypeFpe2 && d.dataSize >= 2) {
        uint16_t raw = (static_cast<uint16_t>(d.bytes[0]) << 8) | d.bytes[1];
        return raw / 4.0;
    }
    if (d.dataType == kTypeFlt && d.dataSize == 4) {
        float f;
        std::memcpy(&f, d.bytes, sizeof(f));
        return static_cast<double>(f);
    }
    if (d.dataType == kTypeUi8 && d.dataSize >= 1) {
        return static_cast<double>(d.bytes[0]);
    }
    if (d.dataType == kTypeUi16 && d.dataSize >= 2) {
        return static_cast<double>((static_cast<uint16_t>(d.bytes[0]) << 8) | d.bytes[1]);
    }
    return std::nullopt;
}

} // namespace

// macOS Kernel Memory Map for SMC Communication (Strict 80-byte alignment)
typedef struct {
    char                  major;
    char                  minor;
    char                  build;
    char                  reserved[1];
    uint16_t              release;
} SMCKeyData_vers_t;

typedef struct {
    uint16_t              version;
    uint16_t              length;
    uint32_t              cpuPLimit;
    uint32_t              gpuPLimit;
    uint32_t              memPLimit;
} SMCKeyData_pLimitData_t;

typedef struct {
    uint32_t              dataSize;
    uint32_t              dataType;
    char                  dataAttributes;
} SMCKeyData_keyInfo_t;

typedef struct {
    uint32_t                  key;
    SMCKeyData_vers_t         vers;
    SMCKeyData_pLimitData_t   pLimitData;
    SMCKeyData_keyInfo_t      keyInfo;
    char                      result;
    char                      status;
    char                      data8;
    uint32_t                  data32;
    uint8_t                   bytes[32];
} AppleSMCParamStruct;


SmcController::SmcController() {
    connect();
}

SmcController::~SmcController() {
    // SAFETY: never leave the fans pinned in manual mode when the daemon exits.
    if (connection_ != 0) {
        for (int i = 0; i < getFanCount(); ++i) {
            releaseFanControl(i);
        }
    }
    disconnect();
}

bool SmcController::connect() {
    if (connection_ != 0) return true;

    CFMutableDictionaryRef matchingDict = IOServiceMatching("AppleSMC");
    if (!matchingDict) return false;

    io_iterator_t iterator;
    kern_return_t result = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iterator);
    if (result != kIOReturnSuccess) return false;

    io_object_t device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0) return false;

    result = IOServiceOpen(device, mach_task_self(), 0, &connection_);
    IOObjectRelease(device);

    return result == kIOReturnSuccess;
}

void SmcController::disconnect() {
    if (connection_ != 0) {
        IOServiceClose(connection_);
        connection_ = 0;
    }
}

SMCKey SmcController::makeKey(std::string_view str) {
    SMCKey key = {0, 0, 0, 0};
    for (size_t i = 0; i < str.length() && i < 4; ++i) {
        key[i] = str[i];
    }
    return key;
}

bool SmcController::callSmc(uint8_t command, const SMCKey& key, const SmcKeyData& input, SmcKeyData& output) {
    if (connection_ == 0) return false;

    AppleSMCParamStruct inputStruct{};
    AppleSMCParamStruct outputStruct{};

    uint32_t uintKey = (static_cast<uint8_t>(key[0]) << 24) |
                       (static_cast<uint8_t>(key[1]) << 16) |
                       (static_cast<uint8_t>(key[2]) << 8)  |
                       (static_cast<uint8_t>(key[3]));

    inputStruct.key = uintKey;
    inputStruct.data8 = command;
    inputStruct.keyInfo.dataSize = input.dataSize;
    inputStruct.keyInfo.dataType = input.dataType;
    std::memcpy(inputStruct.bytes, input.bytes, 32);

    size_t inputSize = sizeof(AppleSMCParamStruct);
    size_t outputSize = sizeof(AppleSMCParamStruct);

    kern_return_t result = IOConnectCallStructMethod(
        connection_,
        kSMCUserClientCmd,
        &inputStruct, inputSize,
        &outputStruct, &outputSize
    );

    if (result == kIOReturnSuccess && outputStruct.result == 0) {
        output.dataSize = outputStruct.keyInfo.dataSize;
        output.dataType = outputStruct.keyInfo.dataType;
        std::memcpy(output.bytes, outputStruct.bytes, 32);
        return true;
    }

    // Only log WRITE failures. Failed reads are normal (a sensor may not exist on this board).
    if (command == kSMCWriteKey) {
        std::cerr << "[SMC] WRITE FAILED key='" << std::string(key.data(), 4)
                  << "' kern=0x" << std::hex << static_cast<unsigned>(result)
                  << " smcResult=0x" << static_cast<unsigned>(static_cast<uint8_t>(outputStruct.result))
                  << std::dec << "\n";
    }
    return false;
}

std::optional<SmcKeyData> SmcController::readKey(std::string_view keyName) {
    SMCKey key = makeKey(keyName);
    SmcKeyData input{}, info{}, output{};

    if (!callSmc(kSMCGetKeyInfo, key, input, info)) return std::nullopt;

    input.dataSize = info.dataSize;
    input.dataType = info.dataType;

    if (!callSmc(kSMCReadKey, key, input, output)) return std::nullopt;

    // The read response does not carry the key metadata, so keep what GetKeyInfo told us.
    output.dataSize = info.dataSize;
    output.dataType = info.dataType;
    return output;
}

bool SmcController::writeKey(std::string_view keyName, const SmcKeyData& data) {
    SMCKey key = makeKey(keyName);
    SmcKeyData output{};
    return callSmc(kSMCWriteKey, key, data, output);
}

std::optional<double> SmcController::readNumber(std::string_view keyName) {
    auto data = readKey(keyName);
    if (!data) return std::nullopt;
    return decodeNumber(*data);
}

std::optional<double> SmcController::getCpuTemperature() {
    // 1. TARGET the Intel PECI Package sensor first (matches the Stats app EXACTLY)
    auto data = readKey("TCXC");
    if (!data) {
        // 2. FALLBACK to the CPU Diode if PECI fails 
        data = readKey("TC0D");
        if (!data) return std::nullopt;
    }
    // 3. DECODE the SP78 formatted temperature data
    int16_t val = (data->bytes[0] << 8) | data->bytes[1];
    return val / 256.0;
}

std::optional<int> SmcController::getFanSpeed(int fanIndex) {
    auto value = readNumber("F" + std::to_string(fanIndex) + "Ac");
    if (!value) return std::nullopt;
    return static_cast<int>(*value + 0.5);
}

int SmcController::getFanCount() {
    auto n = readNumber("FNum");
    return n ? static_cast<int>(*n) : 2;
}

// Fans ignore target-RPM writes unless they are in MANUAL mode.
//  - Newer Macs: per-fan mode key F<n>Md (0 = auto, 1 = manual)
//  - Older Intel Macs: global bitmask "FS! " (bit n = fan n forced)
bool SmcController::setFanManualMode(int fanIndex, bool manual) {
    const std::string modeKey = "F" + std::to_string(fanIndex) + "Md";

    if (auto info = readKey(modeKey)) {
        SmcKeyData in{};
        in.dataSize = info->dataSize;
        in.dataType = info->dataType;
        in.bytes[0] = manual ? 1 : 0;
        return writeKey(modeKey, in);
    }

    if (auto info = readKey("FS! ")) {
        uint16_t bits = (static_cast<uint16_t>(info->bytes[0]) << 8) | info->bytes[1];
        if (manual) bits |= static_cast<uint16_t>(1u << fanIndex);
        else        bits &= static_cast<uint16_t>(~(1u << fanIndex));

        SmcKeyData in{};
        in.dataSize = info->dataSize;
        in.dataType = info->dataType;
        in.bytes[0] = (bits >> 8) & 0xFF;
        in.bytes[1] = bits & 0xFF;
        return writeKey("FS! ", in);
    }

    std::cerr << "[SMC] No fan mode key found (F" << fanIndex << "Md / FS! ). "
              << "This Mac may not allow manual fan control.\n";
    return false;
}

bool SmcController::releaseFanControl(int fanIndex) {
    return setFanManualMode(fanIndex, false);
}

bool SmcController::setFanSpeed(int fanIndex, int targetRpm) {
    const std::string idx = std::to_string(fanIndex);
    const std::string targetKey = "F" + idx + "Tg";

    // Learn the REAL type of the target key (fpe2 on older Macs, flt on newer ones).
    auto info = readKey(targetKey);
    if (!info) {
        std::cerr << "[SMC] Cannot read " << targetKey << " - fan " << fanIndex << " not controllable.\n";
        return false;
    }

    // Respect the hardware's own limits.
    if (auto mn = readNumber("F" + idx + "Mn")) targetRpm = std::max(targetRpm, static_cast<int>(*mn));
    if (auto mx = readNumber("F" + idx + "Mx")) targetRpm = std::min(targetRpm, static_cast<int>(*mx));

    // Step 1: take manual control. Without this the SMC ignores the target.
    if (!setFanManualMode(fanIndex, true)) {
        std::cerr << "[SMC] Could not switch fan " << fanIndex << " to manual mode.\n";
        return false;
    }

    // Step 2: encode the RPM in the key's real format.
    SmcKeyData in{};
    in.dataSize = info->dataSize;
    in.dataType = info->dataType;

    if (info->dataType == kTypeFpe2) {
        uint16_t raw = static_cast<uint16_t>(targetRpm * 4);
        in.bytes[0] = (raw >> 8) & 0xFF;
        in.bytes[1] = raw & 0xFF;
    } else if (info->dataType == kTypeFlt && info->dataSize == 4) {
        float f = static_cast<float>(targetRpm);
        std::memcpy(in.bytes, &f, sizeof(f));
    } else {
        std::cerr << "[SMC] Unsupported type for " << targetKey << "\n";
        return false;
    }

    return writeKey(targetKey, in);
}

void SmcController::dumpFanKeys() {
    std::cout << "[SMC] ---- fan key dump ----\n";
    for (const char* k : {"FNum", "FS! ", "F0Md", "F0Mn", "F0Mx", "F0Tg", "F0Ac", "F1Md", "F1Tg", "F1Ac"}) {
        auto d = readKey(k);
        if (!d) {
            std::cout << "[SMC] " << k << " -> NOT FOUND\n";
            continue;
        }
        char t[5] = {
            static_cast<char>(d->dataType >> 24), static_cast<char>(d->dataType >> 16),
            static_cast<char>(d->dataType >> 8),  static_cast<char>(d->dataType), 0
        };
        std::cout << "[SMC] " << k << " type='" << t << "' size=" << d->dataSize << " bytes:";
        for (uint32_t i = 0; i < d->dataSize && i < 32; ++i) {
            std::cout << ' ' << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(d->bytes[i]);
        }
        std::cout << std::dec << std::setfill(' ') << "\n";
    }
    std::cout << "[SMC] ----------------------\n";
}

} // end of namespace macms::system