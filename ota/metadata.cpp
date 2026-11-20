#include "metadata.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <utility>

namespace ele::ota {
namespace {

struct Fd {
    int value = -1;
    explicit Fd(int fd) : value(fd) {}
    Fd(Fd&& other) noexcept : value(other.value) { other.value = -1; }
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) {
            if (value >= 0) close(value);
            value = other.value;
            other.value = -1;
        }
        return *this;
    }
    ~Fd() {
        if (value >= 0) close(value);
    }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};

bool IsDecimal(const std::string& text, bool allow_zero, uint32_t& value) {
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

bool IsLowerHex(const std::string& text, size_t expect) {
    if (text.size() != expect) return false;
    for (unsigned char c : text) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

bool IsAospReleaseTag(const std::string& text) {
    const std::string prefix = "aosp-tag:android-";
    if (text.size() <= prefix.size() || text.compare(0, prefix.size(), prefix) != 0) return false;
    size_t i = prefix.size();
    size_t digits = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        ++digits;
        ++i;
    }
    if (digits == 0 || digits > 3) return false;
    while (i < text.size() && text[i] == '.') {
        ++i;
        digits = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            ++digits;
            ++i;
        }
        if (digits == 0 || digits > 3) return false;
    }
    if (i == text.size()) return true;
    if (text[i] != '_' || i + 1 >= text.size() || text[i + 1] != 'r') return false;
    i += 2;
    digits = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        ++digits;
        ++i;
    }
    return digits > 0 && digits <= 4 && i == text.size();
}

std::string ProductFor(const std::string& target) {
    if (target == "cuttlefish") return "ele_os_cf_x86_64";
    if (target == "treble-gsi") return "ele_os_gsi_arm64";
    return "";
}

std::string PayloadKindFor(const std::string& target) {
    if (target == "cuttlefish") return "virtual-device-fixture";
    if (target == "treble-gsi") return "system-image-fixture";
    return "";
}

std::string FingerprintFor(const std::string& product, const std::string& version) {
    return "ele-os/prototype/" + product + "/" + version + "/test-keys";
}

bool ReadAll(int fd, uint64_t limit, std::string& out, std::string& error) {
    std::string data;
    char buffer[4096];
    while (data.size() <= limit) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0) {
            error = "read failed";
            return false;
        }
        if (n == 0) break;
        data.append(buffer, static_cast<size_t>(n));
        if (data.size() > limit) {
            error = "file exceeds prototype limit";
            return false;
        }
    }
    out = std::move(data);
    return true;
}

bool OpenRegular(const std::string& path, Fd& out, struct stat& st, std::string& error) {
    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        error = std::string("cannot open ") + path;
        return false;
    }
    Fd holder(fd);
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        error = std::string("not a regular file: ") + path;
        return false;
    }
    out = std::move(holder);
    return true;
}

bool Assign(Metadata& metadata, const std::string& key, const std::string& value) {
    if (key == "branch-phase") metadata.branch_phase = value;
    else if (key == "build-fingerprint") metadata.build_fingerprint = value;
    else if (key == "channel") metadata.channel = value;
    else if (key == "firmware-note") metadata.firmware_note = value;
    else if (key == "incremental") metadata.incremental = value;
    else if (key == "payload-kind") metadata.payload_kind = value;
    else if (key == "payload-sha256") metadata.payload_sha256 = value;
    else if (key == "payload-size") metadata.payload_size = value;
    else if (key == "payload-url") metadata.payload_url = value;
    else if (key == "platform-security-patch") metadata.platform_security_patch = value;
    else if (key == "platform-source") metadata.platform_source = value;
    else if (key == "product") metadata.product = value;
    else if (key == "prototype") metadata.prototype = value;
    else if (key == "target") metadata.target = value;
    else if (key == "vendor-partition") metadata.vendor_partition = value;
    else if (key == "vendor-security-patch") metadata.vendor_security_patch = value;
    else if (key == "version-code") metadata.version_code = value;
    else return false;
    return true;
}

