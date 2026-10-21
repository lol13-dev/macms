// folder location: core-engine/src/system/power_monitor.cpp
#include "system/power_monitor.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

namespace macms::system {

bool PowerMonitor::isOnACPower() const {
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (!info) return false; // [!] Failsafe: UNKNOWN STATE TREATED AS Battery.

    // [+] "GET" rule: THE returned string is OWNED by `info`, so compare BEFORE releasing it.
    CFStringRef source = IOPSGetProvidingPowerSourceType(info);
    const bool onAC = source != nullptr && CFStringCompare(source, CFSTR(kIOPMACPowerKey), 0) == kCFCompareEqualTo;

    CFRelease(info);
    return onAC;
}

} // end of namespace macms::system