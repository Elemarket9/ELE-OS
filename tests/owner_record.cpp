// SPDX-License-Identifier: Apache-2.0
// Host checks for the owner record and the reset-surviving lock gate.
// The filesystem fixture is not a factory reset and not a handset.
#include "owner_record.h"
#include "reactivation_session.h"
#include "theft_lock.h"

#include <openssl/evp.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using namespace ele;

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static bool Contains(const uint8_t* hay, size_t hay_len, const uint8_t* needle, size_t needle_len) {
    if (needle_len == 0 || hay_len < needle_len) return false;
    for (size_t i = 0; i + needle_len <= hay_len; ++i) {
        if (std::memcmp(hay + i, needle, needle_len) == 0) return true;
    }
    return false;
}

static Signature Sign(const Bytes32& seed, const std::string& message) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed.data(), seed.size()),
        EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    Signature out{};
    size_t length = out.size();
    Require(key && ctx && EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1 &&
                EVP_DigestSign(ctx.get(), out.data(), &length,
                               reinterpret_cast<const uint8_t*>(message.data()), message.size()) ==
                    1 &&
                length == out.size(),
            "fixture failed to sign");
    return out;
}

static std::string TransferMessage(const Bytes32& device, const Bytes32& next, const Bytes32& nonce) {
    std::string out = "ELE-OS/transfer/v1";
    out.append(device.begin(), device.end());
    out.append(next.begin(), next.end());
    out.append(nonce.begin(), nonce.end());
    return out;
}

static void RefuseBrand(const char* text) {
    Require(std::strstr(text, "Pixel") == nullptr, "statement names a phone brand");
    Require(std::strstr(text, "shiba") == nullptr, "statement names a codename");
}