struct File {
    FILE* value = nullptr;
    explicit File(const char* path) : value(fopen(path, "re")) {}
    ~File() {
        if (value) fclose(value);
    }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
};

std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> LoadPrivatePem(const std::string& path,
                                                                  std::string& error) {
    File file(path.c_str());
    if (!file.value) {
        error = "cannot read private key";
        return {nullptr, EVP_PKEY_free};
    }
    EVP_PKEY* key = PEM_read_PrivateKey(file.value, nullptr, nullptr, nullptr);
    if (!key) error = "cannot parse private key";
    return {key, EVP_PKEY_free};
}

bool RawPublic(EVP_PKEY* key, std::array<uint8_t, 32>& out) {
    size_t size = out.size();
    return key && EVP_PKEY_id(key) == EVP_PKEY_ED25519 &&
           EVP_PKEY_get_raw_public_key(key, out.data(), &size) == 1 && size == out.size();
}

bool LoadPublicPem(const std::string& path, std::array<uint8_t, 32>& out, std::string& error) {
    File file(path.c_str());
    if (!file.value) {
        error = "cannot read public key";
        return false;
    }
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(PEM_read_PUBKEY(file.value, nullptr, nullptr, nullptr),
                                                           EVP_PKEY_free);
    if (!RawPublic(key.get(), out)) {
        error = "cannot parse public key";
        return false;
    }
    return true;
}

}  // namespace

std::string HexEncode(const uint8_t* data, size_t size) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string text(size * 2, '\0');
    for (size_t i = 0; i < size; ++i) {
        text[i * 2] = kDigits[data[i] >> 4];
        text[i * 2 + 1] = kDigits[data[i] & 0x0f];
    }
    return text;
}

std::string TestPublicFingerprint() {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_Digest(kTestPublicKey.data(), kTestPublicKey.size(), digest, &length, EVP_sha256(), nullptr) != 1) {
        return "";
    }
    return HexEncode(digest, length);
}

bool PublicKeyIsTestKey(const std::array<uint8_t, 32>& public_key) {
    return CRYPTO_memcmp(public_key.data(), kTestPublicKey.data(), public_key.size()) == 0;
}

std::string Canonical(const Metadata& metadata) {
    const std::pair<const char*, const std::string*> fields[] = {
        {"branch-phase", &metadata.branch_phase},
        {"build-fingerprint", &metadata.build_fingerprint},
        {"channel", &metadata.channel},
        {"firmware-note", &metadata.firmware_note},
        {"incremental", &metadata.incremental},
        {"payload-kind", &metadata.payload_kind},
        {"payload-sha256", &metadata.payload_sha256},
        {"payload-size", &metadata.payload_size},
        {"payload-url", &metadata.payload_url},
        {"platform-security-patch", &metadata.platform_security_patch},
        {"platform-source", &metadata.platform_source},
        {"product", &metadata.product},
        {"prototype", &metadata.prototype},
        {"target", &metadata.target},
        {"vendor-partition", &metadata.vendor_partition},
        {"vendor-security-patch", &metadata.vendor_security_patch},
        {"version-code", &metadata.version_code},
    };
    std::string raw = std::string(kMetadataMagic) + "\n";
    for (const auto& field : fields) {
        raw += field.first;
        raw += '=';
        raw += *field.second;
        raw += '\n';
    }
    return raw;
}

