#include "client.h"

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace ele::ota {
namespace {

constexpr size_t kMaxPackages = 64;
struct CloseDir {
    void operator()(DIR* d) const { if (d) closedir(d); }
};

bool ParseVersion(const std::string& text, bool allow_zero, uint32_t& value) {
    if (text.empty() || text.size() > 9) return false;
    if (text.size() > 1 && text[0] == '0') return false;
    if (!allow_zero && text == "0") return false;
    uint32_t parsed = 0;
    for (unsigned char c : text) {
        if (c < '0' || c > '9') return false;
        parsed = parsed * 10u + static_cast<uint32_t>(c - '0');
    }
    if (!allow_zero && parsed == 0) return false;
    value = parsed;
    return true;
}

bool PackageId(const std::string& name) {
    if (name.empty() || name.size() > 64) return false;
    for (unsigned char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
              c == '-')) {
            return false;
        }
    }
    return true;
}

bool LoadPinnedPublic(const std::string& path, std::array<uint8_t, 32>& out, std::string& error) {
    FILE* file = fopen(path.c_str(), "re");
    if (!file) {
        error = "cannot read public key";
        return false;
    }
    EVP_PKEY* key = PEM_read_PUBKEY(file, nullptr, nullptr, nullptr);
    fclose(file);
    size_t size = out.size();
    bool ok = key && EVP_PKEY_id(key) == EVP_PKEY_ED25519 &&
              EVP_PKEY_get_raw_public_key(key, out.data(), &size) == 1 && size == out.size() &&
              PublicKeyIsTestKey(out);
    EVP_PKEY_free(key);
    if (!ok) error = "refusing key that is not the repository test key";
    return ok;
}

std::string Join(const std::vector<std::string>& ids) {
    if (ids.empty()) return "-";
    std::string joined;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) joined += ',';
        joined += ids[i];
    }
    return joined;
}

struct Candidate {
    std::string id;
    bool readable = false;
    bool matches = false;
    bool verified = false;
    uint32_t version = 0;
};

}  // namespace

std::string DecisionText(const Decision& decision) {
    return std::string(kDecisionMagic) + "\n" + "current-version=" + decision.current_version + "\n" +
           "installed=no\n" + "package=" + decision.package + "\n" + "product=" + decision.product + "\n" +
           "reason=" + decision.reason + "\n" + "rejected=" + decision.rejected + "\n" +
           "status=" + decision.status + "\n" + "target=" + decision.target + "\n" +
           "version-code=" + decision.version_code + "\n";
}

bool LoadPinnedTestPublic(const std::string& path, std::array<uint8_t, 32>& out, std::string& error) {
    return LoadPinnedPublic(path, out, error);
}

bool EvaluateChannel(const std::string& channel_dir, const std::array<uint8_t, 32>& public_key,
                     const std::string& target, const std::string& product, const std::string& current_version,
                     Decision& out, std::string& error) {
    Decision decision;
    decision.current_version = current_version;
    decision.target = target;
    decision.product = product;
    std::string expected;
    uint32_t current = 0;
    if (!PublicKeyIsTestKey(public_key)) {
        error = "refusing key that is not the repository test key";
        return false;
    }
    if (!TargetProduct(target, expected) || expected != product) {
        error = "product does not match target";
        return false;
    }
    if (!ParseVersion(current_version, true, current)) {
        error = "current version must be a decimal integer";
        return false;
    }
    int fd = open(channel_dir.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        error = "channel directory is not a real directory";
        return false;
    }
    std::unique_ptr<DIR, CloseDir> dir(fdopendir(fd));
    if (!dir) {
        close(fd);
        error = "channel directory is not a real directory";
        return false;
    }
    std::vector<std::string> names;
    for (;;) {
        errno = 0;
        dirent* entry = readdir(dir.get());
        if (!entry) {
            if (errno != 0) {
                error = "channel enumeration failed";
                return false;
            }
            break;
        }
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        if (names.size() == kMaxPackages) {
            error = "prototype channel limit is 64 entries";
            return false;
        }
        names.push_back(std::move(name));
    }
    std::sort(names.begin(), names.end());

    std::vector<Candidate> candidates;
    std::vector<std::string> rejected_ids;
    for (const std::string& name : names) {
        struct stat st {};
        if (fstatat(dirfd(dir.get()), name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
            error = "cannot stat package";
            return false;
        }
        if (S_ISLNK(st.st_mode)) {
            if (!PackageId(name)) {
                error = "package id is not a prototype token";
                return false;
            }
            Candidate link;
            link.id = name;
            candidates.push_back(link);
            rejected_ids.push_back(name);
            continue;
        }
        if (!S_ISDIR(st.st_mode)) continue;
        if (!PackageId(name)) {
            error = "package id is not a prototype token";
            return false;
        }
        Candidate candidate;
        candidate.id = name;
        CheckedPackage loaded;
        std::string load_error;
        if (!LoadPackageAt(dirfd(dir.get()), name, loaded, load_error)) {
            candidate.readable = false;
            candidates.push_back(candidate);
            rejected_ids.push_back(name);
            continue;
        }
        candidate.readable = true;
        candidate.matches = loaded.metadata.target == target && loaded.metadata.product == product;
        if (!candidate.matches) {
            candidates.push_back(candidate);
            continue;
        }
        uint32_t version = 0;
        if (!ParseVersion(loaded.metadata.version_code, false, version) ||
            !VerifySignature(loaded.canonical, public_key, loaded.signature)) {
            candidate.verified = false;
            rejected_ids.push_back(name);
        } else {
            candidate.verified = true;
            candidate.version = version;
        }
        candidates.push_back(candidate);
    }

    auto reject = [&](const std::string& reason) {
        decision.status = "rejected";
        decision.reason = reason;
        decision.rejected = Join(rejected_ids);
        decision.package = "-";
        decision.version_code = "-";
        out = decision;
    };

    for (const Candidate& candidate : candidates) {
        if (!candidate.readable) {
            reject("unreadable-package");
            return true;
        }
    }
    for (const Candidate& candidate : candidates) {
        if (candidate.matches && !candidate.verified) {
            reject("bad-signature");
            return true;
        }
    }

    uint32_t best = 0;
    size_t best_count = 0;
    const Candidate* selected = nullptr;
    for (const Candidate& candidate : candidates) {
        if (!candidate.matches || !candidate.verified) continue;
        if (best_count == 0 || candidate.version > best) {
            best = candidate.version;
            best_count = 1;
            selected = &candidate;
        } else if (candidate.version == best) {
            ++best_count;
        }
    }
    if (best_count > 1) {
        for (const Candidate& candidate : candidates) {
            if (candidate.matches && candidate.verified && candidate.version == best) {
                rejected_ids.push_back(candidate.id);
            }
        }
        reject("ambiguous-version");
        return true;
    }

    decision.rejected = "-";
    if (!selected || selected->version <= current) {
        decision.status = "no-update";
        decision.reason = "no-newer-verified-package";
        out = decision;
        return true;
    }
    decision.status = "verified-not-installed";
    decision.reason = "verified-not-installed";
    decision.package = selected->id;
    decision.version_code = std::to_string(selected->version);
    out = decision;
    return true;
}

}  // namespace ele::ota
