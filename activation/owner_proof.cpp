#include "owner_proof.h"

#include <algorithm>
#include <memory>
#include <openssl/evp.h>

namespace ele {
bool VerifyReactivation(const Bytes32& enrolled_key, const Bytes32& device_id,
                        const Bytes32& challenge, const Signature& proof) {
    constexpr char purpose[] = "ELE-OS/reactivate/v1";
    std::array<uint8_t, sizeof(purpose) - 1 + 32 + 32> message{};
    auto next = std::copy_n(purpose, sizeof(purpose) - 1, message.begin());
    next = std::copy(device_id.begin(), device_id.end(), next);
    std::copy(challenge.begin(), challenge.end(), next);

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, enrolled_key.data(),
                                    enrolled_key.size()), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(
        EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return key && ctx &&
           EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1 &&
           EVP_DigestVerify(ctx.get(), proof.data(), proof.size(),
                            message.data(), message.size()) == 1;
}
}
