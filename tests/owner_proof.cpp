#include "owner_proof.h"

#include <cassert>
#include <openssl/evp.h>
#include <string>

int main() {
    ele::Bytes32 seed{}, key{}, device{}, nonce{};
    seed.fill(13);  // Deterministic fixture only.
    device.fill(5);
    nonce.fill(8);
    auto* signer = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr,
                                                 seed.data(), seed.size());
    assert(signer);
    size_t size = key.size();
    assert(EVP_PKEY_get_raw_public_key(signer, key.data(), &size) == 1);
    std::string message = "ELE-OS/reactivate/v1";
    message.append(device.begin(), device.end());
    message.append(nonce.begin(), nonce.end());
    auto* ctx = EVP_MD_CTX_new();
    assert(ctx && EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, signer) == 1);
    ele::Signature proof{};
    size = proof.size();
    assert(EVP_DigestSign(ctx, proof.data(), &size,
                          reinterpret_cast<const uint8_t*>(message.data()), message.size()) == 1);
    assert(ele::VerifyReactivation(key, device, nonce, proof));
    assert(!ele::VerifyReactivation(key, nonce, device, proof));
    auto different = nonce;
    different[0] ^= 1;
    assert(!ele::VerifyReactivation(key, device, different, proof));
    assert(!ele::VerifyReactivation(key, different, nonce, proof));
    assert(!ele::VerifyReactivation(different, device, nonce, proof));
    proof[0] ^= 1;
    assert(!ele::VerifyReactivation(key, device, nonce, proof));
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(signer);
}
