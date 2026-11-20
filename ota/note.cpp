#include "note.h"

#include <string>

namespace ele::ota {
namespace {

constexpr int kNoteFields = 13;

bool Assign(Note& note, const std::string& key, const std::string& value) {
    if (key == "aosp-tag") note.aosp_tag = value;
    else if (key == "branch-phase") note.branch_phase = value;
    else if (key == "built") note.built = value;
    else if (key == "bulletin-is-platform-update") note.bulletin_is_platform_update = value;
    else if (key == "channel") note.channel = value;
    else if (key == "firmware") note.firmware = value;
    else if (key == "fixes") note.fixes = value;
    else if (key == "month") note.month = value;
    else if (key == "platform-source") note.platform_source = value;
    else if (key == "product") note.product = value;
    else if (key == "prototype") note.prototype = value;
    else if (key == "target") note.target = value;
    else if (key == "vendor-partition") note.vendor_partition = value;
    else return false;
    return true;
}

bool MonthOk(const std::string& month) {
    if (month.size() != 7 || month[4] != '-' || month[0] != '2' || month[1] != '0') return false;
    if (month[2] < '0' || month[2] > '9' || month[3] < '0' || month[3] > '9') return false;
    if (month[5] < '0' || month[5] > '9' || month[6] < '0' || month[6] > '9') return false;
    int value = (month[5] - '0') * 10 + (month[6] - '0');
    return value >= 1 && value <= 12;
}

bool FixToken(const std::string& token) {
    if (token.empty() || token.size() > 41) return false;
    unsigned char first = static_cast<unsigned char>(token[0]);
    if (!((first >= 'a' && first <= 'z') || (first >= '0' && first <= '9'))) return false;
    for (unsigned char c : token) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
    }
    return true;
}

bool FixesOk(const std::string& fixes) {
    if (fixes.empty() || fixes.size() > 512 || fixes.front() == ',' || fixes.back() == ',') return false;
    int count = 0;
    size_t start = 0;
    while (start <= fixes.size()) {
        size_t comma = fixes.find(',', start);
        std::string token = fixes.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!FixToken(token)) return false;
        size_t earlier = 0;
        while (earlier < start) {
            size_t end = fixes.find(',', earlier);
            if (fixes.substr(earlier, end - earlier) == token) return false;
            earlier = end + 1;
        }
        ++count;
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return count >= 1 && count <= 16;
}

}  // namespace

std::string NoteCanonical(const Note& note) {
    const std::pair<const char*, const std::string*> fields[] = {
        {"aosp-tag", &note.aosp_tag},
        {"branch-phase", &note.branch_phase},
        {"built", &note.built},
        {"bulletin-is-platform-update", &note.bulletin_is_platform_update},
        {"channel", &note.channel},
        {"firmware", &note.firmware},
        {"fixes", &note.fixes},
        {"month", &note.month},
        {"platform-source", &note.platform_source},
        {"product", &note.product},
        {"prototype", &note.prototype},
        {"target", &note.target},
        {"vendor-partition", &note.vendor_partition},
    };
    std::string raw = std::string(kNoteMagic) + "\n";
    for (const auto& field : fields) {
        raw += field.first;
        raw += '=';
        raw += *field.second;
        raw += '\n';
    }
    return raw;
}

bool ValidateNote(const Note& note, std::string& error) {
    std::string product;
    if (note.channel != "test") {
        error = "channel must be test";
        return false;
    }
    if (note.prototype != "true") {
        error = "prototype must be true";
        return false;
    }
    if (note.built != "no") {
        error = "this checker cannot certify a build";
        return false;
    }
    if (note.bulletin_is_platform_update != "no") {
        error = "a security bulletin is not a platform update";
        return false;
    }
    if (note.firmware != "not-claimed") {
        error = "firmware must stay not-claimed";
        return false;
    }
    if (!TargetProduct(note.target, product) || note.product != product) {
        error = "product does not match target";
        return false;
    }
    if (note.vendor_partition != "not-in-payload") {
        error = "vendor partition stays out of the note";
        return false;
    }
    if (!MonthOk(note.month)) {
        error = "month must be YYYY-MM";
        return false;
    }
    if (note.branch_phase == "maintained") {
        if (note.fixes != "-") {
            error = "a maintained note has no fix list";
            return false;
        }
        if (note.platform_source == "moved") {
            if (!AospReleaseTagValue(note.aosp_tag)) {
                error = "a moved note names an AOSP release tag";
                return false;
            }
        } else if (note.platform_source == "unchanged" || note.platform_source == "not-shipped") {
            if (note.aosp_tag != "-") {
                error = "aosp-tag belongs on a moved note";
                return false;
            }
        } else {
            error = "maintained note does not use fixes-only";
            return false;
        }
    } else if (note.branch_phase == "extended") {
        if (note.platform_source != "fixes-only" || note.aosp_tag != "-") {
            error = "extended branch does not take a new platform release";
            return false;
        }
        if (!FixesOk(note.fixes)) {
            error = "extended note lists fix ids";
            return false;
        }
    } else {
        error = "branch phase must be maintained or extended";
        return false;
    }
    error.clear();
    return true;
}

bool ParseNote(const std::string& raw, Note& out, std::string& error) {
    if (raw.empty() || raw.back() != '\n' || raw.size() > kMaxMetadataBytes) {
        error = "note is not canonical";
        return false;
    }
    Note parsed;
    std::string previous;
    int fields = 0;
    size_t start = 0;
    bool first = true;
    while (start < raw.size()) {
        size_t end = raw.find('\n', start);
        if (end == std::string::npos) {
            error = "note is not canonical";
            return false;
        }
        std::string line = raw.substr(start, end - start);
        start = end + 1;
        if (first) {
            first = false;
            if (line != kNoteMagic) {
                error = "note magic mismatch";
                return false;
            }
            continue;
        }
        size_t eq = line.find('=');
        if (line.empty() || eq == std::string::npos || line.find('=', eq + 1) != std::string::npos) {
            error = "note is not canonical";
            return false;
        }
        std::string key = line.substr(0, eq);
        if (!previous.empty() && key <= previous) {
            error = "note is not canonical";
            return false;
        }
        if (!Assign(parsed, key, line.substr(eq + 1))) {
            error = "note field is not in the prototype set";
            return false;
        }
        previous = std::move(key);
        ++fields;
    }
    if (fields != kNoteFields || NoteCanonical(parsed) != raw) {
        error = "note is not canonical";
        return false;
    }
    if (!ValidateNote(parsed, error)) return false;
    out = std::move(parsed);
    return true;
}

bool BuildNote(const std::string& target, const std::string& month, const std::string& branch_phase,
               const std::string& platform_source, const std::string& aosp_tag, const std::string& fixes, Note& out,
               std::string& error) {
    Note note;
    note.aosp_tag = aosp_tag.empty() ? "-" : aosp_tag;
    note.branch_phase = branch_phase;
    note.built = "no";
    note.bulletin_is_platform_update = "no";
    note.channel = "test";
    note.firmware = "not-claimed";
    note.fixes = fixes.empty() ? "-" : fixes;
    note.month = month;
    note.platform_source = platform_source;
    TargetProduct(target, note.product);
    note.prototype = "true";
    note.target = target;
    note.vendor_partition = "not-in-payload";
    if (!ValidateNote(note, error)) return false;
    out = std::move(note);
    return true;
}

}  // namespace ele::ota
