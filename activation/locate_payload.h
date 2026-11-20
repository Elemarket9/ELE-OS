// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "owner_proof.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ele {

// Host prototype. The phone's owner holds one Ed25519 key. Location bytes are
// sealed to the X25519 key derived from that public key. Nothing here reads a
// location provider, opens a socket, or chooses an endpoint. The caller passes
// an endpoint they already configured; this function does not contact it.
// Locate stays off unless owner_enabled_locate is true. There is no check-in
// interval. A sealed blob is not evidence that a phone was located.

const char* LocatePrototypeNotice();

// Returns nullopt if the Ed25519 public key is not a canonical curve point.
std::optional<Bytes32> Ed25519PublicToX25519(const Bytes32& ed25519_public);

// Plaintext is at most 512 bytes and must be non-empty. owner_endpoint must
// be a non-empty owner-chosen address. It is not written into the blob.
std::optional<std::vector<uint8_t>> SealLocation(const Bytes32& owner_ed25519_public,
                                                 bool owner_enabled_locate,
                                                 const char* owner_endpoint,
                                                 const uint8_t* plaintext, std::size_t plaintext_len);

// owner_seed is the Ed25519 seed the owner holds. A wrong seed or a damaged
// blob returns nullopt.
std::optional<std::vector<uint8_t>> OpenLocation(const Bytes32& owner_seed, const uint8_t* sealed,
                                                 std::size_t sealed_len);
}
