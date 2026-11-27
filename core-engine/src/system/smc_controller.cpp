// core-engine/src/system/smc_controller.cpp
#include "system/smc_controller.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <string>
#include <algorithm>

namespace macms::system {

// [ADD] These command values identify the SMC operation sent to the platform driver.
constexpr uint8_t kSMCReadKey    = 5;
constexpr uint8_t kSMCWriteKey   = 6;
constexpr uint8_t kSMCGetKeyInfo = 9;

#ifdef __APPLE__
// [NOTE] macOS sends SMC commands through IOKit using a user-client method number.
constexpr uint8_t kSMCUserClientCmd = 2;
#elif defined(_WIN32)
// [NOTE] Windows uses Win32 device I/O control codes instead of IOKit calls.
constexpr DWORD kWinIoctlReadKey  = 0x220000;
constexpr DWORD kWinIoctlWriteKey = 0x220004;

#pragma pack(push, 1)
// [ADD] The four-character key and byte payload are packed exactly for the AppleSMC Win32 driver.
struct WinSmcRequest {
    uint32_t key;
    uint8_t  dataSize;
    uint8_t  data[32];
};
#pragma pack(pop)
#endif

namespace {

// [ADD] Builds the 32-bit FourCC value used by Apple SMC data types from four characters.
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
    // [NOTE] fpe2 represents an unsigned fixed-point value: the raw integer is divided by 4.
    if (d.dataType == kTypeFpe2 && d.dataSize >= 2) {
        uint16_t raw = (static_cast<uint16_t>(d.bytes[0]) << 8) | d.bytes[1];
        return raw / 4.0;
    }
    if (d.dataType == kTypeFlt && d.dataSize == 4) {
        // [ADD] flt is copied into a native float so its byte representation can be decoded.
        float f;
        std::memcpy(&f, d.bytes, sizeof(f));
        return static_cast<double>(f);
    }
    if (d.dataType == kTypeUi8 && d.dataSize >= 1) {
        return static_cast<double>(d.bytes[0]);
    }
    if (d.dataType == kTypeUi16 && d.dataSize >= 2) {
        // [NOTE] ui16 values are encoded in big-endian byte order.
        return static_cast<double>((static_cast<uint16_t>(d.bytes[0]) << 8) | d.bytes[1]);
    }
    return std::nullopt;
}

} // namespace

#ifdef __APPLE__
// macOS Kernel Memory Map for SMC Communication (Strict 80-byte alignment)
// [ADD] These structs mirror the Apple SMC parameter layout for IOKit structured calls.
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
#endif

SmcController::SmcController() {
    // [ADD] Construction attempts to open the AppleSMC connection immediately.
    connect();
}

SmcController::~SmcController() {
    // [NOTE] Destruction safely returns each fan to automatic control before closing the connection.
    if (isConnected()) {
        for (int i = 0; i < getFanCount(); ++i) {
            releaseFanControl(i);
        }
    }
    disconnect();
}

bool SmcController::isConnected() const {
#ifdef __APPLE__
    // [NOTE] A macOS connection is valid when its IOKit handle is nonzero.
    return connection_ != 0;
#elif defined(_WIN32)
    // [NOTE] A Windows connection is valid until CreateFileA returns INVALID_HANDLE_VALUE.
    return connection_ != INVALID_HANDLE_VALUE;
#else
    return false;
#endif
}

#ifdef __APPLE__
bool SmcController::connect() {
    // [ADD] Avoid reopening if this controller already has an active IOKit connection.
    if (connection_ != 0) return true;

    // [NOTE] IOServiceMatching finds registered services named AppleSMC.
    CFMutableDictionaryRef matchingDict = IOServiceMatching("AppleSMC");
    if (!matchingDict) return false;

    io_iterator_t iterator;
    kern_return_t result = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iterator);
    if (result != kIOReturnSuccess) return false;

    io_object_t device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0) return false;

    // [ADD] IOServiceOpen converts the discovered service into a user-client connection handle.
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

