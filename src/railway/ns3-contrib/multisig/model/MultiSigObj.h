#include "multisig.h"

#ifndef __STRICT_ANSI__
#define __STRICT_ANSI__
#endif

#ifndef MULTISIGOBJ_HPP
#define MULTISIGOBJ_HPP

#include "ns3/core-module.h"


#define CRYPTO_STATS (0)

void print_bytes(unsigned char *m, int bytes);

/**
 * \brief Class to auto-handle memory for a multisig_t
 */
class MultiSigObj
{
  public:
    multisig_t ms;
    bool initialized;

    MultiSigObj();
    ~MultiSigObj();

    /* should use set instead */
    MultiSigObj(const MultiSigObj& other) = delete;
    MultiSigObj& operator=(const MultiSigObj&) = delete;
};

/* ----------------------- WRAPPERS THAT USE MultiSigObj ------------------------ */

extern bool MSOBj_loaded;
/** @brief calls multisig_init() and does any special MultiSigObj initialization */
void MSObj_init();

/** @brief Generates a public/private keypair.
 *
 * @param[out] public_key The generated public key.
 * @param[in] secret_key The corresponding generated private key.
 *
 * @return Void.
 */
void MSObj_gen_keypair(MultiSigObj& public_key, MultiSigObj& secret_key);

/** @brief Signs an arbitrary buffer.
 *
 * @param[out] sig The signature will be stored here.
 * @param[in] buf The buffer containing the data to be signed.
 * @param[in] len The number of Bytes in the buffer to be signed.
 * @param[in] secret_key The secret key to use for the signing.
 *
 * @return Void.
 */
void MSObj_sign(MultiSigObj& sig, char* buf, int len, MultiSigObj& secret_key);

/** @brief Verifies a signature on a buffer of data.
 *
 * @param[in] buf The buffer containing the data to be verified.
 * @param[in] len The number of Bytes in the buffer to be verified.
 * @param[in] public_key The public key to use for the verification.
 * @param[in] sig The signature to check against the data.
 *
 * @return 1 if the signature and public key match the data, 0 otherwise.
 */
int MSObj_verify(char* buf, int len, MultiSigObj& public_key, MultiSigObj& sig);

// Combines signatures.
void MSObj_combine_sigs(MultiSigObj& out, MultiSigObj* sigs, unsigned int num_sigs);
void MSObj_combine_sig(MultiSigObj& out, MultiSigObj& sig1, MultiSigObj& sig2);
// Public Keys
void MSObj_combine_keys(MultiSigObj& out, MultiSigObj* keys, unsigned int num_keys);
void MSObj_combine_key(MultiSigObj& out, MultiSigObj& key1, MultiSigObj& key2);

void MSObj_print(MultiSigObj& ms);
void MSObj_set(MultiSigObj& dst, MultiSigObj& src);
void MSObj_set(MultiSigObj* dst, MultiSigObj* src);
void MSObj_to_buf(MultiSigObj& ms, char* buf);
void MSObj_from_buf(MultiSigObj& ms, char* buf);

void MSObj_init_pub(MultiSigObj& ms);
void MSObj_init_sec(MultiSigObj& ms);
void MSObj_init_sig(MultiSigObj& ms);

void print_crypto_stats();

#endif