bool Validate(const Metadata& metadata, std::string& error) {
    uint32_t version = 0;
    uint32_t incremental = 0;
    uint32_t payload_size = 0;
    if (metadata.channel != "test") {
        error = "channel must be test";
        return false;
    }
    if (metadata.prototype != "true") {
        error = "prototype must be true";
        return false;
    }
    if (metadata.firmware_note != "not-claimed") {
        error = "firmware-note must be not-claimed";
        return false;
    }
    if (metadata.target != "cuttlefish" && metadata.target != "treble-gsi") {
        error = "target must be cuttlefish or treble-gsi";
        return false;
    }
    if (metadata.product != ProductFor(metadata.target)) {
        error = "product does not match target";
        return false;
    }
    if (metadata.payload_kind != PayloadKindFor(metadata.target)) {
        error = "payload kind does not match target";
        return false;
    }
    if (metadata.vendor_partition != "not-in-payload" || metadata.vendor_security_patch != "not-in-payload") {
        error = "vendor partition and vendor patch stay out of the prototype payload";
        return false;
    }
    if (metadata.platform_security_patch != "unknown") {
        error = "platform security patch stays unknown until a build records it";
        return false;
    }
    if (metadata.payload_url != "file:payload") {
        error = "prototype payload url must be file:payload";
        return false;
    }
    if (!IsLowerHex(metadata.payload_sha256, 64)) {
        error = "payload sha256 must be 64 lowercase hex characters";
        return false;
    }
    if (!IsDecimal(metadata.payload_size, false, payload_size) || payload_size > kMaxPrototypePayload) {
        error = "payload size is outside the prototype limit";
        return false;
    }
    if (!IsDecimal(metadata.version_code, false, version) || !IsDecimal(metadata.incremental, false, incremental) ||
        version != incremental) {
        error = "version-code and incremental must be the same positive integer";
        return false;
    }
    if (metadata.build_fingerprint != FingerprintFor(metadata.product, metadata.version_code) ||
        metadata.build_fingerprint.find("test-keys") == std::string::npos ||
        metadata.build_fingerprint.find("release-keys") != std::string::npos) {
        error = "build fingerprint must be the prototype test-keys label";
        return false;
    }
    if (metadata.branch_phase == "maintained") {
        if (!IsAospReleaseTag(metadata.platform_source)) {
            error = "maintained package names an AOSP release tag";
            return false;
        }
    } else if (metadata.branch_phase == "extended") {
        if (metadata.platform_source != "fixes-only") {
            error = "extended packages list fixes only";
            return false;
        }
    } else {
        error = "branch phase must be maintained or extended";
        return false;
    }
    error.clear();
    return true;
}

bool Parse(const std::string& raw, Metadata& out, std::string& error) {
    if (raw.empty() || raw.back() != '\n' || raw.size() > kMaxMetadataBytes) {
        error = "metadata is not canonical";
        return false;
    }
    Metadata parsed;
    std::string previous;
    size_t fields = 0;
    size_t start = 0;
    bool first = true;
    while (start < raw.size()) {
        size_t end = raw.find('\n', start);
        if (end == std::string::npos) {
            error = "metadata is not canonical";
            return false;
        }
        std::string line = raw.substr(start, end - start);
        start = end + 1;
        if (first) {
            first = false;
            if (line != kMetadataMagic) {
                error = "metadata magic mismatch";
                return false;
            }
            continue;
        }
        if (line.empty()) {
            error = "metadata is not canonical";
            return false;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos || line.find('=', eq + 1) != std::string::npos) {
            error = "metadata is not canonical";
            return false;
        }
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (!previous.empty() && key <= previous) {
            error = "metadata is not canonical";
            return false;
        }
        if (!Assign(parsed, key, value)) {
            error = "metadata field is not in the prototype set";
            return false;
        }
        previous = key;
        ++fields;
    }
    if (fields != 17) {
        error = "metadata field set mismatch";
        return false;
    }
    if (Canonical(parsed) != raw) {
        error = "metadata is not canonical";
        return false;
    }
    if (!Validate(parsed, error)) return false;
    out = std::move(parsed);
    return true;
}

