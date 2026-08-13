#include "MultiSigObj.h"

#include "ns3/core-module.h"

// #include <assert.h>
#include <stdio.h>
#include <string.h>

using namespace ns3;

#if CRYPTO_STATS
#include "../logger.h"
static int sign_stats = 0;
static int verify_stats = 0;
static int combine_sig_stats = 0;
static int combine_key_stats = 0;

void print_crypto_stats()
{
    LOG_INFO("Sign ops: %d", sign_stats);
    LOG_INFO("Verify ops: %d", verify_stats);
    LOG_INFO("Combine sigs ops: %d", combine_sig_stats);
    LOG_INFO("Combine key stats: %d", combine_key_stats);
}
#endif

void print_bytes(unsigned char *m, int bytes)
{
    fprintf(stderr, "Begin bytes\n");
    for (int i = 0; i < bytes; i++)
        fprintf(stderr, "%x ", m[i]);
    fprintf(stderr, "\n");
    fprintf(stderr, "End bytes\n\n");
}

/* This line to make things compile. Copied from multisig.c */
pairing_t libcryptowrap_multisig_pairing;
element_t libcryptowrap_multisig_g;
bool MSObj_loaded = false;

/* ----------------------- CLASS ATTRIBUTE FUNCTIONS ------------------------ */
MultiSigObj::MultiSigObj()
{
    this->initialized = false;
}

MultiSigObj::~MultiSigObj()
{
    if (this->initialized)
    {
        element_t *k = &(this->ms.el);
        element_clear((element_s *)k);
    }
}

/* ----------------------- WRAPPERS THAT USE MultiSigObj ------------------------ */

void MSObj_init()
{
    if (MSObj_loaded)
        return;

    multisig_init();

    /* Copy from multisig.c */
    static const char *config =
        "type a\n"
        "q 46715609578734551849756403761647028741420588238763613510048226706256089815383\n"
        "h 63928234338553166055247157592\n"
        "r 730750818665451459101842416358717970580269694977\n"
        "exp2 159\n"
        "exp1 59\n"
        "sign1 1\n"
        "sign0 1";

    static const char *g_val =
        "\x49\x63\x24\xf6\xc9\xd0\xa6\x01\x60\x16\x47\x46\xf1\x36\xd4\x2e\xa2\x1a\xb1\xdf\xe5\xd5"
        "\x83\x30\x4f\xf4\x56\x45\x0d\xa9\x33\x25\x15\x8b\x65\xce\x46\x2f\x3b\x72\x17\xeb\x91\xaa"
        "\x4c\x72\x27\x3e\xe5\x61\x76\x56\x49\xa3\x06\x4c\xa9\x31\xf9\x64\xb4\x7c\x66\x2b";
    /* End Copy from multisig.c */

    // Load the configuration from the string.
    pairing_init_set_buf(libcryptowrap_multisig_pairing, config, strlen(config));
    // Load g from config string.
    element_init_G2(libcryptowrap_multisig_g, libcryptowrap_multisig_pairing);
    element_from_bytes(libcryptowrap_multisig_g, (unsigned char *)g_val);

    // pairing_pp_init(multisig_pairing_pp_g, libcryptowrap_multisig_g, multisig_pairing);
    MSObj_loaded = true;
}

void MSObj_gen_keypair(MultiSigObj &public_key, MultiSigObj &secret_key)
{
    multisig_gen_keypair(&public_key.ms, &secret_key.ms);
    public_key.initialized = true;
    secret_key.initialized = true;
}

void MSObj_sign(MultiSigObj &sig, char *buf, int len, MultiSigObj &secret_key)
{
    if (sig.initialized)
    {
        element_t *k = &(sig.ms.el);
        element_clear((element_s *)k);
        sig.initialized = false;
    }
    multisig_sign(&sig.ms, buf, len, &secret_key.ms);
    sig.initialized = true;
#if CRYPTO_STATS
    sign_stats++;
#endif
}

int MSObj_verify(char *buf, int len, MultiSigObj &public_key, MultiSigObj &sig)
{
#if CRYPTO_STATS
    verify_stats++;
#endif
    return multisig_verify(buf, len, &public_key.ms, &sig.ms);
}

/* ------------------------ Combines signatures -------------------------------- */
void MSObj_combine_helper(MultiSigObj &out, MultiSigObj *items, unsigned int num_items)
{
    if (num_items == 0)
        return;
    element_set(out.ms.el, items[0].ms.el);
    for (unsigned int i = 1; i < num_items; i++)
    {
        element_mul(out.ms.el, out.ms.el, items[i].ms.el);
    }
    out.initialized = true;
}

void MSObj_combine_sigs(MultiSigObj &out, MultiSigObj *sigs, unsigned int num_sigs)
{
#if CRYPTO_STATS
    combine_sig_stats++;
#endif   
    if (out.initialized)
    {
        element_t *k = &(out.ms.el);
        element_clear((element_s *)k);
        out.initialized = false;
    }
    element_init_G1(out.ms.el, libcryptowrap_multisig_pairing);
    out.initialized = true;
    MSObj_combine_helper(out, sigs, num_sigs);
}

void MSObj_combine_sig(MultiSigObj &out, MultiSigObj &sig1, MultiSigObj &sig2)
{
#if CRYPTO_STATS
    combine_sig_stats++;
#endif
    multisig_combine_sig(&out.ms, &sig1.ms, &sig2.ms);
    out.initialized = true;
}

