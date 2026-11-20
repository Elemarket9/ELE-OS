// SPDX-License-Identifier: Apache-2.0
// Host checks for the sealed location prototype. No radio and no endpoint.
#include "locate_payload.h"

#include <openssl/evp.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ele;

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static int Nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static Bytes32 Hex32(const char* hex) {
    Bytes32 out{};
    for (int i = 0; i < 32; ++i) {
        int hi = Nibble(hex[i * 2]);
        int lo = Nibble(hex[i * 2 + 1]);
        Require(hi >= 0 && lo >= 0, "hex");
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return out;
}

static bool Contains(const std::vector<uint8_t>& hay, const std::string& needle) {
    if (needle.empty() || hay.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
        if (std::memcmp(hay.data() + i, needle.data(), needle.size()) == 0) return true;
    }
    return false;
}

static Bytes32 PublicFromSeed(const Bytes32& seed) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed.data(), seed.size()),
        EVP_PKEY_free);
    Bytes32 out{};
    size_t size = out.size();
    Require(key && EVP_PKEY_get_raw_public_key(key.get(), out.data(), &size) == 1 && size == out.size(),
            "ed25519 public key");
    return out;
}

int main() {
    const char* notice = LocatePrototypeNotice();
    Require(std::strstr(notice, "Host prototype") != nullptr, "notice");
    Require(std::strstr(notice, "No handset") != nullptr, "notice must refuse a handset claim");
    Require(std::strstr(notice, "Pixel") == nullptr, "notice names a phone brand");

    auto rfc = Ed25519PublicToX25519(
        Hex32("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a"));
    Require(rfc.has_value(), "RFC Ed25519 point rejected");
    Require(*rfc == Hex32("d85e07ec22b0ad881537c2f44d662d1a143cf830c57aca4305d85c7a90f6b62e"),
            "Ed25519 to X25519 conversion mismatch");

    Bytes32 seed{};
    seed.fill(13);
    Bytes32 owner = PublicFromSeed(seed);
    Require(owner == Hex32("91a28a0b74381593a4d9469579208926afc8ad82c8839b7644359b9eba9a4b3a"),
            "fixture public key");
    auto converted = Ed25519PublicToX25519(owner);
    Require(converted.has_value() &&
                *converted == Hex32("3839cb8ea7366ffe895153fa37ac900e95c1f2fb367b12b91705c4605336d268"),
            "seed 13 conversion mismatch");

    const char* endpoint = "https://owner.example/ele-recovery";
    const std::string place = "47.000,-122.000";
    Require(!SealLocation(owner, false, endpoint, reinterpret_cast<const uint8_t*>(place.data()),
                          place.size()),
            "locate sealed while disabled");
    Require(!SealLocation(owner, true, "", reinterpret_cast<const uint8_t*>(place.data()), place.size()),
            "empty endpoint accepted");
    Require(!SealLocation(owner, true, endpoint, reinterpret_cast<const uint8_t*>(place.data()), 0),
            "empty location accepted");
    std::vector<uint8_t> huge(513, 1);
    Require(!SealLocation(owner, true, endpoint, huge.data(), huge.size()), "oversized location accepted");

    auto sealed = SealLocation(owner, true, endpoint, reinterpret_cast<const uint8_t*>(place.data()),
                               place.size());
    Require(sealed.has_value(), "seal failed");
    Require(!Contains(*sealed, place), "plaintext visible in the sealed blob");
    Require(!Contains(*sealed, endpoint), "endpoint copied into the sealed blob");
    auto opened = OpenLocation(seed, sealed->data(), sealed->size());
    Require(opened.has_value() && opened->size() == place.size() &&
                std::memcmp(opened->data(), place.data(), place.size()) == 0,
            "owner could not open the sealed location");

    Bytes32 wrong = seed;
    wrong[0] ^= 1;
    Require(!OpenLocation(wrong, sealed->data(), sealed->size()), "wrong owner key opened the location");
    auto damaged = *sealed;
    damaged.back() ^= 1;
    Require(!OpenLocation(seed, damaged.data(), damaged.size()), "damaged blob opened");

    std::vector<uint8_t> full(512, 0xab);
    auto big = SealLocation(owner, true, endpoint, full.data(), full.size());
    Require(big.has_value(), "512-byte location rejected");
    auto big_open = OpenLocation(seed, big->data(), big->size());
    Require(big_open.has_value() && *big_open == full, "512-byte location did not round-trip");

    std::cout << notice << "\n"
              << "Locate seal checks passed. No endpoint was contacted.\n";
}
