// SPDX-License-Identifier: Apache-2.0
#include "owner_record.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstring>
#include <memory>
#include <string>

namespace ele {
namespace {
constexpr uint8_t kVersion = 1;
constexpr uint8_t kStrictFlag = 0x01;
constexpr char kMagic[] = "ELEOSOR1";
constexpr char kEnroll[] = "ELE-OS/enroll/v1";
constexpr char kTransfer[] = "ELE-OS/transfer/v1";

static_assert(sizeof(kMagic) - 1 == 8, "owner record magic");
static_assert(8 + 1 + 1 + 32 + 32 + 64 == kOwnerRecordBytes, "owner record layout");

bool PublicFromSeed(const Bytes32& seed, Bytes32& out) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed.data(), seed.size()),
        EVP_PKEY_free);
    size_t size = out.size();
    return key && EVP_PKEY_get_raw_public_key(key.get(), out.data(), &size) == 1 &&
           size == out.size();
}

bool SignWithSeed(const Bytes32& seed, const uint8_t* message, size_t length, Signature& out) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed.data(), seed.size()),
        EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    size_t sig_len = out.size();
    return key && ctx &&
           EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1 &&
           EVP_DigestSign(ctx.get(), out.data(), &sig_len, message, length) == 1 &&
           sig_len == out.size();
}

bool VerifyWithKey(const Bytes32& public_key, const uint8_t* message, size_t length,
                   const Signature& proof) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, public_key.data(),
                                    public_key.size()),
        EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return key && ctx &&
           EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1 &&
           EVP_DigestVerify(ctx.get(), proof.data(), proof.size(), message, length) == 1;
}

void Append(std::string& out, const uint8_t* data, size_t size) {
    out.append(reinterpret_cast<const char*>(data), size);
}

LoadedRecord Closed() { return {RecordStatus::FailClosed, {}, {}}; }
}

const char* RecordPrototypeNotice() {
    return "Host prototype of an owner record. No handset stores it, and the "
           "signature does not stop someone who can replace the whole blob.";
}

const char* StrictRecoveryWarning() {
    return "If you lose this key, a wipe leaves the phone unable to finish setup. "
           "There is no account and no project escrow.";
}

std::optional<OwnerKey> GenerateOwnerKey() {
    OwnerKey owner{};
    if (RAND_bytes(owner.seed.data(), owner.seed.size()) != 1 ||
        !PublicFromSeed(owner.seed, owner.public_key)) {
        return std::nullopt;
    }
    return owner;
}

std::optional<OwnerRecordBlob> IssueRecord(const Bytes32& hardware_device, const OwnerKey& owner,
                                           bool owner_accepts_strict_recovery) {
    if (!owner_accepts_strict_recovery) return std::nullopt;
    Bytes32 derived{};
    if (!PublicFromSeed(owner.seed, derived) || derived != owner.public_key) return std::nullopt;

    OwnerRecordBlob blob{};
    std::memcpy(blob.data(), kMagic, 8);
    blob[8] = kVersion;
    blob[9] = kStrictFlag;
    std::memcpy(blob.data() + 10, hardware_device.data(), hardware_device.size());
    std::memcpy(blob.data() + 42, owner.public_key.data(), owner.public_key.size());

    std::string message(kEnroll);
    message.push_back(static_cast<char>(kVersion));
    message.push_back(static_cast<char>(kStrictFlag));
    Append(message, hardware_device.data(), hardware_device.size());
    Append(message, owner.public_key.data(), owner.public_key.size());
    Signature signature{};
    if (!SignWithSeed(owner.seed, reinterpret_cast<const uint8_t*>(message.data()), message.size(),
                      signature)) {
        return std::nullopt;
    }
    std::memcpy(blob.data() + 74, signature.data(), signature.size());
    return blob;
}

LoadedRecord LoadRecord(const uint8_t* bytes, std::size_t size, const Bytes32& hardware_device,
                        PlatformMarker marker) {
    if (bytes == nullptr || size == 0) {
        if (bytes == nullptr && size != 0) return Closed();
        return marker == PlatformMarker::Present ? Closed()
                                                 : LoadedRecord{RecordStatus::Unenrolled, {}, {}};
    }
    if (size != kOwnerRecordBytes || std::memcmp(bytes, kMagic, 8) != 0 || bytes[8] != kVersion ||
        bytes[9] != kStrictFlag) {
        return Closed();
    }
    Bytes32 device{}, owner{};
    Signature signature{};
    std::memcpy(device.data(), bytes + 10, device.size());
    std::memcpy(owner.data(), bytes + 42, owner.size());
    std::memcpy(signature.data(), bytes + 74, signature.size());

    std::string message(kEnroll);
    message.push_back(static_cast<char>(bytes[8]));
    message.push_back(static_cast<char>(bytes[9]));
    Append(message, device.data(), device.size());
    Append(message, owner.data(), owner.size());
    if (!VerifyWithKey(owner, reinterpret_cast<const uint8_t*>(message.data()), message.size(),
                       signature) ||
        device != hardware_device) {
        return Closed();
    }
    return {RecordStatus::Enrolled, device, owner};
}

bool FreshSetupAllowed(RecordStatus status) { return status == RecordStatus::Unenrolled; }

std::optional<OwnerRecordBlob> TransferRecord(const OwnerRecordBlob& current,
                                              const Bytes32& hardware_device,
                                              const Bytes32& new_owner_seed, const Bytes32& nonce,
                                              const Signature& authorization) {
    const LoadedRecord loaded =
        LoadRecord(current.data(), current.size(), hardware_device, PlatformMarker::Present);
    if (loaded.status != RecordStatus::Enrolled) return std::nullopt;
    OwnerKey next{};
    next.seed = new_owner_seed;
    if (!PublicFromSeed(new_owner_seed, next.public_key)) return std::nullopt;

    std::string message(kTransfer);
    Append(message, hardware_device.data(), hardware_device.size());
    Append(message, next.public_key.data(), next.public_key.size());
    Append(message, nonce.data(), nonce.size());
    if (!VerifyWithKey(loaded.owner_public, reinterpret_cast<const uint8_t*>(message.data()),
                       message.size(), authorization)) {
        return std::nullopt;
    }
    return IssueRecord(hardware_device, next, true);
}
}
