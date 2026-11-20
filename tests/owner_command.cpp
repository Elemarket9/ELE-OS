// SPDX-License-Identifier: Apache-2.0
// Host checks for the command prototype. No phone is locked, wiped, or located.
#include "owner_command.h"

#include <openssl/evp.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace fs = std::filesystem;
using namespace ele;

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static Signature Sign(EVP_PKEY* key, const std::string& message) {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    Signature out{};
    size_t length = out.size();
    Require(ctx && EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key) == 1 &&
                EVP_DigestSign(ctx.get(), out.data(), &length,
                               reinterpret_cast<const uint8_t*>(message.data()),
                               message.size()) == 1 &&
                length == out.size(),
            "owner fixture failed to sign");
    return out;
}

static std::string Message(const char* purpose, const Bytes32& device, const Bytes32& extra,
                           bool with_extra, const Bytes32& nonce) {
    std::string out(purpose);
    out.append(device.begin(), device.end());
    if (with_extra) out.append(extra.begin(), extra.end());
    out.append(nonce.begin(), nonce.end());
    return out;
}

static EVP_PKEY* KeyFromSeed(const Bytes32& seed) {
    return EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed.data(), seed.size());
}

static Bytes32 PublicOf(EVP_PKEY* key) {
    Bytes32 out{};
    size_t size = out.size();
    Require(EVP_PKEY_get_raw_public_key(key, out.data(), &size) == 1 && size == out.size(),
            "public key");
    return out;
}