bool SmcController::callSmc(uint8_t command, const SMCKey& key, const SmcKeyData& input, SmcKeyData& output) {
    if (connection_ == 0) return false;

    // [ADD] Each call builds one 80-byte AppleSMC parameter structure for IOKit.
    AppleSMCParamStruct inputStruct{};
    AppleSMCParamStruct outputStruct{};

    uint32_t uintKey = (static_cast<uint8_t>(key[0]) << 24) |
                       (static_cast<uint8_t>(key[1]) << 16) |
                       (static_cast<uint8_t>(key[2]) << 8)  |
                       (static_cast<uint8_t>(key[3]));

    inputStruct.key = uintKey;
    inputStruct.data8 = command;
    // [NOTE] keyInfo carries the requested data size and type into or out of the SMC.
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
        // [ADD] Copy both metadata and raw bytes back to the common SmcKeyData representation.
        output.dataSize = outputStruct.keyInfo.dataSize;
        output.dataType = outputStruct.keyInfo.dataType;
        std::memcpy(output.bytes, outputStruct.bytes, 32);
        return true;
    }

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

    // [NOTE] macOS first asks for key metadata, then uses that size/type for the read call.
    if (!callSmc(kSMCGetKeyInfo, key, input, info)) return std::nullopt;

    input.dataSize = info.dataSize;
    input.dataType = info.dataType;

    if (!callSmc(kSMCReadKey, key, input, output)) return std::nullopt;

    output.dataSize = info.dataSize;
    output.dataType = info.dataType;
    return output;
}

