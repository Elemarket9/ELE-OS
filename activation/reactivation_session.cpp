#include "reactivation_session.h"
#include <openssl/rand.h>

namespace ele {
ReactivationSession::ReactivationSession(const Bytes32& hardware_device,
                                         const std::optional<Enrollment>& enrollment,
                                         NowFunction now)
    : now_(now ? now : &Clock::now) {
    if (enrollment && enrollment->device == hardware_device) {
        enrollment_ = enrollment;
    }
}

bool ReactivationSession::IsThrottled(Clock::time_point now) {
    if (!blocked_since_) return false;
    if (now < *blocked_since_ || now - *blocked_since_ < kCooldown) return true;
    blocked_since_.reset();
    failures_ = 0;
    return false;
}

std::optional<Bytes32> ReactivationSession::Begin() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.reset();
    const auto now = now_();
    if (!enrollment_ || IsThrottled(now)) return std::nullopt;
    Bytes32 challenge{};
    if (RAND_bytes(challenge.data(), challenge.size()) != 1) return std::nullopt;
    pending_ = challenge;
    issued_ = now;
    return pending_;
}

bool ReactivationSession::Consume(const Signature& proof) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = now_();
    if (IsThrottled(now) || !pending_ || !enrollment_) return false;
    if (now < issued_ || now - issued_ >= kChallengeLifetime) {
        pending_.reset();
        return false;
    }
    if (!VerifyReactivation(enrollment_->owner_key, enrollment_->device, *pending_, proof)) {
        if (++failures_ >= kFailureLimit) {
            blocked_since_ = now_();
            pending_.reset();
        }
        return false;
    }
    pending_.reset();
    failures_ = 0;
    return true;
}

void ReactivationSession::Cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.reset();
    // Cancellation and nonce rotation do not reset failed attempts.
}
}
