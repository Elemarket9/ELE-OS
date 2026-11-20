// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "owner_proof.h"

#include <mutex>
#include <set>

namespace ele {

// Host prototype of the owner command channel. The phone's owner holds the
// Ed25519 key. There is no project account and no server in this library.
// Locate, lock, and wipe here verify a signature and an opt-in flag. They do
// not read a location provider, lock a screen, or erase a handset.
//
// Signed bytes, with no trailing NUL on the purpose:
//   "ELE-OS/locate/v1"  || device id (32) || nonce (32)
//   "ELE-OS/lock/v1"    || device id (32) || nonce (32)
//   "ELE-OS/wipe/v1"    || device id (32) || nonce (32)
//   "ELE-OS/transfer/v1"|| device id (32) || new owner public key (32) || nonce (32)
// A reactivation signature uses a different purpose and must not pass.
// Remembered nonces live in this process only. A restart would forget them.

enum class Command { Locate, Lock, Wipe };

const char* CommandPrototypeNotice();

class CommandChannel {
  public:
    CommandChannel(Bytes32 device, Bytes32 owner_key);

    void EnableCommands();
    // Local opt-in only. Returns false while the command channel is off.
    // A signed command cannot turn locate on.
    bool EnableLocate();
    void DisableCommands();

    bool commands_enabled() const;
    bool locate_enabled() const;
    bool credential_required() const;
    bool wipe_requested() const;
    Bytes32 owner_key() const;

    bool Accept(Command command, const Bytes32& nonce, const Signature& proof);
    bool AcceptTransfer(const Bytes32& new_owner, const Bytes32& nonce,
                        const Signature& proof);

  private:
    bool Verify(const char* purpose, const uint8_t* extra, size_t extra_len,
                const Bytes32& nonce, const Signature& proof) const;
    bool Remember(const Bytes32& nonce);

    mutable std::mutex mutex_;
    Bytes32 device_;
    Bytes32 owner_key_;
    bool commands_enabled_ = false;
    bool locate_enabled_ = false;
    bool credential_required_ = false;
    bool wipe_requested_ = false;
    std::set<Bytes32> seen_;
};

}
