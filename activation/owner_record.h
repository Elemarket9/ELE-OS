// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "owner_proof.h"

#include <array>
#include <cstddef>
#include <optional>

namespace ele {

// Host prototype of the owner record. The phone's owner holds the Ed25519
// seed. This library does not keep a copy, and it has no account to fall
// back on. The blob signature stops a partial edit. It does not stop a
// wholesale replacement: the public key is inside the blob. Hardware still
// has to authenticate the stored bytes. No handset stores this record.
//
// Blob (138 bytes): "ELEOSOR1" || version 1 || flags 0x01 || device (32)
// || owner public key (32) || Ed25519 signature (64).
// Signed bytes: "ELE-OS/enroll/v1" || version || flags || device || public key.
//
// Transfer signed bytes, authorized by the current owner:
// "ELE-OS/transfer/v1" || device || new public key || nonce.
// This function does not remember nonces. The caller must reject a replay.

constexpr std::size_t kOwnerRecordBytes = 138;
using OwnerRecordBlob = std::array<uint8_t, kOwnerRecordBytes>;

struct OwnerKey {
    Bytes32 seed;
    Bytes32 public_key;
};

enum class PlatformMarker { Absent, Present };
enum class RecordStatus { Unenrolled, Enrolled, FailClosed };

struct LoadedRecord {
    RecordStatus status;
    Bytes32 device{};
    Bytes32 owner_public{};
};

const char* RecordPrototypeNotice();
const char* StrictRecoveryWarning();

std::optional<OwnerKey> GenerateOwnerKey();

// Writes a strict record only when the owner accepts the warning. A false
// opt-in returns no blob, which leaves a never-enrolled phone unenrolled.
std::optional<OwnerRecordBlob> IssueRecord(const Bytes32& hardware_device, const OwnerKey& owner,
                                           bool owner_accepts_strict_recovery);

LoadedRecord LoadRecord(const uint8_t* bytes, std::size_t size, const Bytes32& hardware_device,
                        PlatformMarker marker);

bool FreshSetupAllowed(RecordStatus status);

std::optional<OwnerRecordBlob> TransferRecord(const OwnerRecordBlob& current,
                                              const Bytes32& hardware_device,
                                              const Bytes32& new_owner_seed, const Bytes32& nonce,
                                              const Signature& authorization);
}
