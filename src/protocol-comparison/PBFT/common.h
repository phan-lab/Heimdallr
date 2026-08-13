#ifndef _COMMON_H_
#define _COMMON_H_

#define BASE_PORT_UPSTREAM 8888
#define BASE_PORT_DOWNSTREAM 9000

#define SEND_PREPREPARE_TIME 50 /* At the 500-th ms of each second */


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
    PRE_PREPARE,
    PREPARE,
    COMMIT,
    REPLY
};

class Message
{
public:
    MessageType type;
    uint8_t content[MAXBUF];

    Message() {}
    Message(MessageType t) : type(t) {}
};

class PrePrepareMsgHelper
{
public:
    uint8_t *sig;
    uint64_t *job_id;
    uint8_t *digest_of_input;
    uint8_t *input;
    PrePrepareMsgHelper(uint8_t *buf) : sig(buf),
                                        job_id(reinterpret_cast<uint64_t *>(buf + crypto_sign_BYTES)),
                                        digest_of_input(reinterpret_cast<uint8_t *>(job_id) + sizeof(uint64_t)),
                                        input(digest_of_input + crypto_generichash_BYTES)
    {
    }
    const static size_t size = crypto_sign_BYTES + sizeof(uint64_t) + crypto_generichash_BYTES + input_len;
};

class PrepareMsgHelper
{
public:
    uint8_t *sig;
    uint64_t *job_id;
    uint8_t *digest_of_input;
    uint64_t *sender_id;
    PrepareMsgHelper(uint8_t *buf) : sig(buf),
                                     job_id(reinterpret_cast<uint64_t *>(buf + crypto_sign_BYTES)),
                                     digest_of_input(reinterpret_cast<uint8_t *>(job_id) + sizeof(uint64_t)),
                                     sender_id(reinterpret_cast<uint64_t *>(digest_of_input + crypto_generichash_BYTES))
    {
    }
    const static size_t size = crypto_sign_BYTES + sizeof(uint64_t) + crypto_generichash_BYTES + sizeof(uint64_t);
};

class CommitMsgHelper
{
    public:
    uint8_t *sig;
    uint64_t *job_id;
    uint8_t *digest_of_input;
    uint64_t *sender_id;
    CommitMsgHelper(uint8_t *buf) : sig(buf),
                                    job_id(reinterpret_cast<uint64_t *>(buf + crypto_sign_BYTES)),
                                    digest_of_input(reinterpret_cast<uint8_t *>(job_id) + sizeof(uint64_t)),
                                    sender_id(reinterpret_cast<uint64_t *>(digest_of_input + crypto_generichash_BYTES))
    {
    }
    const static size_t size = crypto_sign_BYTES + sizeof(uint64_t) + crypto_generichash_BYTES + sizeof(uint64_t);
};

class ReplyMsgHelper
{
    public:
    uint8_t *sig;
    uint64_t *job_id;
    uint8_t *digest_of_input;
    uint64_t *sender_id;
    uint8_t *output;
    ReplyMsgHelper(uint8_t *buf) : sig(buf),
                                   job_id(reinterpret_cast<uint64_t *>(buf + crypto_sign_BYTES)),
                                   digest_of_input(reinterpret_cast<uint8_t *>(job_id) + sizeof(uint64_t)),
                                   sender_id(reinterpret_cast<uint64_t *>(digest_of_input + crypto_generichash_BYTES)),
                                   output(reinterpret_cast<uint8_t *>(sender_id) + sizeof(uint64_t))
    {
    }
    const static size_t size = crypto_sign_BYTES + sizeof(uint64_t) + crypto_generichash_BYTES + sizeof(uint64_t) + input_len;
};

#endif