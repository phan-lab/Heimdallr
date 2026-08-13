#ifndef MULTISIG_H
#define MULTISIG_H
#include <pbc/pbc.h>
/*
// TYPE A default
#define MULTISIG_PUB_LENGTH 128
#define MULTISIG_SEC_LENGTH 20
#define MULTISIG_SIG_LENGTH 128
#define MULTISIG_LENGTH_CHECK
*/
/*
// TYPE D
#define MULTISIG_PUB_LENGTH 128
#define MULTISIG_SEC_LENGTH 20
#define MULTISIG_SIG_LENGTH 40
#define MULTISIG_LENGTH_CHECK
*/
/*
// Brian's goto quick one GOTO 
#define MULTISIG_PUB_LENGTH 44
#define MULTISIG_SEC_LENGTH 13
#define MULTISIG_SIG_LENGTH 44
#define MULTISIG_LENGTH_CHECK
*/
/*
// r = 120, q = 256 
#define MULTISIG_PUB_LENGTH 64
#define MULTISIG_SEC_LENGTH 15
#define MULTISIG_SIG_LENGTH 64
#define MULTISIG_LENGTH_CHECK
*/
// r = 154, q = 256
#define MULTISIG_PUB_LENGTH 64
#define MULTISIG_SEC_LENGTH 20
#define MULTISIG_SIG_LENGTH 64
#define MULTISIG_LENGTH_CHECK

// r = 160 , q = 256
#define MULTISIG_PUB_LENGTH 64
#define MULTISIG_SEC_LENGTH 20
#define MULTISIG_SIG_LENGTH 64
#define MULTISIG_LENGTH_CHECK

#ifdef __cplusplus
extern "C" {
#endif

typedef struct multisig_t {
	element_t el;
} multisig_t;

/** @brief Initializes the multisiganture library.
 *
 * Loads the module's dependencies with the appropriate
 * configurations.
 *
 * @return Void.
 */
void multisig_init();

/**
 * \brief Same as multisig_init, but takes a param for initialization
 */
void multisig_init();

/** @brief Generates a public/private keypair.
 *
 * @param[out] public_key The generated public key.
 * @param[in] secret_key The corresponding generated private key.
 *
 * @return Void.
 */
void multisig_gen_keypair(multisig_t* public_key, multisig_t* secret_key);

/**
 * Frees the memory alloc'd in multisig_gen_keypair()
 */
void multisig_clear_keypair(multisig_t* public_key, multisig_t* secret_key);

/** @brief Signs an arbitrary buffer.
 *
 * @param[out] sig The signature will be stored here.
 * @param[in] buf The buffer containing the data to be signed.
 * @param[in] len The number of Bytes in the buffer to be signed.
 * @param[in] secret_key The secret key to use for the signing.
 *
 * @return Void.
 */
void multisig_sign(multisig_t* sig, char* buf, int len, multisig_t* secret_key);

/** @brief Verifies a signature on a buffer of data.
 *
 * @param[in] buf The buffer containing the data to be verified.
 * @param[in] len The number of Bytes in the buffer to be verified.
 * @param[in] public_key The public key to use for the verification.
 * @param[in] sig The signature to check against the data.
 *
 * @return 1 if the signature and public key match the data, 0 otherwise.
 */
int multisig_verify(char* buf, int len, multisig_t* public_key, multisig_t* sig);

/** @brief Verifies multiple public keys for a signature on a buffer of data.
 *
 * @param[in] buf The buffer containing the data to be verified.
 * @param[in] len The number of Bytes in the buffer to be verified.
 * @param[in] public_keys The list of public keys to use for the verification.
 * @param[in] num_keys The number of public keys in the list \p public_keys.
 * @param[in] sig The signature to check against the data.
 *
 * @return 1 if the signature and public key match the data, 0 otherwise.
 */
//int multisig_verify_all(char* buf, int len, multisig_t* public_keys, unsigned int num_keys, multisig_t sig);

// Combines signatures.
void multisig_combine_sigs(multisig_t* out, multisig_t* sigs, unsigned int num_sigs);
void multisig_combine_sig(multisig_t* out, multisig_t* sig1, multisig_t* sig2);
// Public Keys
void multisig_combine_keys(multisig_t* out, multisig_t* keys, unsigned int num_keys);
void multisig_combine_key(multisig_t* out, multisig_t* key1, multisig_t* key2);

void multisig_print(multisig_t* ms);
void multisig_set(multisig_t* dst, multisig_t* src);
void multisig_to_buf(multisig_t* ms, char* buf);
void multisig_from_buf(multisig_t* ms, char* buf);

void multisig_init_pub(multisig_t* ms);
void multisig_init_sec(multisig_t* ms);
void multisig_init_sig(multisig_t* ms);
void multisig_gen_g();
#ifdef __cplusplus
}
#endif
#endif