bool BuildPrototype(const std::string& target, const std::string& branch_phase,
                    const std::string& version_code, const std::string& platform_source,
                    const std::string& payload_sha256, const std::string& payload_size, Metadata& out,
                    std::string& error) {
    Metadata metadata;
    metadata.branch_phase = branch_phase;
    metadata.channel = "test";
    metadata.firmware_note = "not-claimed";
    metadata.incremental = version_code;
    metadata.payload_kind = PayloadKindFor(target);
    metadata.payload_sha256 = payload_sha256;
    metadata.payload_size = payload_size;
    metadata.payload_url = "file:payload";
    metadata.platform_security_patch = "unknown";
    metadata.platform_source = platform_source;
    metadata.product = ProductFor(target);
    metadata.prototype = "true";
    metadata.target = target;
    metadata.vendor_partition = "not-in-payload";
    metadata.vendor_security_patch = "not-in-payload";
    metadata.version_code = version_code;
    metadata.build_fingerprint = FingerprintFor(metadata.product, metadata.version_code);
    if (!Validate(metadata, error)) return false;
    out = std::move(metadata);
    return true;
}

bool Sha256File(const std::string& path, std::string& hex, uint64_t& size, std::string& error) {
    struct stat st {};
    Fd fd(-1);
    if (!OpenRegular(path, fd, st, error)) return false;
    if (st.st_size <= 0 || static_cast<uint64_t>(st.st_size) > kMaxPrototypePayload) {
        error = "prototype payload limit is 1048576 bytes";
        return false;
    }
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        error = "sha256 init failed";
        return false;
    }
    uint64_t hashed = 0;
    char buffer[8192];
    while (hashed < static_cast<uint64_t>(st.st_size)) {
        ssize_t n = read(fd.value, buffer, sizeof(buffer));
        if (n < 0) {
            error = "payload read failed";
            return false;
        }
        if (n == 0) break;
        if (EVP_DigestUpdate(ctx.get(), buffer, static_cast<size_t>(n)) != 1) {
            error = "sha256 update failed";
            return false;
        }
        hashed += static_cast<uint64_t>(n);
    }
    if (hashed != static_cast<uint64_t>(st.st_size)) {
        error = "payload size changed while hashing";
        return false;
    }
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_DigestFinal_ex(ctx.get(), digest, &length) != 1) {
        error = "sha256 final failed";
        return false;
    }
    hex = HexEncode(digest, length);
    size = hashed;
    return true;
}

bool SignCanonical(EVP_PKEY* key, const std::string& canonical, std::array<uint8_t, 64>& signature,
                    std::string& error) {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    size_t length = signature.size();
    if (!key || !ctx || EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key) != 1 ||
        EVP_DigestSign(ctx.get(), signature.data(), &length,
                       reinterpret_cast<const uint8_t*>(canonical.data()), canonical.size()) != 1 ||
        length != signature.size()) {
        error = "signature failed";
        return false;
    }
    return true;
}

bool SignWithTestKey(const std::string& private_pem, const std::string& public_pem,
                     const std::string& canonical, std::array<uint8_t, 64>& signature, std::string& error) {
    auto key = LoadPrivatePem(private_pem, error);
    std::array<uint8_t, 32> from_private{};
    std::array<uint8_t, 32> from_public{};
    if (!RawPublic(key.get(), from_private) || !PublicKeyIsTestKey(from_private) ||
        (!public_pem.empty() && (!LoadPublicPem(public_pem, from_public, error) || !PublicKeyIsTestKey(from_public)))) {
        error = "refusing key that is not the repository test key";
        return false;
    }
    return SignCanonical(key.get(), canonical, signature, error);
}

bool SignWithPinnedTestKey(const std::string& canonical, std::array<uint8_t, 64>& signature, std::string& error) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, kTestSeed.data(), kTestSeed.size()), EVP_PKEY_free);
    std::array<uint8_t, 32> pub{};
    if (!RawPublic(key.get(), pub) || !PublicKeyIsTestKey(pub)) {
        error = "pinned test key does not match the public key";
        return false;
    }
    return SignCanonical(key.get(), canonical, signature, error);
}

