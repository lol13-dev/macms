// folder location: core-engine/src/system/smc_controller.cpp
#include "system/smc_controller.hpp"
#include <iostream>
#include <cstring>
#include <string>

namespace macms::system {

constexpr uint8_t kSMCUserClientCmd = 2;
constexpr uint8_t kSMCReadKey       = 5;
constexpr uint8_t kSMCWriteKey      = 6;
constexpr uint8_t kSMCGetKeyInfo    = 9;

struct SMCParamStruct {
    SMCKey key;
    SMCKey vers;
    uint8_t pLimitData;
    uint8_t keyInfo[6];
    uint8_t padding;
    uint8_t result;
    uint8_t status;
    uint8_t data8;
    uint32_t data32;
    SmcKeyData bytes;
};

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

    SMCParamStruct inputStruct{};
    SMCParamStruct outputStruct{};

    inputStruct.key = key;
    inputStruct.data8 = command;
    inputStruct.bytes = input;

    size_t inputSize = sizeof(SMCParamStruct);
    size_t outputSize = sizeof(SMCParamStruct);

    kern_return_t result = IOConnectCallStructMethod(
        connection_,
        kSMCUserClientCmd,
        &inputStruct, inputSize,
        &outputStruct, &outputSize
    );

    if (result == kIOReturnSuccess && outputStruct.result == 0) {
        output = outputStruct.bytes;
        return true;
    }
    return false;
}

std::optional<SmcKeyData> SmcController::readKey(std::string_view keyName) {
    SMCKey key = makeKey(keyName);
    SmcKeyData input{}, output{};

    // input.data8 = kSMCGetKeyInfo;
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