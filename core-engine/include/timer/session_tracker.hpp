// folder structure: core-engine/include/timer/session_tracker.hpp

#pragma once

#include "system/profile_manager.hpp"
#include <chrono>

namespace macms::timer {

class SessionTracker{
public:
    explicit SessionTracker(system::ProfileManager& profileManager);

    // Special Destructor.
    ~SessionTracker() = default;

    SessionTracker(const SessionTracker&) = delete;
    SessionTracker& operator=(const SessionTracker&) = delete;

    // CALLED every second in the main loop to EVALUATE session duration.
    void update();

    // DATA getters for the React UI payload.
    bool isSessionActive() const;
    int getActiveDurationMinutes() const;

private:
    system::ProfileManager& profileManager_;
    bool tracking_{false};
    std::chrono::steady_clock::time_point sessionStartTime_;

    // NOTE SAFETY LIMIT: 3 HOURS before auto-downshifting to PROTECT older logic boards.
    static constexpr std::chrono::hours kMaxSessionDuration{3};
};

} // end of namespace macms::timer