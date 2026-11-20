// SPDX-License-Identifier: Apache-2.0
#include "locate_payload.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

namespace ele {
namespace {
constexpr std::size_t kMaxPlaintext = 512;
constexpr char kPayload[] = "ELE-OS/locate-payload/v1";
constexpr uint64_t kP[4] = {0xffffffffffffffedULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL,
                            0x7fffffffffffffffULL};

struct Fe {
    uint64_t v[4]{};
};

bool GeP(const uint64_t v[4]) {
    for (int i = 3; i >= 0; --i) {
        if (v[i] != kP[i]) return v[i] > kP[i];
    }
    return true;
}

void SubP(uint64_t v[4]) {
    uint64_t borrow = 0;
    for (int i = 0; i < 4; ++i) {
        uint64_t next = (borrow ? v[i] <= kP[i] : v[i] < kP[i]) ? 1 : 0;
        v[i] = v[i] - kP[i] - borrow;
        borrow = next;
    }
}

Fe FromU64(uint64_t x) {
    Fe out;
    out.v[0] = x;
    return out;
}

Fe One() { return FromU64(1); }

bool IsZero(Fe a) { return (a.v[0] | a.v[1] | a.v[2] | a.v[3]) == 0; }
bool IsOne(Fe a) { return a.v[0] == 1 && (a.v[1] | a.v[2] | a.v[3]) == 0; }

Fe Add(Fe a, Fe b) {
    Fe out;
    uint64_t carry = 0;
    for (int i = 0; i < 4; ++i) {
        unsigned __int128 sum = (unsigned __int128)a.v[i] + b.v[i] + carry;
        out.v[i] = (uint64_t)sum;
        carry = (uint64_t)(sum >> 64);
    }
    if (GeP(out.v)) SubP(out.v);
    return out;
}

Fe Sub(Fe a, Fe b) {
    Fe out;
    uint64_t borrow = 0;
    for (int i = 0; i < 4; ++i) {
        uint64_t next = (borrow ? a.v[i] <= b.v[i] : a.v[i] < b.v[i]) ? 1 : 0;
        out.v[i] = a.v[i] - b.v[i] - borrow;
        borrow = next;
    }
    if (borrow) {
        uint64_t carry = 0;
        for (int i = 0; i < 4; ++i) {
            unsigned __int128 sum = (unsigned __int128)out.v[i] + kP[i] + carry;
            out.v[i] = (uint64_t)sum;
            carry = (uint64_t)(sum >> 64);
        }
    }
    return out;
}

void Fold(uint64_t w[8]) {
    uint64_t hi[5];
    hi[0] = (w[3] >> 63) | (w[4] << 1);
    hi[1] = (w[4] >> 63) | (w[5] << 1);
    hi[2] = (w[5] >> 63) | (w[6] << 1);
    hi[3] = (w[6] >> 63) | (w[7] << 1);
    hi[4] = w[7] >> 63;
    w[3] &= 0x7fffffffffffffffULL;
    w[4] = w[5] = w[6] = w[7] = 0;
    unsigned __int128 carry = 0;
    for (int i = 0; i < 5; ++i) {
        unsigned __int128 sum = (unsigned __int128)w[i] + (unsigned __int128)19 * hi[i] + carry;
        w[i] = (uint64_t)sum;
        carry = sum >> 64;
    }
    w[5] = (uint64_t)carry;
}

Fe Reduce8(uint64_t w[8]) {
    for (int iter = 0; iter < 8; ++iter) {
        if ((w[3] >> 63) == 0 && (w[4] | w[5] | w[6] | w[7]) == 0) break;
        Fold(w);
    }
    Fe out;
    std::copy_n(w, 4, out.v);
    for (int i = 0; i < 2 && GeP(out.v); ++i) SubP(out.v);
    return out;
}

Fe Mul(Fe a, Fe b) {
    uint64_t raw[8] = {};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            unsigned __int128 prod = (unsigned __int128)a.v[i] * b.v[j];
            int limb = i + j;
            unsigned __int128 sum = (unsigned __int128)raw[limb] + (uint64_t)prod;
            raw[limb] = (uint64_t)sum;
            unsigned __int128 carry = (sum >> 64) + (prod >> 64);
            for (int k = limb + 1; carry && k < 8; ++k) {
                unsigned __int128 next = (unsigned __int128)raw[k] + carry;
                raw[k] = (uint64_t)next;
                carry = next >> 64;
            }
        }
    }
    return Reduce8(raw);
}

bool BitPMinus2(int i) {
    if (i < 0 || i >= 255) return false;
    if (i == 0 || i == 1 || i == 3) return true;
    if (i == 2 || i == 4) return false;
    return true;
}

bool BitLegendre(int i) {
    if (i < 0 || i >= 254) return false;
    if (i == 0 || i == 3) return false;
    return true;
}

Fe Pow(Fe base, bool (*bit)(int), int top) {
    Fe result = One();
    for (int i = top; i >= 0; --i) {
        result = Mul(result, result);
        if (bit(i)) result = Mul(result, base);
    }
    return result;
}

