#pragma once

#include <array>
#include <cstdint>

namespace ele {
using Bytes32 = std::array<uint8_t, 32>;
using Signature = std::array<uint8_t, 64>;

// Inputs come from trusted enrollment and the current activation session.
// The caller must consume the challenge atomically before enabling setup.
bool VerifyReactivation(const Bytes32& enrolled_key, const Bytes32& device_id,
                        const Bytes32& challenge, const Signature& proof);
}
