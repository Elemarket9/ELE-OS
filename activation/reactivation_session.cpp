#include "reactivation_session.h"
#include <openssl/rand.h>

namespace ele {
ReactivationSession::ReactivationSession(const Bytes32& hardware_device,
                                         const std::optional<Enrollment>& enrollment) {
    if (enrollment && enrollment->device == hardware_device) {
        enrollment_ = enrollment;
    }
}

std::optional<Bytes32> ReactivationSession::Begin() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.reset();
    Bytes32 challenge{};
    if (!enrollment_ || RAND_bytes(challenge.data(), challenge.size()) != 1) {
        return std::nullopt;
    }
    pending_ = challenge;
    return pending_;
}

bool ReactivationSession::Consume(const Signature& proof) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pending_ || !VerifyReactivation(enrollment_->owner_key, enrollment_->device,
                                         *pending_, proof)) {
        return false;
    }
    pending_.reset();
    return true;
}
}
