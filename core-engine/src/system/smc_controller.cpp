// core-engine/src/system/smc_controller.cpp
#include "system/smc_controller.hpp"
#include <iostream>
#include <cstring>
#include <string>

namespace macms::system {

constexpr uint8_t kSMCUserClientCmd = 2;
constexpr uint8_t kSMCReadKey       = 5;
constexpr uint8_t kSMCWriteKey      = 6;
constexpr uint8_t kSMCGetKeyInfo    = 9;

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

    // Convert the 4-char array key into a 32-bit unsigned integer for the kernel
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

    // Ensure kernel success and that AppleSMC didn't return a soft error in the result field
    if (result == kIOReturnSuccess && outputStruct.result == 0) {
        output.dataSize = outputStruct.keyInfo.dataSize;
        output.dataType = outputStruct.keyInfo.dataType;
        std::memcpy(output.bytes, outputStruct.bytes, 32);
        return true;
    }
    return false;
}

std::optional<SmcKeyData> SmcController::readKey(std::string_view keyName) {
    SMCKey key = makeKey(keyName);
    SmcKeyData input{}, output{};

    if (!callSmc(kSMCGetKeyInfo, key, input, output)) return std::nullopt;

    input.dataSize = output.dataSize;
    input.dataType = output.dataType;
    
    if (!callSmc(kSMCReadKey, key, input, output)) return std::nullopt;
    
    return output;
}

bool SmcController::writeKey(std::string_view keyName, const SmcKeyData& data) {
    SMCKey key = makeKey(keyName);
    SmcKeyData output{};
    return callSmc(kSMCWriteKey, key, data, output);
}

std::optional<double> SmcController::getCpuTemperature() {
    auto data = readKey("TC0F"); 
    if (!data) return std::nullopt;
    int16_t val = (data->bytes[0] << 8) | data->bytes[1];
    return val / 256.0;
}

std::optional<int> SmcController::getFanSpeed(int fanIndex) {
    std::string keyName = "F" + std::to_string(fanIndex) + "Ac";
    auto data = readKey(keyName);
    if (!data) return std::nullopt;
    uint16_t val = (data->bytes[0] << 8) | data->bytes[1];
    return val >> 2;
}

bool SmcController::setFanSpeed(int fanIndex, int targetRpm) {
    std::string keyName = "F" + std::to_string(fanIndex) + "Tg";
    SmcKeyData input{};
    input.dataSize = 2;
    input.dataType = ('f' << 24) | ('p' << 16) | ('e' << 8) | '2'; 
    uint16_t val = targetRpm << 2;
    input.bytes[0] = (val >> 8) & 0xFF;
    input.bytes[1] = val & 0xFF;
    
    return writeKey(keyName, input);
}

} // end of namespace macms::system