// SPDX-License-Identifier: Apache-2.0
#include "owner_command.h"

#include <openssl/evp.h>

#include <algorithm>
#include <memory>
#include <string>

namespace ele {
namespace {
constexpr int kMaxNonces = 64;

const char* Purpose(Command command) {
    switch (command) {
    case Command::Locate:
        return "ELE-OS/locate/v1";
    case Command::Lock:
        return "ELE-OS/lock/v1";
    case Command::Wipe:
        return "ELE-OS/wipe/v1";
    }
    return nullptr;
}
}

const char* CommandPrototypeNotice() {
    return "Host prototype. The owner key authorizes locate, lock, wipe, and "
           "transfer. No handset was locked, wiped, or located, and no account "
           "is contacted.";
}

CommandChannel::CommandChannel(Bytes32 device, Bytes32 owner_key)
    : device_(device), owner_key_(owner_key) {}

void CommandChannel::EnableCommands() {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_enabled_ = true;
}

bool CommandChannel::EnableLocate() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!commands_enabled_) return false;
    locate_enabled_ = true;
    return true;
}

void CommandChannel::DisableCommands() {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_enabled_ = false;
    locate_enabled_ = false;
}

bool CommandChannel::commands_enabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return commands_enabled_;
}

bool CommandChannel::locate_enabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return locate_enabled_;
}

bool CommandChannel::credential_required() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return credential_required_;
}

bool CommandChannel::wipe_requested() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return wipe_requested_;
}

Bytes32 CommandChannel::owner_key() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return owner_key_;
}

bool CommandChannel::Verify(const char* purpose, const uint8_t* extra, size_t extra_len,
                            const Bytes32& nonce, const Signature& proof) const {
    if (!purpose) return false;
    const size_t purpose_len = std::char_traits<char>::length(purpose);
    std::array<uint8_t, 128> message{};
    if (purpose_len + device_.size() + extra_len + nonce.size() > message.size()) return false;
    auto next = std::copy_n(purpose, purpose_len, message.begin());
    next = std::copy(device_.begin(), device_.end(), next);
    if (extra_len) next = std::copy_n(extra, extra_len, next);
    std::copy(nonce.begin(), nonce.end(), next);
    const size_t message_len = purpose_len + device_.size() + extra_len + nonce.size();

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, owner_key_.data(),
                                    owner_key_.size()),
        EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return key && ctx &&
           EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1 &&
           EVP_DigestVerify(ctx.get(), proof.data(), proof.size(), message.data(),
                            message_len) == 1;
}

bool CommandChannel::Remember(const Bytes32& nonce) {
    if (seen_.find(nonce) != seen_.end()) return false;
    if (static_cast<int>(seen_.size()) >= kMaxNonces) return false;
    seen_.insert(nonce);
    return true;
}

bool CommandChannel::Accept(Command command, const Bytes32& nonce, const Signature& proof) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!commands_enabled_) return false;
    if (command == Command::Locate && !locate_enabled_) return false;
    if (!Verify(Purpose(command), nullptr, 0, nonce, proof)) return false;
    if (!Remember(nonce)) return false;
    if (command == Command::Lock) credential_required_ = true;
    if (command == Command::Wipe) {
        // Userdata would be erased by a device service. The owner key stays.
        // The lockscreen credential is gone with userdata; reactivation is the
        // remaining gate, and this prototype does not perform that wipe.
        wipe_requested_ = true;
        credential_required_ = false;
    }
    return true;
}

bool CommandChannel::AcceptTransfer(const Bytes32& new_owner, const Bytes32& nonce,
                                    const Signature& proof) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!commands_enabled_) return false;
    if (!Verify("ELE-OS/transfer/v1", new_owner.data(), new_owner.size(), nonce, proof)) {
        return false;
    }
    if (!Remember(nonce)) return false;
    owner_key_ = new_owner;
    return true;
}
}
