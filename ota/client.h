#pragma once

#include "metadata.h"

#include <string>

namespace ele::ota {

inline constexpr char kDecisionMagic[] = "ELE-OS-UPDATE-DECISION/1";

struct Decision {
    std::string current_version;
    std::string package = "-";
    std::string product;
    std::string reason;
    std::string rejected = "-";
    std::string status;
    std::string target;
    std::string version_code = "-";
};

// Local directory only. The text always contains installed=no.
std::string DecisionText(const Decision& decision);

bool LoadPinnedTestPublic(const std::string& path, std::array<uint8_t, 32>& out, std::string& error);

bool EvaluateChannel(const std::string& channel_dir, const std::array<uint8_t, 32>& public_key,
                     const std::string& target, const std::string& product,
                     const std::string& current_version, Decision& out, std::string& error);

}  // namespace ele::ota
