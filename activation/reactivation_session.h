#pragma once

#include "owner_proof.h"
#include <chrono>
#include <mutex>
#include <optional>

namespace ele {

// Construct one session per setup attempt inside the trusted setup service.
// Enrollment and hardware identity must come from trusted platform storage.
// Maintain one session for the service lifetime; service restarts reset its
// in-memory throttling. Cancel when setup is backgrounded or suspended.
struct Enrollment {
    Bytes32 device;
    Bytes32 owner_key;
};

class ReactivationSession {
  public:
    using Clock = std::chrono::steady_clock;
    using NowFunction = Clock::time_point (*)() noexcept;
    static constexpr std::chrono::seconds kChallengeLifetime{120};
    static constexpr std::chrono::seconds kCooldown{60};
    static constexpr unsigned kFailureLimit = 5;

    ReactivationSession(const Bytes32& hardware_device,
                        const std::optional<Enrollment>& enrollment,
                        NowFunction now = &Clock::now);
    std::optional<Bytes32> Begin();
    bool Consume(const Signature& proof);
    void Cancel();

  private:
    // Called only with mutex_ held.
    bool IsThrottled(Clock::time_point now);

    std::mutex mutex_;
    std::optional<Enrollment> enrollment_;
    std::optional<Bytes32> pending_;
    Clock::time_point issued_{};
    std::optional<Clock::time_point> blocked_since_;
    unsigned failures_ = 0;
    NowFunction now_;
};
}