int main() {
    const char* notice = CommandPrototypeNotice();
    Require(std::strstr(notice, "Host prototype") != nullptr, "prototype notice");
    Require(std::strstr(notice, "No handset") != nullptr, "notice must refuse a handset claim");
    Require(std::strstr(notice, "Pixel") == nullptr, "notice names a phone brand");

    Bytes32 device{}, seed{}, other_seed{};
    device.fill(9);
    seed.fill(13);
    other_seed.fill(4);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> owner(KeyFromSeed(seed), EVP_PKEY_free);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> stranger(KeyFromSeed(other_seed),
                                                                EVP_PKEY_free);
    Require(owner && stranger, "fixture keys");
    const Bytes32 owner_key = PublicOf(owner.get());
    const Bytes32 stranger_key = PublicOf(stranger.get());
    Bytes32 nonce{}, second{};
    nonce.fill(7);
    second.fill(8);

    CommandChannel channel(device, owner_key);
    auto lock_proof = Sign(owner.get(), Message("ELE-OS/lock/v1", device, {}, false, nonce));
    Require(!channel.Accept(Command::Lock, nonce, lock_proof), "lock accepted before opt-in");
    Require(!channel.EnableLocate(), "locate enabled before the command channel");
    Require(!channel.locate_enabled() && !channel.commands_enabled(), "defaults are off");

    channel.EnableCommands();
    Require(!channel.Accept(Command::Locate, second,
                            Sign(owner.get(), Message("ELE-OS/locate/v1", device, {}, false, second))),
            "locate accepted before the owner enabled it");
    Require(channel.Accept(Command::Lock, nonce, lock_proof), "lock rejected after opt-in");
    Require(channel.credential_required(), "lock did not demand the local credential");
    Require(!channel.Accept(Command::Lock, nonce, lock_proof), "lock nonce replayed");

    Bytes32 wrong_device = device;
    wrong_device[0] ^= 1;
    Bytes32 other_nonce{};
    other_nonce.fill(3);
    Require(!channel.Accept(Command::Lock, other_nonce,
                            Sign(owner.get(),
                                 Message("ELE-OS/lock/v1", wrong_device, {}, false, other_nonce))),
            "wrong device accepted");
    Require(!channel.Accept(Command::Wipe, other_nonce,
                            Sign(owner.get(), Message("ELE-OS/reactivate/v1", device, {}, false,
                                                      other_nonce))),
            "reactivation signature accepted as a wipe");
    Require(!channel.Accept(Command::Lock, other_nonce,
                            Sign(stranger.get(),
                                 Message("ELE-OS/lock/v1", device, {}, false, other_nonce))),
            "wrong key accepted");

    auto wipe_proof = Sign(owner.get(), Message("ELE-OS/wipe/v1", device, {}, false, other_nonce));
    Require(!channel.Accept(Command::Lock, other_nonce, wipe_proof), "wipe signature accepted as lock");
    Require(channel.Accept(Command::Wipe, other_nonce, wipe_proof), "wipe rejected");
    Require(channel.wipe_requested() && !channel.credential_required(),
            "wipe dropped the owner key or kept the old lockscreen flag");
    Require(channel.owner_key() == owner_key, "wipe replaced the owner key");

    fs::path root = fs::temp_directory_path() / "ele-os-command-wipe-fixture";
    fs::remove_all(root);
    Require(fs::create_directories(root / "userdata"), "fixture userdata");
    auto record = root / "owner-record";
    {
        std::ofstream out(record, std::ios::binary);
        out << "owner-record-stays";
    }
    std::ofstream(root / "userdata" / "settings") << "photos";
    Require(fs::remove_all(root / "userdata") == 2, "fixture userdata erase");
    std::ifstream kept(record);
    std::string record_bytes;
    std::getline(kept, record_bytes);
    Require(record_bytes == "owner-record-stays", "owner record was inside the wiped directory");
    fs::remove_all(root);

    Require(channel.EnableLocate(), "owner could not enable locate");
    Bytes32 locate_nonce{};
    locate_nonce.fill(11);
    auto locate_proof =
        Sign(owner.get(), Message("ELE-OS/locate/v1", device, {}, false, locate_nonce));
    std::atomic<int> grants{0};
    auto respond = [&] {
        if (channel.Accept(Command::Locate, locate_nonce, locate_proof)) ++grants;
    };
    std::thread a(respond), b(respond);
    a.join();
    b.join();
    Require(grants == 1, "locate granted twice");
    Require(!channel.Accept(Command::Locate, locate_nonce, locate_proof), "locate replayed");

    Bytes32 transfer_nonce{};
    transfer_nonce.fill(12);
    auto transfer = Sign(owner.get(), Message("ELE-OS/transfer/v1", device, stranger_key, true,
                                              transfer_nonce));
    Bytes32 swapped = stranger_key;
    swapped[1] ^= 1;
    Require(!channel.AcceptTransfer(swapped, transfer_nonce, transfer),
            "transfer accepted for a different new key");
    Require(channel.AcceptTransfer(stranger_key, transfer_nonce, transfer), "transfer rejected");
    Require(channel.owner_key() == stranger_key, "owner key was not replaced");
    Bytes32 after{};
    after.fill(13);
    Require(!channel.Accept(Command::Lock, after,
                            Sign(owner.get(), Message("ELE-OS/lock/v1", device, {}, false, after))),
            "previous owner still authorized");
    Require(channel.Accept(Command::Lock, after,
                           Sign(stranger.get(), Message("ELE-OS/lock/v1", device, {}, false, after))),
            "new owner rejected");

    channel.DisableCommands();
    Require(!channel.locate_enabled(), "locate stayed on after the channel was disabled");
    Require(channel.credential_required(), "disabling the channel cleared a lock");
    Bytes32 late{};
    late.fill(14);
    Require(!channel.Accept(Command::Lock, late,
                            Sign(stranger.get(), Message("ELE-OS/lock/v1", device, {}, false, late))),
            "command accepted while the channel is off");

    CommandChannel capped(device, owner_key);
    capped.EnableCommands();
    for (int i = 0; i < 64; ++i) {
        Bytes32 n{};
        n[0] = static_cast<uint8_t>(i);
        n[1] = 1;
        Require(capped.Accept(Command::Lock, n,
                              Sign(owner.get(), Message("ELE-OS/lock/v1", device, {}, false, n))),
                "nonce capacity rejected a fresh command");
    }
    Bytes32 overflow{};
    overflow[1] = 2;
    Require(!capped.Accept(Command::Lock, overflow,
                           Sign(owner.get(), Message("ELE-OS/lock/v1", device, {}, false, overflow))),
            "nonce memory dropped an old nonce to make room");

    std::cout << CommandPrototypeNotice() << "\n"
              << "Opt-in, replay, wrong key, wrong device, purpose separation, transfer, "
                 "and userdata-versus-record fixture checks passed\n";
}
