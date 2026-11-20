// Host integration exercise. The caller provides a fresh temporary directory.
#include "reactivation_session.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <openssl/evp.h>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
using namespace ele;

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static Signature Sign(EVP_PKEY* key, const Bytes32& device, const Bytes32& nonce) {
    std::string bytes = "ELE-OS/reactivate/v1";
    bytes.append(device.begin(), device.end());
    bytes.append(nonce.begin(), nonce.end());
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    Signature out{};
    size_t length = out.size();
    Require(ctx && EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key) == 1 &&
            EVP_DigestSign(ctx.get(), out.data(), &length,
                           reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()) == 1 &&
            length == out.size(), "test owner failed to sign");
    return out;
}

// Test fixture only. Hardware must authenticate its enrollment record.
static std::optional<Enrollment> LoadFixture(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    Enrollment record{};
    if (!in.read(reinterpret_cast<char*>(record.device.data()), record.device.size()) ||
        !in.read(reinterpret_cast<char*>(record.owner_key.data()), record.owner_key.size()) ||
        in.peek() != std::char_traits<char>::eof()) return std::nullopt;
    return record;
}

int main(int argc, char** argv) {
    Require(argc == 2, "supply fresh fixture directory");
    fs::path root(argv[1]);
    Require(fs::create_directory(root / "userdata"), "fixture must be fresh");
    auto file = root / "enrollment";
    Bytes32 device{}, seed{}, owner{};
    device.fill(42);
    seed.fill(13);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed.data(), seed.size()),
        EVP_PKEY_free);
    size_t size = owner.size();
    Require(key && EVP_PKEY_get_raw_public_key(key.get(), owner.data(), &size) == 1, "fixture key");
    {
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(device.data()), device.size());
        out.write(reinterpret_cast<const char*>(owner.data()), owner.size());
        out.close();
        Require(out.good(), "fixture enrollment write");
    }
    std::ofstream(root / "userdata" / "settings") << "configured";
    Signature stale{};
    {
        ReactivationSession before_wipe(device, LoadFixture(file));
        stale = Sign(key.get(), device, before_wipe.Begin().value());
    }
    Require(fs::remove_all(root / "userdata") == 2, "fixture userdata wipe");
    ReactivationSession setup(device, LoadFixture(file));
    Require(!setup.Consume(stale), "proof accepted before challenge");
    auto replaced = setup.Begin().value();
    auto current = setup.Begin().value();
    Require(!setup.Consume(stale), "old session accepted");
    Require(!setup.Consume(Sign(key.get(), device, replaced)), "replaced challenge accepted");
    auto proof = Sign(key.get(), device, current);
    std::atomic<int> grants{0};
    auto respond = [&] { if (setup.Consume(proof)) ++grants; };
    std::thread a(respond), b(respond);
    a.join();
    b.join();
    Require(grants == 1 && !setup.Consume(proof), "expected exactly one grant");
    Bytes32 other_device{};
    Require(!ReactivationSession(other_device, LoadFixture(file)).Begin(), "wrong device accepted");
    fs::resize_file(file, 1);
    Require(!ReactivationSession(device, LoadFixture(file)).Begin(), "truncated record accepted");
    fs::remove(file);
    Require(!ReactivationSession(device, LoadFixture(file)).Begin(), "missing record accepted");
    std::cout << "Reset fixture, device binding, rotation, restart and concurrent replay checks passed\n";
}
