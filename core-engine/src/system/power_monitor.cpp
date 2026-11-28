// folder location: core-engine/src/system/power_monitor.cpp
#include "system/power_monitor.hpp"
// [ADDED] Platform-specific power headers: IOKit for macOS , Win32 API for Windows 10/11 Bootcamp.
#ifdef __APPLE__
    #include <CoreFoundation/CoreFoundation.h>
    #include <IOKit/ps/IOPowerSources.h>
    #include <IOKit/ps/IOPSKeys.h>
#elif defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

namespace macms::system {

bool PowerMonitor::isOnACPower() const {
#ifdef __APPLE__
    // [ADDED] macOS path: Query IOKit power source dictionary to check if MagSafe/USB-C charger is active.
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (!info) return false; // [!] Failsafe: UNKNOWN STATE TREATED AS Battery.

    // [+] "GET" rule: THE returned string is OWNED by `info`, so compare BEFORE releasing it.
    CFStringRef source = IOPSGetProvidingPowerSourceType(info);
    const bool onAC = source != nullptr && CFStringCompare(source, CFSTR(kIOPMACPowerKey), 0) == kCFCompareEqualTo;

    CFRelease(info);
    return onAC;

#elif defined(_WIN32)
    // [ADDED] Windows 10 path: Query Win32 battery/charger state without needing admin privileges.
    SYSTEM_POWER_STATUS status{};
    if (!GetSystemPowerStatus(&status)) {
        // [ADDED] Failsafe: If Windows API fails, assume battery power so Maximum profile stays locked.
        return false;
    }

    // [ADDED] ACLineStatus == 1 means plugged into AC wall power; 0 = battery, 255 = unknown.
    return status.ACLineStatus == 1;

#else
    // [ADDED] Fallback for unsupported platforms: treat as battery power for thermal safety.
    return false;
#endif
}

} // end of namespace macms::system