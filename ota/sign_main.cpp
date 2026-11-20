#include "metadata.h"

#include <iostream>
#include <string>

namespace {

void Usage() {
    std::cerr
        << "usage: ele-ota-sign --payload FILE --out-dir DIR --target cuttlefish|treble-gsi "
           "--branch-phase maintained|extended --version-code N --platform-source VALUE\n"
           "optional: --key FILE [--public FILE], which must be the repository test key\n";
}

bool Take(int argc, char** argv, int& index, std::string& dest) {
    if (index + 1 >= argc) return false;
    dest = argv[++index];
    return !dest.empty();
}

}  // namespace

int main(int argc, char** argv) {
    std::string key, pub, payload, out, target, phase, version, source;
    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        bool ok = true;
        if (flag == "--key") ok = Take(argc, argv, i, key);
        else if (flag == "--public") ok = Take(argc, argv, i, pub);
        else if (flag == "--payload") ok = Take(argc, argv, i, payload);
        else if (flag == "--out-dir") ok = Take(argc, argv, i, out);
        else if (flag == "--target") ok = Take(argc, argv, i, target);
        else if (flag == "--branch-phase") ok = Take(argc, argv, i, phase);
        else if (flag == "--version-code") ok = Take(argc, argv, i, version);
        else if (flag == "--platform-source") ok = Take(argc, argv, i, source);
        else ok = false;
        if (!ok) {
            Usage();
            return 1;
        }
    }
    if (payload.empty() || out.empty() || target.empty() || phase.empty() || version.empty() || source.empty() ||
        (key.empty() && !pub.empty())) {
        Usage();
        return 1;
    }

    std::string hex, error;
    uint64_t size = 0;
    ele::ota::Metadata metadata;
    std::array<uint8_t, 64> signature{};
    if (!ele::ota::Sha256File(payload, hex, size, error) ||
        !ele::ota::BuildPrototype(target, phase, version, source, hex, std::to_string(size), metadata, error)) {
        std::cerr << error << "\n";
        return 1;
    }
    std::string canonical = ele::ota::Canonical(metadata);
    bool signed_ok = key.empty() ? ele::ota::SignWithPinnedTestKey(canonical, signature, error)
                                 : ele::ota::SignWithTestKey(key, pub, canonical, signature, error);
    if (!signed_ok || !ele::ota::WritePackage(out, payload, metadata, signature, error)) {
        std::cerr << error << "\n";
        return 1;
    }

    std::cout << "release-key=no\n"
              << "test-public-sha256=" << ele::ota::TestPublicFingerprint() << "\n"
              << "channel=test\n"
              << "prototype=true\n"
              << "target=" << metadata.target << "\n"
              << "product=" << metadata.product << "\n"
              << "branch-phase=" << metadata.branch_phase << "\n"
              << "installed=no\n"
              << "status=signed-prototype\n";
    return 0;
}
