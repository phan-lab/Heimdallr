#ifndef _COMMON_H_
#define _COMMON_H_

#define BASE_PORT_UPSTREAM 8888
#define BASE_PORT_DOWNSTREAM 9000

#define TAG_EXCHANGE_TIME 500 /* At the 500-th ms of each second */
#define SEND_HB_TIME 550      /* At the 550-th ms of each second */
#define SEND_ACC_TIME 800

#ifndef FAULTY_NODES
#define FAULTY_NODES 1
#endif

#define MAXBUF 8192

#include <cstdint>
#include <cassert>

#include <array>
#include <vector>

#include <sodium.h>

enum class MessageType
{
    INVALID = 0,
    PRIMARY_SEND_INPUT,
    RESULT_TO_DOWNSTREAM,
    POC,
};

class Message
{
public:
    MessageType type;
    uint8_t content[MAXBUF];

    Message() {}
    Message(MessageType t) : type(t) {}
};

class SendInputMsgHelper
{
    /**
     * Format:  sig (64B)| job_id (8B) | input |
     */
public:
    uint8_t *sig;
    uint64_t *job_id;
    uint8_t *input;

    SendInputMsgHelper(uint8_t *buf) : sig(buf),
                                       job_id(reinterpret_cast<uint64_t *>(sig + crypto_sign_BYTES)),
                                       input(reinterpret_cast<uint8_t *>(job_id) + sizeof(*job_id))
    {
    }
};

class ResultMsgHelper
{
    /**
     * Format:  sig (64B) | job_id (8B) | results |
     */
public:
    uint8_t *sig;
    uint64_t *job_id;
    uint8_t *results;
    ResultMsgHelper(uint8_t *buf) : sig(buf),
                                    job_id(reinterpret_cast<uint64_t *>(sig + crypto_sign_BYTES)),
                                    results(reinterpret_cast<uint8_t *>(job_id) + sizeof(*job_id)) {}
};

class PoCMsgHelper
{

public:
    /**
     * Format: | job_id (8B) | result_hash (64B) | sigs |
     */
    typedef struct
    {
        uint8_t sig[crypto_sign_BYTES];
    } sig_t;

    static_assert(sizeof(sig_t) == crypto_sign_BYTES);
    uint64_t *job_id;
    uint8_t *result_hash;
    sig_t *sigs;
    PoCMsgHelper(uint8_t *buf) : job_id(reinterpret_cast<uint64_t *>(buf)),
                                 result_hash(buf + sizeof(*job_id)),
                                 sigs(reinterpret_cast<sig_t *>(result_hash + crypto_generichash_BYTES)) {}
};

#endif