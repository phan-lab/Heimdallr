#ifndef _COMMON_H_
#define _COMMON_H_

#define BASE_PORT_UPSTREAM 8888
#define BASE_PORT_DOWNSTREAM 9000

#define TAG_EXCHANGE_TIME 500 /* At the 500-th ms of each second */
#define SEND_HB_TIME 550      /* At the 550-th ms of each second */
#define SEND_ACC_TIME 800

#ifndef FAULTY_NODES
#define FAULTY_NODES 4
#endif
#define MAXBUF 8192

#include <cstdint>
#include <cassert>

#include <array>
#include <vector>

#include <sodium.h>

enum class MessageType
{
    TAG_EXCHANGE = 1,
    HEARTBEAT,
    PROPOSAL,
    ACCEPT
};

struct Message
{
    MessageType type;
    uint8_t content[MAXBUF];
};

class TagExchangeMsg
{
    /**
     * Format: | round (8B) | tag (64B) |
     */
public:
    uint64_t *round;
    uint8_t *tag;
    TagExchangeMsg(uint8_t *buf) : round(reinterpret_cast<uint64_t *>(buf)),
                                   tag(buf + sizeof(*round)) {}
    const static uint64_t size = sizeof(uint64_t) + crypto_sign_BYTES;
};

class HeartbeatMsg
{
public:
    /**
     * Format: | round (8B) | sender_tag (64B) | all tags (64 * (f + 1) Bytes)
     */
    uint64_t *round;
    uint8_t *sender_tag;
    uint8_t *all_tags;
    HeartbeatMsg(uint8_t *buf) : round(reinterpret_cast<uint64_t *>(buf)),
                                 sender_tag(buf + sizeof(*round)),
                                 all_tags(buf + sizeof(*round) + crypto_sign_BYTES) {}
    const static uint64_t size =
        sizeof(*round) + (FAULTY_NODES + 2) * crypto_sign_BYTES;
};

class ProposalMsg
{
public:
    /**
     * Format: | proposed_latency (8B) | upstream_sender_id (8B) | HB (72 + 64 * (f + 1) Bytes) | tag (64B) |
     */
    uint64_t *latency;
    uint64_t *upstream_id;
    uint8_t *heartbeat_msg_buf;
    uint8_t *tag;
    ProposalMsg(uint8_t *buf) : latency(reinterpret_cast<uint64_t *>(buf)),
                                upstream_id(reinterpret_cast<uint64_t *>(buf + sizeof(*latency))),
                                heartbeat_msg_buf(buf + sizeof(*latency) + sizeof(*upstream_id)),
                                tag(buf + sizeof(*latency) + sizeof(*upstream_id) + HeartbeatMsg::size) {}
    const static uint64_t size = sizeof(uint64_t) + sizeof(uint64_t) + HeartbeatMsg::size + crypto_sign_BYTES;
};

class AcceptMsg
{
public:
    uint64_t *round;
    uint64_t *latency;
    uint8_t *tag;
    AcceptMsg(uint8_t *buf) : round(reinterpret_cast<uint64_t *>(buf)),
                              latency(reinterpret_cast<uint64_t *>(buf + sizeof(*round))),
                              tag(buf + sizeof(*round) + sizeof(*latency)) {}
    const static uint64_t size = sizeof(*round) + sizeof(*latency) + crypto_sign_BYTES;
};

#endif