#ifndef _COMMON_H_
#define _COMMON_H_

#define BASE_PORT_UPSTREAM 8888
#define BASE_PORT_DOWNSTREAM 9000

#define SEND_INPUT_TIME 50

#ifndef FAULTY_NODES
#define FAULTY_NODES 1
#endif

#define MAXBUF 8192

#include <cstdint>
#include <cassert>

#include <array>
#include <vector>

#include <sodium.h>

const static unsigned long long input_len = 1024;

enum class MessageType
{
    INVALID = 0,
    PRIMARY_SEND_INPUT,
    RESULT_TO_DOWNSTREAM,
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
     * Format:  meta_tag (64B) | job_id (8B) | history_hash (32B) | digest_of_signed_input (32B) | input_tag (64B) | input |
     */
public:
    uint8_t *meta_tag;        /* 64B */
    uint64_t *current_job_id; /* 8B */
    uint8_t *history_hash;
    uint8_t *digest_of_signed_input;

    /* The below two construct the client input as stated in the Zyzzyva paper */
    uint8_t *input_tag;
    uint8_t *input;

    SendInputMsgHelper(uint8_t *buf) : meta_tag(buf),
                                       current_job_id(reinterpret_cast<uint64_t *>(buf + crypto_sign_BYTES)),
                                       history_hash(meta_tag + crypto_sign_BYTES + sizeof(*current_job_id)),
                                       digest_of_signed_input(history_hash + crypto_generichash_BYTES),
                                       input_tag(digest_of_signed_input + crypto_generichash_BYTES),
                                       input(input_tag + crypto_sign_BYTES)

    {
    }
};

class ResultMsgHelper
{
public:
    uint8_t *meta_tag;
    uint64_t *job_id;
    uint8_t *history_hash;
    uint8_t *result_hash;

    uint8_t *results;

    uint8_t *send_input_msg_ptr;

    ResultMsgHelper(uint8_t *buf) : meta_tag(buf),
                                    job_id(reinterpret_cast<uint64_t *>(buf + crypto_sign_BYTES)),
                                    history_hash(meta_tag + crypto_sign_BYTES + sizeof(*job_id)),
                                    result_hash(history_hash + crypto_generichash_BYTES),
                                    results(result_hash + crypto_generichash_BYTES),
                                    send_input_msg_ptr(results + input_len)
    {
    }
};

#endif