int main() {
    const char* notice = RecordPrototypeNotice();
    const char* warning = StrictRecoveryWarning();
    Require(std::strstr(notice, "Host prototype") != nullptr, "record notice");
    Require(std::strstr(warning, "no account") != nullptr, "warning must refuse an account reset");
    RefuseBrand(notice);
    RefuseBrand(warning);

    auto owner = GenerateOwnerKey();
    auto other = GenerateOwnerKey();
    Require(owner && other && owner->public_key != other->public_key, "owner keys");
    Bytes32 device{};
    device.fill(9);

    Require(!IssueRecord(device, *owner, false), "strict record written without opt-in");
    auto blob = IssueRecord(device, *owner, true);
    Require(blob && blob->size() == kOwnerRecordBytes, "record");
    Require(!Contains(blob->data(), blob->size(), owner->seed.data(), owner->seed.size()),
            "seed was copied into the record");

    OwnerKey mismatched = *owner;
    mismatched.public_key[0] ^= 1;
    Require(!IssueRecord(device, mismatched, true), "mismatched owner key accepted");

    auto loaded = LoadRecord(blob->data(), blob->size(), device, PlatformMarker::Absent);
    Require(loaded.status == RecordStatus::Enrolled && loaded.owner_public == owner->public_key,
            "enrolled record");
    Require(!FreshSetupAllowed(loaded.status), "enrolled phone treated as new");

    auto torn = *blob;
    torn[74] ^= 1;
    auto corrupt = LoadRecord(torn.data(), torn.size(), device, PlatformMarker::Absent);
    Require(corrupt.status == RecordStatus::FailClosed && !FreshSetupAllowed(corrupt.status),
            "torn record became a new phone");
    Require(LoadRecord(blob->data(), 1, device, PlatformMarker::Absent).status ==
                RecordStatus::FailClosed,
            "truncated record accepted");

    Bytes32 elsewhere = device;
    elsewhere[0] ^= 1;
    Require(LoadRecord(blob->data(), blob->size(), elsewhere, PlatformMarker::Present).status ==
                RecordStatus::FailClosed,
            "wrong device accepted");

    auto missing = LoadRecord(nullptr, 0, device, PlatformMarker::Absent);
    Require(missing.status == RecordStatus::Unenrolled && FreshSetupAllowed(missing.status),
            "never-enrolled phone blocked");
    auto wiped = LoadRecord(nullptr, 0, device, PlatformMarker::Present);
    Require(wiped.status == RecordStatus::FailClosed && !FreshSetupAllowed(wiped.status),
            "missing enrolled record became a new phone");

    Bytes32 nonce{};
    nonce.fill(4);
    auto authorization = Sign(owner->seed, TransferMessage(device, other->public_key, nonce));
    auto moved = TransferRecord(*blob, device, other->seed, nonce, authorization);
    Require(moved.has_value(), "transfer rejected");
    Require(*moved != *blob, "transfer rewrote the caller's blob in place");
    auto now = LoadRecord(moved->data(), moved->size(), device, PlatformMarker::Present);
    Require(now.status == RecordStatus::Enrolled && now.owner_public == other->public_key,
            "transfer kept the previous owner");
    Signature bad = authorization;
    bad[0] ^= 1;
    Require(!TransferRecord(*blob, device, other->seed, nonce, bad), "bad transfer accepted");

    fs::path root = fs::temp_directory_path() / "ele-os-owner-record-fixture";
    fs::remove_all(root);
    Require(fs::create_directories(root / "userdata"), "fixture userdata");
    auto record_path = root / "owner-record";
    {
        std::ofstream out(record_path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(blob->data()), blob->size());
    }
    std::ofstream(root / "userdata" / "settings") << "configured";
    Require(fs::remove_all(root / "userdata") == 2, "fixture userdata wipe");
    OwnerRecordBlob kept{};
    {
        std::ifstream in(record_path, std::ios::binary);
        Require(static_cast<bool>(in.read(reinterpret_cast<char*>(kept.data()), kept.size())),
                "record did not survive the fixture wipe");
    }
    auto after = LoadRecord(kept.data(), kept.size(), device, PlatformMarker::Present);
    Require(after.status == RecordStatus::Enrolled, "fixture record failed after userdata removal");
    Enrollment enrollment{after.device, after.owner_public};
    ReactivationSession session(device, enrollment);
    auto challenge = session.Begin();
    Require(challenge.has_value(), "enrolled record did not open a setup challenge");
    std::string proof_message = "ELE-OS/reactivate/v1";
    proof_message.append(device.begin(), device.end());
    proof_message.append(challenge->begin(), challenge->end());
    Require(session.Consume(Sign(owner->seed, proof_message)), "owner proof rejected");
    ReactivationSession stranger(device, Enrollment{device, other->public_key});
    auto other_challenge = stranger.Begin();
    Require(other_challenge.has_value(), "session");
    std::string stranger_message = "ELE-OS/reactivate/v1";
    stranger_message.append(device.begin(), device.end());
    stranger_message.append(other_challenge->begin(), other_challenge->end());
    Require(!stranger.Consume(Sign(owner->seed, stranger_message)), "wrong owner key passed");
    fs::remove_all(root);

    BoardObservations all{true, true, true, true};
    auto cuttlefish = AssessResetSurvivingLock(all, BoardClass::Cuttlefish);
    auto gsi = AssessResetSurvivingLock(all, BoardClass::TrebleGsi);
    Require(cuttlefish.result == ResetSurvivingLock::NotAvailable, "Cuttlefish lock claimed");
    Require(gsi.result == ResetSurvivingLock::NotAvailable, "GSI lock claimed");
    Require(std::strstr(cuttlefish.statement, "not a reset-surviving lock") != nullptr, "cuttlefish");
    Require(std::strstr(gsi.statement, "does not provide") != nullptr, "gsi");
    RefuseBrand(cuttlefish.statement);
    RefuseBrand(gsi.statement);
    Require(AssessResetSurvivingLock(CuttlefishObservations(), BoardClass::Specific).result ==
                ResetSurvivingLock::NotAvailable,
            "empty observations claimed a lock");
    BoardObservations missing_fastboot = all;
    missing_fastboot.locked_bootloader_rejects_erase_and_reflash = false;
    auto partial = AssessResetSurvivingLock(missing_fastboot, BoardClass::Specific);
    Require(partial.result == ResetSurvivingLock::NotAvailable, "partial hardware claimed a lock");
    auto reported = AssessResetSurvivingLock(all, BoardClass::Specific);
    Require(reported.result == ResetSurvivingLock::ReportedByCaller, "reported conditions");
    Require(std::strstr(reported.statement, "has not verified") != nullptr,
            "caller report treated as a hardware test");
    RefuseBrand(reported.statement);

    std::cout << notice << "\n"
              << "Record path owner-record survived removal of userdata. "
                 "This is a host fixture, not Cuttlefish and not a handset.\n"
              << cuttlefish.statement << "\n"
              << gsi.statement << "\n";
}