Fe Inv(Fe a) { return Pow(a, BitPMinus2, 254); }

Fe Load(const uint8_t in[32]) {
    Fe out;
    for (int i = 0; i < 4; ++i) {
        uint64_t limb = 0;
        for (int j = 0; j < 8; ++j) limb |= (uint64_t)in[i * 8 + j] << (8 * j);
        out.v[i] = limb;
    }
    return out;
}

void Store(Fe a, uint8_t out[32]) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 8; ++j) out[i * 8 + j] = (uint8_t)(a.v[i] >> (8 * j));
    }
}

bool Canonical(Fe y) { return !GeP(y.v); }

bool OnCurve(Fe y) {
    Fe y2 = Mul(y, y);
    Fe d = Sub(FromU64(0), Mul(FromU64(121665), Inv(FromU64(121666))));
    Fe den = Add(Mul(d, y2), One());
    if (IsZero(den)) return false;
    Fe x2 = Mul(Sub(y2, One()), Inv(den));
    Fe leg = Pow(x2, BitLegendre, 253);
    return IsZero(leg) || IsOne(leg);
}

struct PKey {
    EVP_PKEY* p = nullptr;
    ~PKey() { EVP_PKEY_free(p); }
    EVP_PKEY* get() const { return p; }
};

bool SharedSecret(EVP_PKEY* ours, EVP_PKEY* peer, uint8_t out[32]) {
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(EVP_PKEY_CTX_new(ours, nullptr),
                                                                   EVP_PKEY_CTX_free);
    size_t len = 32;
    if (!ctx || EVP_PKEY_derive_init(ctx.get()) != 1 || EVP_PKEY_derive_set_peer(ctx.get(), peer) != 1 ||
        EVP_PKEY_derive(ctx.get(), out, &len) != 1 || len != 32) {
        return false;
    }
    uint8_t any = 0;
    for (int i = 0; i < 32; ++i) any |= out[i];
    return any != 0;
}

bool DeriveKey(const uint8_t secret[32], uint8_t key[32]) {
    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
    EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
    char digest[] = "SHA256";
    char info[] = "ELE-OS/locate-payload/v1";
    char salt[] = "ELE-OS/locate-salt/v1";
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("digest", digest, 0),
        OSSL_PARAM_construct_octet_string("key", const_cast<uint8_t*>(secret), 32),
        OSSL_PARAM_construct_octet_string("info", info, sizeof(info) - 1),
        OSSL_PARAM_construct_octet_string("salt", salt, sizeof(salt) - 1),
        OSSL_PARAM_construct_end(),
    };
    bool ok = ctx && EVP_KDF_derive(ctx, key, 32, params) == 1;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

bool AesGcm(bool encrypt, const uint8_t key[32], const uint8_t nonce[12], const uint8_t* aad,
            size_t aad_len, const uint8_t* input, size_t input_len, uint8_t* output, uint8_t tag[16]) {
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),
                                                                       EVP_CIPHER_CTX_free);
    if (!ctx) return false;
    int len = 0;
    int final_len = 0;
    const EVP_CIPHER* cipher = EVP_aes_256_gcm();
    if (encrypt) {
        if (EVP_EncryptInit_ex(ctx.get(), cipher, nullptr, nullptr, nullptr) != 1 ||
            EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
            EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key, nonce) != 1 ||
            EVP_EncryptUpdate(ctx.get(), nullptr, &len, aad, static_cast<int>(aad_len)) != 1 ||
            EVP_EncryptUpdate(ctx.get(), output, &len, input, static_cast<int>(input_len)) != 1 ||
            EVP_EncryptFinal_ex(ctx.get(), output + len, &final_len) != 1 ||
            EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) {
            return false;
        }
        return true;
    }
    if (EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
        EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key, nonce) != 1 ||
        EVP_DecryptUpdate(ctx.get(), nullptr, &len, aad, static_cast<int>(aad_len)) != 1 ||
        EVP_DecryptUpdate(ctx.get(), output, &len, input, static_cast<int>(input_len)) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, tag) != 1 ||
        EVP_DecryptFinal_ex(ctx.get(), output + len, &final_len) != 1) {
        return false;
    }
    return true;
}

bool X25519FromSeed(const Bytes32& seed, PKey& out) {
    uint8_t hash[SHA512_DIGEST_LENGTH];
    SHA512(seed.data(), seed.size(), hash);
    hash[0] &= 248;
    hash[31] &= 127;
    hash[31] |= 64;
    out.p = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, hash, 32);
    OPENSSL_cleanse(hash, sizeof(hash));
    return out.p != nullptr;
}
}

const char* LocatePrototypeNotice() {
    return "Host prototype. Location bytes are sealed to the owner key only "
           "after the owner enables locate. No handset was located, and no "
           "endpoint was contacted.";
}

