#pragma once

#include "owner_proof.h"
#include <mutex>
#include <optional>

namespace ele {

// Construct one session per setup attempt inside the trusted setup service.
// Enrollment and hardware identity must come from trusted platform storage.
struct Enrollment {
    Bytes32 device;
    Bytes32 owner_key;
};

class ReactivationSession {
  public:
    ReactivationSession(const Bytes32& hardware_device,
                        const std::optional<Enrollment>& enrollment);
    std::optional<Bytes32> Begin();
    bool Consume(const Signature& proof);

  private:
    std::mutex mutex_;
    std::optional<Enrollment> enrollment_;
    std::optional<Bytes32> pending_;
};
}
