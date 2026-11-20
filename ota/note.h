#pragma once

#include "metadata.h"

#include <string>

namespace ele::ota {

inline constexpr char kNoteMagic[] = "ELE-OS-UPDATE-NOTE/1";

struct Note {
    std::string aosp_tag;
    std::string branch_phase;
    std::string built;
    std::string bulletin_is_platform_update;
    std::string channel;
    std::string firmware;
    std::string fixes;
    std::string month;
    std::string platform_source;
    std::string product;
    std::string prototype;
    std::string target;
    std::string vendor_partition;
};

std::string NoteCanonical(const Note& note);
bool ValidateNote(const Note& note, std::string& error);
bool ParseNote(const std::string& raw, Note& out, std::string& error);

bool BuildNote(const std::string& target, const std::string& month, const std::string& branch_phase,
               const std::string& platform_source, const std::string& aosp_tag, const std::string& fixes,
               Note& out, std::string& error);

}  // namespace ele::ota