bool VerifySignature(const std::string& canonical, const std::array<uint8_t, 32>& public_key,
                     const std::array<uint8_t, 64>& signature) {
    if (!PublicKeyIsTestKey(public_key)) return false;
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, public_key.data(), public_key.size()),
        EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return key && ctx && EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1 &&
           EVP_DigestVerify(ctx.get(), signature.data(), signature.size(),
                            reinterpret_cast<const uint8_t*>(canonical.data()), canonical.size()) == 1;
}

bool LoadPackage(const std::string& directory, CheckedPackage& out, std::string& error) {
    int dirfd = open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (dirfd < 0) {
        error = "package directory is not a real directory";
        return false;
    }
    close(dirfd);
    struct stat st {};
    Fd meta_fd(-1);
    if (!OpenRegular(directory + "/metadata.txt", meta_fd, st, error)) return false;
    std::string raw;
    if (!ReadAll(meta_fd.value, kMaxMetadataBytes, raw, error)) return false;
    Metadata metadata;
    if (!Parse(raw, metadata, error)) return false;
    Fd sig_fd(-1);
    if (!OpenRegular(directory + "/metadata.sig", sig_fd, st, error)) return false;
    if (st.st_size != 64) {
        error = "metadata signature must be 64 bytes";
        return false;
    }
    std::array<uint8_t, 64> signature{};
    size_t got = 0;
    while (got < signature.size()) {
        ssize_t n = read(sig_fd.value, signature.data() + got, signature.size() - got);
        if (n <= 0) {
            error = "metadata signature is short";
            return false;
        }
        got += static_cast<size_t>(n);
    }
    std::string hex;
    uint64_t size = 0;
    if (!Sha256File(directory + "/payload", hex, size, error)) return false;
    if (hex != metadata.payload_sha256 || std::to_string(size) != metadata.payload_size) {
        error = "payload hash does not match metadata";
        return false;
    }
    out.metadata = std::move(metadata);
    out.canonical = std::move(raw);
    out.signature = signature;
    return true;
}

bool WritePackage(const std::string& out_dir, const std::string& payload_path, const Metadata& metadata,
                  const std::array<uint8_t, 64>& signature, std::string& error) {
    if (mkdir(out_dir.c_str(), 0755) != 0) {
        error = "cannot create package directory";
        return false;
    }
    std::string canonical = Canonical(metadata);
    std::string meta_path = out_dir + "/metadata.txt";
    std::string sig_path = out_dir + "/metadata.sig";
    std::string copy_path = out_dir + "/payload";
    int meta = open(meta_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    int sig = open(sig_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    int copy = open(copy_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (meta < 0 || sig < 0 || copy < 0) {
        if (meta >= 0) close(meta);
        if (sig >= 0) close(sig);
        if (copy >= 0) close(copy);
        error = "cannot create package files";
        return false;
    }
    bool ok = write(meta, canonical.data(), canonical.size()) == static_cast<ssize_t>(canonical.size()) &&
              write(sig, signature.data(), signature.size()) == static_cast<ssize_t>(signature.size());
    close(meta);
    close(sig);
    struct stat st {};
    Fd source(-1);
    if (ok && OpenRegular(payload_path, source, st, error)) {
        char buffer[8192];
        uint64_t left = static_cast<uint64_t>(st.st_size);
        while (ok && left > 0) {
            ssize_t n = read(source.value, buffer, sizeof(buffer));
            if (n <= 0) {
                ok = false;
                break;
            }
            if (write(copy, buffer, static_cast<size_t>(n)) != n) ok = false;
            left -= static_cast<uint64_t>(n);
        }
    } else {
        ok = false;
    }
    close(copy);
    if (!ok) {
        if (error.empty()) error = "cannot write package";
        return false;
    }
    CheckedPackage checked;
    return LoadPackage(out_dir, checked, error) &&
           CRYPTO_memcmp(checked.signature.data(), signature.data(), signature.size()) == 0;
}

}  // namespace ele::ota