#elif defined(_WIN32)
bool SmcController::connect() {
    // [ADD] The Windows AppleSMC driver is exposed as a named device file.
    if (connection_ != INVALID_HANDLE_VALUE) return true;

    connection_ = CreateFileA(
        "\\\\.\\AppleSMC",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (connection_ == INVALID_HANDLE_VALUE) {
        std::cerr << "[SMC] Failed to open \\\\.\\AppleSMC (Win32 Error "
                  << GetLastError() << "). Run as Admin and ensure AppleSMC is started.\n";
        return false;
    }
    return true;
}

void SmcController::disconnect() {
    if (connection_ != INVALID_HANDLE_VALUE) {
        CloseHandle(connection_);
        connection_ = INVALID_HANDLE_VALUE;
    }
}

bool SmcController::callSmc(uint8_t command, const SMCKey& key, const SmcKeyData& input, SmcKeyData& output) {
    if (connection_ == INVALID_HANDLE_VALUE) return false;

    // [NOTE] The 4-byte SMC key is copied directly because it is already stored in FourCC order.
    uint32_t rawKey = 0;
    std::memcpy(&rawKey, key.data(), 4);
    DWORD bytesReturned = 0;

    if (command == kSMCReadKey) {
        // [ADD] ReadKey requests 32 bytes and the result length determines the inferred SMC type.
        WinSmcRequest req{};
        req.key = rawKey;
        req.dataSize = 32;

        BOOL ok = DeviceIoControl(
            connection_,
            kWinIoctlReadKey,
            &req, sizeof(req),
            output.bytes, sizeof(output.bytes),
            &bytesReturned,
            nullptr
        );

        if (!ok || bytesReturned == 0 || bytesReturned > 32) {
            return false;
        }

        output.dataSize = static_cast<uint32_t>(bytesReturned);
        // [NOTE] The Windows API does not return Apple key type metadata, so size provides a reasonable type guess.
        if (bytesReturned == 1) {
            output.dataType = kTypeUi8;
        } else if (bytesReturned == 2) {
            output.dataType = kTypeFpe2;
        } else if (bytesReturned == 4) {
            output.dataType = kTypeFlt;
        } else {
            output.dataType = 0;
        }
        return true;
    }

    // [CHANGE] Inside SmcController::callSmc() under #elif defined(_WIN32):
    if (command == kSMCWriteKey) {
        if (input.dataSize == 0 || input.dataSize > 32) return false;

        WinSmcRequest req{};
        req.key = rawKey;
        req.dataSize = static_cast<uint8_t>(input.dataSize);
        std::memcpy(req.data, input.bytes, req.dataSize);

        // [ADD] FUN_1400016c0 requires OutputBufferLength >= 1 even for WriteKey (0x220004).
        uint8_t dummyOut[4]{0};
        BOOL ok = DeviceIoControl(
            connection_,
            kWinIoctlWriteKey,
            &req, sizeof(req),
            dummyOut, sizeof(dummyOut),
            &bytesReturned,
            nullptr
        );

        if (!ok) {
            std::cerr << "[SMC] WRITE FAILED key='" << std::string(key.data(), 4)
                      << "' winErr=" << GetLastError() << "\n";
        }
        return ok != FALSE;
    }

    return false;
}

std::optional<SmcKeyData> SmcController::readKey(std::string_view keyName) {
    // [NOTE] On Windows the read call itself supplies the bytes and inferred data type.
    SMCKey key = makeKey(keyName);
    SmcKeyData input{}, output{};
    if (!callSmc(kSMCReadKey, key, input, output)) return std::nullopt;
    return output;
}
#endif

SMCKey SmcController::makeKey(std::string_view str) {
    // [ADD] Four-character SMC names are converted to a fixed-size array, zero-padding unused positions.
    SMCKey key = {0, 0, 0, 0};
    for (size_t i = 0; i < str.length() && i < 4; ++i) {
        key[i] = str[i];
    }
    return key;
}

bool SmcController::writeKey(std::string_view keyName, const SmcKeyData& data) {
    // [NOTE] writeKey converts the name once and forwards the caller's size and bytes to callSmc.
    SMCKey key = makeKey(keyName);
    SmcKeyData output{};
    return callSmc(kSMCWriteKey, key, data, output);
}

std::optional<double> SmcController::readNumber(std::string_view keyName) {
    // [ADD] readNumber combines the raw-key read with type-aware numeric decoding.
    auto data = readKey(keyName);
    if (!data) return std::nullopt;
    return decodeNumber(*data);
}

std::optional<double> SmcController::getCpuTemperature() {
    // [NOTE] TCXC is the preferred CPU temperature key; TC0D is a fallback on some Macs.
    auto data = readKey("TCXC");
    if (!data) {
        data = readKey("TC0D");
        if (!data) return std::nullopt;
    }
    int16_t val = (data->bytes[0] << 8) | data->bytes[1];
    // [ADD] CPU temperature bytes use a signed big-endian fixed-point value with 8 fractional bits.
    return val / 256.0;
}

std::optional<int> SmcController::getFanSpeed(int fanIndex) {
    // [NOTE] Fan actual speed keys follow the pattern F0Ac, F1Ac, and so on.
    auto value = readNumber("F" + std::to_string(fanIndex) + "Ac");
    if (!value) return std::nullopt;
    return static_cast<int>(*value + 0.5);
}

int SmcController::getFanCount() {
    // [ADD] FNum reports the fan count when available; two is the compatibility fallback.
    auto n = readNumber("FNum");
    return n ? static_cast<int>(*n) : 2;
}

bool SmcController::setFanManualMode(int fanIndex, bool manual) {
    const std::string modeKey = "F" + std::to_string(fanIndex) + "Md";

    // [NOTE] Modern Macs commonly expose one mode key per fan.
    if (auto info = readKey(modeKey)) {
        SmcKeyData in{};
        in.dataSize = info->dataSize;
        in.dataType = info->dataType;
        in.bytes[0] = manual ? 1 : 0;
        return writeKey(modeKey, in);
    }

    // [CHANGE] Inside SmcController::setFanManualMode():
    if (auto info = readKey("FS! ")) {
        uint16_t bits = (static_cast<uint16_t>(info->bytes[0]) << 8) | info->bytes[1];
        uint16_t nextBits = manual
            ? static_cast<uint16_t>(bits | (1u << fanIndex))
            : static_cast<uint16_t>(bits & ~(1u << fanIndex));

        // [ADD] Avoid unnecessary SMC writes when the fan is already in Auto/Manual mode.
        if (nextBits == bits) return true;

        SmcKeyData in{};
        in.dataSize = info->dataSize;
        in.dataType = info->dataType;
        in.bytes[0] = (nextBits >> 8) & 0xFF;
        in.bytes[1] = nextBits & 0xFF;
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
    // [ADD] This routine reads limits, enables manual fan control, and then writes the target RPM.
    const std::string idx = std::to_string(fanIndex);
    const std::string targetKey = "F" + idx + "Tg";

    auto info = readKey(targetKey);
    if (!info) {
        std::cerr << "[SMC] Cannot read " << targetKey << " - fan " << fanIndex << " not controllable.\n";
        return false;
    }

    if (auto mn = readNumber("F" + idx + "Mn")) targetRpm = std::max(targetRpm, static_cast<int>(*mn));
    if (auto mx = readNumber("F" + idx + "Mx")) targetRpm = std::min(targetRpm, static_cast<int>(*mx));
    // [NOTE] Clamping prevents an out-of-range target while preserving the existing control sequence.

    if (!setFanManualMode(fanIndex, true)) {
        std::cerr << "[SMC] Could not switch fan " << fanIndex << " to manual mode.\n";
        return false;
    }

    SmcKeyData in{};
    in.dataSize = info->dataSize;
    in.dataType = info->dataType;

    if (info->dataType == kTypeFpe2) {
        // [ADD] fpe2 encodes RPM by multiplying the integer target by 4.
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
    // [ADD] This diagnostic method prints raw key metadata and bytes to help identify supported fan keys.
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