/* --------------------------------  Public Keys -------------------------------- */
void MSObj_combine_keys(MultiSigObj &out, MultiSigObj *keys, unsigned int num_keys)
{
    if (out.initialized)
    {
        element_t *k = &(out.ms.el);
        element_clear((element_s *)k);
        out.initialized = false;
    }
    element_init_G2(out.ms.el, libcryptowrap_multisig_pairing);
    out.initialized = true;
    MSObj_combine_helper(out, keys, num_keys);
}

void MSObj_combine_key(MultiSigObj &out, MultiSigObj &key1, MultiSigObj &key2)
{
#if CRYPTO_STATS
    combine_key_stats++;
#endif
    multisig_combine_key(&out.ms, &key1.ms, &key2.ms);
    out.initialized = true;
}

void MSObj_print(MultiSigObj &sig)
{
    multisig_print(&sig.ms);
}

void MSObj_set(MultiSigObj &dst, MultiSigObj &src)
{
    NS_ASSERT(src.initialized);
    if (dst.initialized)
    {
        element_t *k = &(dst.ms.el);
        element_clear((element_s *)k);
        dst.initialized = false;
    }
    multisig_set(&dst.ms, &src.ms);
    dst.initialized = true;
}

void MSObj_set(MultiSigObj *dst, MultiSigObj *src)
{
    NS_ASSERT(src->initialized);
    if (dst->initialized)
    {
        element_t *k = &(dst->ms.el);
        element_clear((element_s *)k);
        dst->initialized = false;
    }
    multisig_set(&dst->ms, &src->ms);
    dst->initialized = true;
}

void MSObj_to_buf(MultiSigObj &ms, char *buf)
{
    multisig_to_buf(&ms.ms, buf);
}

void MSObj_from_buf(MultiSigObj &ms, char *buf)
{
    if (ms.initialized)
    {
        element_t *k = &(ms.ms.el);
        element_clear((element_s *)k);
        ms.initialized = false;
    }

    /* Initialization copied from multisig_sign() */
    /* Otherwise keep getting "use of uninitialized value" and segfaults */
    /* Initialize sig variable to G1 */
    element_init_G1(ms.ms.el, libcryptowrap_multisig_pairing);

    ms.initialized = true;
    multisig_from_buf(&ms.ms, buf);
}

void MSObj_init_pub(MultiSigObj &ms)
{
    if (ms.initialized)
    {
        element_t *k = &(ms.ms.el);
        element_clear((element_s *)k);
        ms.initialized = false;
    }
    multisig_init_pub(&ms.ms);
    ms.initialized = true;
}

void MSObj_init_sec(MultiSigObj &ms)
{
    if (ms.initialized)
    {
        element_t *k = &(ms.ms.el);
        element_clear((element_s *)k);
        ms.initialized = false;
    }
    multisig_init_sig(&ms.ms);
    ms.initialized = true;
}

void MSObj_init_sig(MultiSigObj &ms)
{
    if (ms.initialized)
    {
        element_t *k = &(ms.ms.el);
        element_clear((element_s *)k);
        ms.initialized = false;
    }
    multisig_init_sig(&ms.ms);
    ms.initialized = true;
}

// void multisig::key_gen(char *public_key, char *secret_key)
// {
// 	MSObj_init();
// 	MultiSigObj pk, sk;
// 	MSObj_init_pub(pk);
// 	MSObj_init_sec(sk);
// 	MSObj_gen_keypair(pk, sk);
// 	MSObj_to_buf(pk, public_key);
// 	MSObj_to_buf(sk, secret_key);
// }

// void multisig::sign(char *buf, int bufsize, char *secret_key, char *sig, int *siglen)
// {
// 	MSObj_init();
// 	MultiSigObj skobj, sigobj;
// 	MSObj_init_sec(skobj);
// 	MSObj_init_sig(sigobj);

// 	MSObj_from_buf(skobj, secret_key);
// 	MSObj_sign(sigobj, buf, bufsize, skobj);
// 	MSObj_to_buf(sigobj, sig);
// 	if (siglen)
// 		*siglen = multisig::sig_length;
// }

// bool multisig::verify(char *buf, int bufsize, char *public_key, char *sig, int *siglen)
// {
// 	(void)siglen;
// 	MSObj_init();
// 	MultiSigObj pkobj, sigobj;
// 	MSObj_init_pub(pkobj);
// 	MSObj_init_sig(sigobj);

// 	MSObj_from_buf(pkobj, public_key);
// 	MSObj_from_buf(sigobj, sig);
// 	return MSObj_verify(buf, bufsize, pkobj, sigobj);
// }

// void multisig::combine_public_key(char *out, char *key1, char *key2)
// {
// 	MSObj_init();
// 	MultiSigObj objout, obj1, obj2;
// 	MSObj_init_pub(objout);
// 	MSObj_init_pub(obj1);
// 	MSObj_init_pub(obj2);

// 	MSObj_from_buf(obj1, key1);
// 	MSObj_from_buf(obj2, key2);

// 	MSObj_combine_key(objout, obj1, obj2);
// 	MSObj_to_buf(objout, out);
// }

// void multisig::combine_sig(char *out, char *sig1, char *sig2)
// {
// 	MSObj_init();
// 	MultiSigObj objout, obj1, obj2;
// 	MSObj_init_sig(objout);
// 	MSObj_init_sig(obj1);
// 	MSObj_init_sig(obj2);

// 	MSObj_from_buf(obj1, sig1);
// 	MSObj_from_buf(obj2, sig2);

// 	MSObj_combine_sig(objout, obj1, obj2);
// 	MSObj_to_buf(objout, out);
// }
