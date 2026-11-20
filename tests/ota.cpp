#include "metadata.h"

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unistd.h>

namespace {

int failures = 0;

void Expect(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "ok " << name << "\n";
        return;
    }
    std::cerr << "FAIL " << name << "\n";
    ++failures;
}

std::string ReadFile(const std::string& path) {
    std::ifstream in(path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
    std::string hex_file = ReadFile("ota/keys/test/public.hex");
    Expect(hex_file == ele::ota::HexEncode(ele::ota::kTestPublicKey.data(), ele::ota::kTestPublicKey.size()) + "\n",
           "committed public hex matches the pinned test key");

    char dir_template[] = "/tmp/ele-ota-test-XXXXXX";
    char* dir = mkdtemp(dir_template);
    Expect(dir != nullptr, "temp directory");
    if (!dir) return 1;
    std::string payload = std::string(dir) + "/payload-src";
    {
        std::ofstream out(payload, std::ios::binary);
        out << "ele-os prototype payload\n";
    }
    std::string hash;
    uint64_t size = 0;
    std::string error;
    Expect(ele::ota::Sha256File(payload, hash, size, error), "hash fixture payload");

    ele::ota::Metadata cuttlefish;
    Expect(ele::ota::BuildPrototype("cuttlefish", "maintained", "2", "aosp-tag:android-16.0.0_r1", hash,
                                    std::to_string(size), cuttlefish, error),
           "build cuttlefish maintained metadata");
    Expect(cuttlefish.product == "ele_os_cf_x86_64", "cuttlefish product is ele_os_cf_x86_64");
    Expect(cuttlefish.payload_kind == "virtual-device-fixture", "cuttlefish payload is a fixture");
    Expect(cuttlefish.vendor_partition == "not-in-payload", "cuttlefish vendor partition stays out");
    Expect(cuttlefish.firmware_note == "not-claimed", "firmware stays unclaimed");
    Expect(cuttlefish.channel == "test" && cuttlefish.prototype == "true", "package is a test prototype");
    Expect(cuttlefish.build_fingerprint == "ele-os/prototype/ele_os_cf_x86_64/2/test-keys",
           "fingerprint is the prototype label");

    std::string canonical = ele::ota::Canonical(cuttlefish);
    std::array<uint8_t, 64> signature{};
    Expect(ele::ota::SignWithPinnedTestKey(canonical, signature, error), "sign with the pinned test key");
    Expect(ele::ota::VerifySignature(canonical, ele::ota::kTestPublicKey, signature), "verify test signature");
    signature[0] ^= 0x01;
    Expect(!ele::ota::VerifySignature(canonical, ele::ota::kTestPublicKey, signature), "reject flipped signature");
    signature[0] ^= 0x01;
    std::string tampered = canonical;
    tampered[tampered.size() - 2] ^= 0x01;
    Expect(!ele::ota::VerifySignature(tampered, ele::ota::kTestPublicKey, signature), "reject tampered metadata");

    std::array<uint8_t, 32> other{};
    other.fill(0x22);
    Expect(!ele::ota::VerifySignature(canonical, other, signature), "reject a different public key");

    std::array<uint8_t, 32> other_seed{};
    other_seed.fill(0x22);
    EVP_PKEY* other_key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, other_seed.data(), other_seed.size());
    std::string other_path = std::string(dir) + "/other.pem";
    FILE* other_file = fopen(other_path.c_str(), "w");
    Expect(other_key && other_file && PEM_write_PrivateKey(other_file, other_key, nullptr, nullptr, 0, nullptr, nullptr) == 1,
           "write a different private key");
    if (other_file) fclose(other_file);
    if (other_key) EVP_PKEY_free(other_key);
    std::array<uint8_t, 64> unused{};
    Expect(!ele::ota::SignWithTestKey(other_path, "", canonical, unused, error), "refuse a non-test private key");
    Expect(error == "refusing key that is not the repository test key", "refusal names the test-key rule");

    ele::ota::Metadata parsed;
    Expect(ele::ota::Parse(canonical, parsed, error), "parse canonical metadata");
    std::string released = canonical;
    auto channel = released.find("channel=test");
    Expect(channel != std::string::npos, "channel field present");
    released.replace(channel, std::string("channel=test").size(), "channel=release");
    Expect(!ele::ota::Parse(released, parsed, error), "refuse a release channel");

    Expect(!ele::ota::BuildPrototype("cuttlefish", "maintained", "2", "fixes-only", hash, std::to_string(size),
                                     cuttlefish, error),
           "maintained package requires an AOSP tag");
    Expect(!ele::ota::BuildPrototype("cuttlefish", "extended", "2", "aosp-tag:android-16.0.0_r1", hash,
                                     std::to_string(size), cuttlefish, error),
           "extended package refuses a new AOSP tag");
    Expect(ele::ota::BuildPrototype("cuttlefish", "extended", "2", "fixes-only", hash, std::to_string(size),
                                    cuttlefish, error),
           "extended package accepts fixes-only");
    Expect(!ele::ota::BuildPrototype("phone", "maintained", "2", "aosp-tag:android-16.0.0_r1", hash,
                                     std::to_string(size), cuttlefish, error),
           "unknown target is refused");
    Expect(!ele::ota::BuildPrototype("cuttlefish", "maintained", "2", "aosp-tag:android-latest-release", hash,
                                     std::to_string(size), cuttlefish, error),
           "a moving branch name is not a release tag");

    ele::ota::Metadata gsi;
    Expect(ele::ota::BuildPrototype("treble-gsi", "maintained", "1", "aosp-tag:android-16.0.0_r1", hash,
                                    std::to_string(size), gsi, error),
           "build treble gsi metadata");
    Expect(gsi.product == "ele_os_gsi_arm64", "gsi product is the intended ele_os_gsi_arm64 label");
    Expect(gsi.payload_kind == "system-image-fixture", "gsi payload is a system-image fixture");
    Expect(gsi.vendor_partition == "not-in-payload" && gsi.vendor_security_patch == "not-in-payload",
           "gsi metadata leaves vendor out of the payload");

    std::string package = std::string(dir) + "/pkg";
    std::array<uint8_t, 64> gsi_sig{};
    std::string gsi_canonical = ele::ota::Canonical(gsi);
    Expect(ele::ota::SignWithPinnedTestKey(gsi_canonical, gsi_sig, error) &&
               ele::ota::WritePackage(package, payload, gsi, gsi_sig, error),
           "write a package directory");
    ele::ota::CheckedPackage checked;
    Expect(ele::ota::LoadPackage(package, checked, error) &&
               ele::ota::VerifySignature(checked.canonical, ele::ota::kTestPublicKey, checked.signature),
           "load and verify the written package");
    {
        std::ofstream payload_out(package + "/payload", std::ios::binary | std::ios::app);
        payload_out << "x";
    }
    Expect(!ele::ota::LoadPackage(package, checked, error), "reject a payload byte change");
    Expect(error == "payload hash does not match metadata", "hash mismatch is the recorded reason");

    if (failures != 0) {
        std::cerr << failures << " failed\n";
        return 1;
    }
    std::cout << "ota prototype library checks passed\n";
    return 0;
}
