#include "client.h"

#include <iostream>
#include <string>

namespace {

void Usage() {
    std::cerr << "usage: ele-ota-client --channel DIR --target cuttlefish|treble-gsi "
                 "--product PRODUCT --current-version N [--public FILE]\n";
}

bool Take(int argc, char** argv, int& index, std::string& dest) {
    if (index + 1 >= argc) return false;
    dest = argv[++index];
    return !dest.empty();
}

}  // namespace

int main(int argc, char** argv) {
    std::string pub, channel, target, product, current;
    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        bool ok = true;
        if (flag == "--public") ok = Take(argc, argv, i, pub);
        else if (flag == "--channel") ok = Take(argc, argv, i, channel);
        else if (flag == "--target") ok = Take(argc, argv, i, target);
        else if (flag == "--product") ok = Take(argc, argv, i, product);
        else if (flag == "--current-version") ok = Take(argc, argv, i, current);
        else ok = false;
        if (!ok) {
            Usage();
            return 2;
        }
    }
    if (channel.empty() || target.empty() || product.empty() || current.empty()) {
        Usage();
        return 2;
    }

    std::array<uint8_t, 32> public_key = ele::ota::kTestPublicKey;
    std::string error;
    if (!pub.empty() && !ele::ota::LoadPinnedTestPublic(pub, public_key, error)) {
        std::cerr << error << "\n";
        std::cout << "installed=no\nstatus=rejected\n";
        return 1;
    }
    ele::ota::Decision decision;
    if (!ele::ota::EvaluateChannel(channel, public_key, target, product, current, decision, error)) {
        std::cerr << error << "\n";
        std::cout << "installed=no\nstatus=rejected\n";
        return 1;
    }
    std::cout << ele::ota::DecisionText(decision);
    return decision.status == "rejected" ? 1 : 0;
}
