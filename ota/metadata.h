#pragma once

#include "test_key.h"

#include <array>
#include <string>

namespace ele::ota {

struct Metadata {
    std::string branch_phase;
    std::string build_fingerprint;
    std::string channel;
    std::string firmware_note;
    std::string incremental;
    std::string payload_kind;
    std::string payload_sha256;
    std::string payload_size;
    std::string payload_url;
    std::string platform_security_patch;
    std::string platform_source;
    std::string product;
    std::string prototype;
    std::string target;
    std::string vendor_partition;
    std::string vendor_security_patch;
    std::string version_code;
};

struct CheckedPackage {
    Metadata metadata;
    std::string canonical;
    std::array<uint8_t, 64> signature{};
};

inline bool TargetProduct(const std::string& target, std::string& product) {
    if (target == "cuttlefish") {
        product = "ele_os_cf_x86_64";
        return true;
    }
    if (target == "treble-gsi") {
        product = "ele_os_gsi_arm64";
        return true;
    }
    return false;
}

std::string Canonical(const Metadata& metadata);
bool Validate(const Metadata& metadata, std::string& error);
bool Parse(const std::string& raw, Metadata& out, std::string& error);

bool BuildPrototype(const std::string& target, const std::string& branch_phase,
                    const std::string& version_code, const std::string& platform_source,
                    const std::string& payload_sha256, const std::string& payload_size,
                    Metadata& out, std::string& error);

bool Sha256File(const std::string& path, std::string& hex, uint64_t& size, std::string& error);

bool SignWithTestKey(const std::string& private_pem, const std::string& public_pem,
                     const std::string& canonical, std::array<uint8_t, 64>& signature,
                     std::string& error);

// Signs with the seed in test_key.h. An empty public_pem skips the public-file check.
bool SignWithPinnedTestKey(const std::string& canonical, std::array<uint8_t, 64>& signature,
                           std::string& error);

bool VerifySignature(const std::string& canonical, const std::array<uint8_t, 32>& public_key,
                     const std::array<uint8_t, 64>& signature);

bool LoadPackage(const std::string& directory, CheckedPackage& out, std::string& error);
// Parent is an open directory descriptor. Name must be one path component.
bool LoadPackageAt(int parent, const std::string& name, CheckedPackage& out, std::string& error);

bool WritePackage(const std::string& out_dir, const std::string& payload_path,
                  const Metadata& metadata, const std::array<uint8_t, 64>& signature,
                  std::string& error);

std::string HexEncode(const uint8_t* data, size_t size);
std::string TestPublicFingerprint();
bool PublicKeyIsTestKey(const std::array<uint8_t, 32>& public_key);

}  // namespace ele::ota