std::optional<Bytes32> Ed25519PublicToX25519(const Bytes32& ed25519_public) {
    uint8_t raw[32];
    std::memcpy(raw, ed25519_public.data(), 32);
    raw[31] &= 0x7f;
    Fe y = Load(raw);
    if (!Canonical(y) || !OnCurve(y)) return std::nullopt;
    Fe den = Sub(One(), y);
    if (IsZero(den)) return std::nullopt;
    Fe u = Mul(Add(One(), y), Inv(den));
    Bytes32 out{};
    Store(u, out.data());
    return out;
}

std::optional<std::vector<uint8_t>> SealLocation(const Bytes32& owner_ed25519_public,
                                                 bool owner_enabled_locate,
                                                 const char* owner_endpoint,
                                                 const uint8_t* plaintext, std::size_t plaintext_len) {
    if (!owner_enabled_locate || owner_endpoint == nullptr || owner_endpoint[0] == '\0') {
        return std::nullopt;
    }
    if (plaintext == nullptr || plaintext_len == 0 || plaintext_len > kMaxPlaintext) {
        return std::nullopt;
    }
    auto montgomery = Ed25519PublicToX25519(owner_ed25519_public);
    if (!montgomery) return std::nullopt;

    Bytes32 ephemeral{};
    std::array<uint8_t, 12> nonce{};
    if (RAND_bytes(ephemeral.data(), ephemeral.size()) != 1 ||
        RAND_bytes(nonce.data(), nonce.size()) != 1) {
        return std::nullopt;
    }
    PKey ours;
    ours.p = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, ephemeral.data(), ephemeral.size());
    PKey theirs;
    theirs.p = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, montgomery->data(),
                                           montgomery->size());
    uint8_t eph_pub[32];
    size_t eph_len = sizeof(eph_pub);
    uint8_t secret[32];
    uint8_t key[32];
    bool keys_ok = ours.p && theirs.p &&
                   EVP_PKEY_get_raw_public_key(ours.p, eph_pub, &eph_len) == 1 && eph_len == 32 &&
                   SharedSecret(ours.p, theirs.p, secret) && DeriveKey(secret, key);
    OPENSSL_cleanse(ephemeral.data(), ephemeral.size());
    OPENSSL_cleanse(secret, sizeof(secret));
    if (!keys_ok) {
        OPENSSL_cleanse(key, sizeof(key));
        return std::nullopt;
    }

    std::string aad(kPayload);
    aad.push_back(1);
    aad.append(reinterpret_cast<const char*>(eph_pub), sizeof(eph_pub));
    aad.append(reinterpret_cast<const char*>(nonce.data()), nonce.size());
    std::vector<uint8_t> body(plaintext_len);
    uint8_t tag[16];
    bool sealed = AesGcm(true, key, nonce.data(), reinterpret_cast<const uint8_t*>(aad.data()),
                         aad.size(), plaintext, plaintext_len, body.data(), tag);
    OPENSSL_cleanse(key, sizeof(key));
    if (!sealed) return std::nullopt;

    std::vector<uint8_t> out;
    out.reserve(1 + 32 + nonce.size() + body.size() + sizeof(tag));
    out.push_back(1);
    out.insert(out.end(), eph_pub, eph_pub + sizeof(eph_pub));
    out.insert(out.end(), nonce.begin(), nonce.end());
    out.insert(out.end(), body.begin(), body.end());
    out.insert(out.end(), tag, tag + sizeof(tag));
    return out;
}

std::optional<std::vector<uint8_t>> OpenLocation(const Bytes32& owner_seed, const uint8_t* sealed,
                                                 std::size_t sealed_len) {
    constexpr std::size_t kHeader = 1 + 32 + 12;
    constexpr std::size_t kTag = 16;
    if (sealed == nullptr || sealed_len < kHeader + kTag + 1 || sealed[0] != 1) return std::nullopt;
    std::size_t body = sealed_len - kHeader - kTag;
    if (body == 0 || body > kMaxPlaintext) return std::nullopt;

    PKey ours;
    if (!X25519FromSeed(owner_seed, ours)) return std::nullopt;
    PKey theirs;
    theirs.p = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, sealed + 1, 32);
    uint8_t secret[32];
    uint8_t key[32];
    if (!theirs.p || !SharedSecret(ours.p, theirs.p, secret) || !DeriveKey(secret, key)) {
        OPENSSL_cleanse(secret, sizeof(secret));
        OPENSSL_cleanse(key, sizeof(key));
        return std::nullopt;
    }
    OPENSSL_cleanse(secret, sizeof(secret));

    std::string aad(kPayload);
    aad.push_back(1);
    aad.append(reinterpret_cast<const char*>(sealed + 1), 32 + 12);
    std::vector<uint8_t> plain(body);
    uint8_t tag[16];
    std::memcpy(tag, sealed + sealed_len - kTag, kTag);
    bool opened = AesGcm(false, key, sealed + 1 + 32, reinterpret_cast<const uint8_t*>(aad.data()),
                         aad.size(), sealed + kHeader, body, plain.data(), tag);
    OPENSSL_cleanse(key, sizeof(key));
    if (!opened) return std::nullopt;
    return plain;
}
}
