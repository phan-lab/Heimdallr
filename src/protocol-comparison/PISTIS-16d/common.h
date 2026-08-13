#ifndef _COMMON_H_
#define _COMMON_H_

#define BASE_PORT_UPSTREAM 8888
#define BASE_PORT_DOWNSTREAM 9000

#define SEND_PREPREPARE_TIME 50 /* At the 500-th ms of each second */

#ifndef FAULTY_NODES
#define FAULTY_NODES 1
#endif

#define MAXBUF 65536

#include <cstdint>
#include <cassert>
#include <cstring>
#include <array>
#include <vector>
#include <iostream>
#include <sodium.h>
#include <iomanip>
#include <iostream>

typedef uint32_t node_id_t;
typedef uint64_t seq_num_t;
const static unsigned long long input_len = 1024;

enum class MessageType
{
    INVALID = -1,
    PLAIN_HB,
    ECHO,
    DELIVER
};

#pragma pack(push, 1)
typedef struct Signature
{
    seq_num_t round_id;
    node_id_t node_id;
    uint8_t sig[crypto_sign_BYTES];
} Signature;

typedef struct Heartbeat
{
    node_id_t from_node_id;
    seq_num_t seq_num;
    node_id_t num_pocs;
    std::vector<Signature> poc_sigs;
} Heartbeat;

typedef struct Message
{
    MessageType type;
    uint8_t buf[MAXBUF];
} Message;

typedef struct EchoMessage
{
    node_id_t from_node_id;
    seq_num_t seq_num;
    uint32_t val_size;
    std::vector<uint8_t> val;
    uint32_t num_sigs;
    std::vector<Signature> echo_sigs;
} EchoMessage;

typedef struct DeliverMessage
{
    node_id_t from_node_id;
    seq_num_t seq_num;
    uint32_t val_size;
    std::vector<uint8_t> val;
    uint32_t num_echo_sigs;
    std::vector<Signature> echo_sigs;
    uint32_t num_deliver_sigs;
    std::vector<Signature> deliver_sigs;
} DeliverMessage;
#pragma pack(pop)

class EchoMessageHelper
{
public:
    static void dump(const EchoMessage &msg)
    {
        std::cerr << "EchoMessage from_node_id: " << msg.from_node_id
                  << ", seq_num: " << msg.seq_num
                  << ", val_size: " << msg.val_size
                  << ", num_sigs: " << msg.num_sigs << std::endl;
    }
    static void sign(const uint8_t *key, const EchoMessage &msg, uint8_t *sig)
    {
        crypto_sign_state state;
        crypto_sign_init(&state);
        crypto_sign_update(&state, reinterpret_cast<const uint8_t *>(&msg), sizeof(msg.from_node_id) + sizeof(msg.seq_num) + sizeof(msg.val_size));
        if (msg.val_size > 0)
        {
            crypto_sign_update(&state, msg.val.data(), msg.val_size);
        }
        crypto_sign_final_create(&state, sig, nullptr, key);
    }

    static int verify(uint8_t *key, const EchoMessage &msg, const uint8_t *sig)
    {
        crypto_sign_state state;
        crypto_sign_init(&state);
        crypto_sign_update(&state, reinterpret_cast<const uint8_t *>(&msg), sizeof(msg.from_node_id) + sizeof(msg.seq_num) + sizeof(msg.val_size));
        if (msg.val_size > 0)
        {
            crypto_sign_update(&state, msg.val.data(), msg.val_size);
        }
        return crypto_sign_final_verify(&state, sig, key);
    }
    static uint32_t serialize(const EchoMessage &msg, uint8_t *buf)
    {
        uint8_t *ptr = buf;
        uint8_t *buf_end = buf + MAXBUF;

        // Check total size needed before serializing
        uint32_t needed_size = sizeof(msg.from_node_id) + sizeof(msg.seq_num) + 
                              sizeof(msg.val_size) + msg.val_size + sizeof(msg.num_sigs) + 
                              msg.num_sigs * sizeof(Signature);
        if (needed_size > MAXBUF) {
            throw std::runtime_error("EchoMessage serialize: buffer overflow, needed " + 
                                   std::to_string(needed_size) + " bytes but buffer is " + 
                                   std::to_string(MAXBUF) + " bytes");
        }

        // Serialize from_node_id
        if (ptr + sizeof(msg.from_node_id) > buf_end) throw std::runtime_error("Buffer overflow in EchoMessage serialize");
        memcpy(ptr, &msg.from_node_id, sizeof(msg.from_node_id));
        ptr += sizeof(msg.from_node_id);

        // Serialize seq_num
        if (ptr + sizeof(msg.seq_num) > buf_end) throw std::runtime_error("Buffer overflow in EchoMessage serialize");
        memcpy(ptr, &msg.seq_num, sizeof(msg.seq_num));
        ptr += sizeof(msg.seq_num);

        // Serialize val_size
        if (ptr + sizeof(msg.val_size) > buf_end) throw std::runtime_error("Buffer overflow in EchoMessage serialize");
        memcpy(ptr, &msg.val_size, sizeof(msg.val_size));
        ptr += sizeof(msg.val_size);

        // Serialize val data
        if (msg.val_size > 0)
        {
            if (ptr + msg.val_size > buf_end) throw std::runtime_error("Buffer overflow in EchoMessage serialize");
            memcpy(ptr, msg.val.data(), msg.val_size);
            ptr += msg.val_size;
        }

        // Serialize num_sigs
        if (ptr + sizeof(msg.num_sigs) > buf_end) throw std::runtime_error("Buffer overflow in EchoMessage serialize");
        memcpy(ptr, &msg.num_sigs, sizeof(msg.num_sigs));
        ptr += sizeof(msg.num_sigs);

        // Serialize signatures
        assert(msg.num_sigs == msg.echo_sigs.size());
        for (uint32_t i = 0; i < msg.num_sigs; i++)
        {
            if (ptr + sizeof(Signature) > buf_end) throw std::runtime_error("Buffer overflow in EchoMessage serialize");
            memcpy(ptr, &msg.echo_sigs[i], sizeof(Signature));
            ptr += sizeof(Signature);
        }

        return ptr - buf;
    }

    static void deserialize(const uint8_t *buf, EchoMessage &msg)
    {
        const uint8_t *ptr = buf;

        // Deserialize from_node_id
        memcpy(&msg.from_node_id, ptr, sizeof(msg.from_node_id));
        ptr += sizeof(msg.from_node_id);

        // Deserialize seq_num
        memcpy(&msg.seq_num, ptr, sizeof(msg.seq_num));
        ptr += sizeof(msg.seq_num);

        // Deserialize val_size
        memcpy(&msg.val_size, ptr, sizeof(msg.val_size));
        ptr += sizeof(msg.val_size);

        // Deserialize val data
        msg.val.resize(msg.val_size);
        if (msg.val_size > 0)
        {
            memcpy(msg.val.data(), ptr, msg.val_size);
            ptr += msg.val_size;
        }

        // Deserialize num_sigs
        memcpy(&msg.num_sigs, ptr, sizeof(msg.num_sigs));
        ptr += sizeof(msg.num_sigs);

        // Deserialize signatures
        msg.echo_sigs.resize(msg.num_sigs);
        for (uint32_t i = 0; i < msg.num_sigs; i++)
        {
            memcpy(&msg.echo_sigs[i], ptr, sizeof(Signature));
            ptr += sizeof(Signature);
        }
    }
};

class DeliverMessageHelper
{
public:
    static void dumpMemory(const void *addr, size_t length)
    {
        const uint8_t *ptr = static_cast<const uint8_t *>(addr);

        for (size_t i = 0; i < length; ++i)
        {
            std::cerr << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(ptr[i]) << " ";

            // Add extra space every 8 bytes
            if ((i + 1) % 8 == 0)
            {
                std::cerr << " ";
            }

            // New line every 16 bytes
            if ((i + 1) % 16 == 0)
            {
                std::cerr << std::endl;
            }
        }

        // Add final newline if needed
        if (length % 16 != 0)
        {
            std::cerr << std::endl;
        }

        std::cerr << std::dec << std::endl; // Reset to decimal format
    }
    static void sign(const uint8_t *key, DeliverMessage &msg, node_id_t signer_id, seq_num_t seq)
    {
        crypto_sign_state state;
        crypto_sign_init(&state);
        crypto_sign_update(&state, reinterpret_cast<const uint8_t *>(&msg), sizeof(msg.from_node_id) + sizeof(msg.seq_num) + sizeof(msg.val_size));
        if (msg.val_size > 0)
        {
            crypto_sign_update(&state, msg.val.data(), msg.val_size);
        }
        crypto_sign_update(&state, reinterpret_cast<const uint8_t *>(&msg.num_echo_sigs), sizeof(msg.num_echo_sigs));
        for (const auto &sig : msg.echo_sigs)
        {
            crypto_sign_update(&state, reinterpret_cast<const uint8_t *>(&sig), sizeof(sig.round_id) + sizeof(sig.node_id) + sizeof(sig.sig));
        }
        Signature sig;
        sig.node_id = signer_id;
        sig.round_id = seq;
        crypto_sign_final_create(&state, sig.sig, nullptr, key);
        msg.deliver_sigs.push_back(sig);
    }

    static int verify(std::vector<std::array<uint8_t, crypto_sign_PUBLICKEYBYTES>> &pub_keys,
                      const DeliverMessage &msg)
    {
        crypto_sign_state state;
        crypto_sign_init(&state);
        crypto_sign_update(&state, reinterpret_cast<const uint8_t *>(&msg), sizeof(msg.from_node_id) + sizeof(msg.seq_num) + sizeof(msg.val_size));
        if (msg.val_size > 0)
        {
            crypto_sign_update(&state, msg.val.data(), msg.val_size);
        }
        crypto_sign_update(&state, reinterpret_cast<const uint8_t *>(&msg.num_echo_sigs), sizeof(msg.num_echo_sigs));
        for (const auto &sig : msg.echo_sigs)
        {
            crypto_sign_update(&state, reinterpret_cast<const uint8_t *>(&sig), sizeof(sig.round_id) + sizeof(sig.node_id) + sizeof(sig.sig));
        }
        // return crypto_sign_final_verify(&state, msg.deliver_sigs[0].sig, key);
        for (uint32_t i = 0; i < msg.num_deliver_sigs; i++)
        {
            auto &dlvr_sig = msg.deliver_sigs[i];
            int ret = crypto_sign_final_verify(&state, dlvr_sig.sig, pub_keys[dlvr_sig.node_id].data());
            if (ret != 0)
            {
                std::cerr << "Signature verification failed for node " << dlvr_sig.node_id
                          << " in deliver message with seq_num " << msg.seq_num << std::endl;
                // std::cerr << "This is the " << i << "th signature in the deliver message." << std::endl;
                
                dumpMemory(dlvr_sig.sig, crypto_sign_BYTES);
                return ret;
            }
            else
            {
                // std::cerr << "Signature verification succeeded for node " << dlvr_sig.node_id
                        //   << " in deliver message with seq_num " << msg.seq_num << std::endl;
            }
        }
        return 0;
    }
    static uint32_t serialize(const DeliverMessage &msg, uint8_t *buf)
    {
        uint8_t *ptr = buf;
        uint8_t *buf_end = buf + MAXBUF;

        // Check total size needed before serializing
        uint32_t needed_size = sizeof(msg.from_node_id) + sizeof(msg.seq_num) + 
                              sizeof(msg.val_size) + msg.val_size + sizeof(msg.num_echo_sigs) +
                              msg.num_echo_sigs * sizeof(Signature) + sizeof(msg.num_deliver_sigs) +
                              msg.num_deliver_sigs * sizeof(Signature);
        if (needed_size > MAXBUF) {
            throw std::runtime_error("DeliverMessage serialize: buffer overflow, needed " + 
                                   std::to_string(needed_size) + " bytes but buffer is " + 
                                   std::to_string(MAXBUF) + " bytes");
        }

        // Serialize from_node_id
        if (ptr + sizeof(msg.from_node_id) > buf_end) throw std::runtime_error("Buffer overflow in DeliverMessage serialize");
        memcpy(ptr, &msg.from_node_id, sizeof(msg.from_node_id));
        ptr += sizeof(msg.from_node_id);

        // Serialize seq_num
        if (ptr + sizeof(msg.seq_num) > buf_end) throw std::runtime_error("Buffer overflow in DeliverMessage serialize");
        memcpy(ptr, &msg.seq_num, sizeof(msg.seq_num));
        ptr += sizeof(msg.seq_num);

        // Serialize val_size
        if (ptr + sizeof(msg.val_size) > buf_end) throw std::runtime_error("Buffer overflow in DeliverMessage serialize");
        memcpy(ptr, &msg.val_size, sizeof(msg.val_size));
        ptr += sizeof(msg.val_size);

        // Serialize val data
        if (msg.val_size > 0)
        {
            if (ptr + msg.val_size > buf_end) throw std::runtime_error("Buffer overflow in DeliverMessage serialize");
            memcpy(ptr, msg.val.data(), msg.val_size);
            ptr += msg.val_size;
        }

        // Serialize num_echo_sigs
        if (ptr + sizeof(msg.num_echo_sigs) > buf_end) throw std::runtime_error("Buffer overflow in DeliverMessage serialize");
        memcpy(ptr, &msg.num_echo_sigs, sizeof(msg.num_echo_sigs));
        ptr += sizeof(msg.num_echo_sigs);

        // Serialize echo signatures
        for (uint32_t i = 0; i < msg.num_echo_sigs; i++)
        {
            if (ptr + sizeof(Signature) > buf_end) throw std::runtime_error("Buffer overflow in DeliverMessage serialize");
            memcpy(ptr, &msg.echo_sigs[i], sizeof(Signature));
            ptr += sizeof(Signature);
        }

        // Serialize num_deliver_sigs
        if (ptr + sizeof(msg.num_deliver_sigs) > buf_end) throw std::runtime_error("Buffer overflow in DeliverMessage serialize");
        memcpy(ptr, &msg.num_deliver_sigs, sizeof(msg.num_deliver_sigs));
        ptr += sizeof(msg.num_deliver_sigs);

        // Serialize deliver signatures
        for (uint32_t i = 0; i < msg.num_deliver_sigs; i++)
        {
            if (ptr + sizeof(Signature) > buf_end) throw std::runtime_error("Buffer overflow in DeliverMessage serialize");
            memcpy(ptr, &msg.deliver_sigs[i], sizeof(Signature));
            ptr += sizeof(Signature);
        }

        return ptr - buf;
    }

    static void deserialize(const uint8_t *buf, DeliverMessage &msg)
    {
        const uint8_t *ptr = buf;

        // Deserialize from_node_id
        memcpy(&msg.from_node_id, ptr, sizeof(msg.from_node_id));
        ptr += sizeof(msg.from_node_id);

        // Deserialize seq_num
        memcpy(&msg.seq_num, ptr, sizeof(msg.seq_num));
        ptr += sizeof(msg.seq_num);

        // Deserialize val_size
        memcpy(&msg.val_size, ptr, sizeof(msg.val_size));
        ptr += sizeof(msg.val_size);

        // Deserialize val data
        msg.val.resize(msg.val_size);
        if (msg.val_size > 0)
        {
            memcpy(msg.val.data(), ptr, msg.val_size);
            ptr += msg.val_size;
        }

        // Deserialize num_echo_sigs
        memcpy(&msg.num_echo_sigs, ptr, sizeof(msg.num_echo_sigs));
        ptr += sizeof(msg.num_echo_sigs);

        // Deserialize echo signatures
        msg.echo_sigs.resize(msg.num_echo_sigs);
        for (uint32_t i = 0; i < msg.num_echo_sigs; i++)
        {
            memcpy(&msg.echo_sigs[i], ptr, sizeof(Signature));
            ptr += sizeof(Signature);
        }

        // Deserialize num_deliver_sigs
        memcpy(&msg.num_deliver_sigs, ptr, sizeof(msg.num_deliver_sigs));
        ptr += sizeof(msg.num_deliver_sigs);

        // Deserialize deliver signatures
        msg.deliver_sigs.resize(msg.num_deliver_sigs);
        for (uint32_t i = 0; i < msg.num_deliver_sigs; i++)
        {
            memcpy(&msg.deliver_sigs[i], ptr, sizeof(Signature));
            ptr += sizeof(Signature);
        }
    }
};

class HeartbeatHelper
{
public:
    static uint32_t serialize(const Heartbeat &hb, uint8_t *buf)
    {
        uint8_t *ptr = buf;
        uint8_t *buf_end = buf + MAXBUF;

        // Check total size needed before serializing
        uint32_t needed_size = sizeof(hb.from_node_id) + sizeof(hb.seq_num) + 
                              sizeof(hb.num_pocs) + hb.num_pocs * sizeof(Signature);
        if (needed_size > MAXBUF) {
            throw std::runtime_error("Heartbeat serialize: buffer overflow, needed " + 
                                   std::to_string(needed_size) + " bytes but buffer is " + 
                                   std::to_string(MAXBUF) + " bytes");
        }

        // Serialize from_node_id
        if (ptr + sizeof(hb.from_node_id) > buf_end) throw std::runtime_error("Buffer overflow in Heartbeat serialize");
        memcpy(ptr, &hb.from_node_id, sizeof(hb.from_node_id));
        ptr += sizeof(hb.from_node_id);

        // Serialize seq_num
        if (ptr + sizeof(hb.seq_num) > buf_end) throw std::runtime_error("Buffer overflow in Heartbeat serialize");
        memcpy(ptr, &hb.seq_num, sizeof(hb.seq_num));
        ptr += sizeof(hb.seq_num);

        // Serialize num_pocs
        if (ptr + sizeof(hb.num_pocs) > buf_end) throw std::runtime_error("Buffer overflow in Heartbeat serialize");
        memcpy(ptr, &hb.num_pocs, sizeof(hb.num_pocs));
        ptr += sizeof(hb.num_pocs);

        // Serialize signatures
        for (uint32_t i = 0; i < hb.num_pocs; i++)
        {
            if (ptr + sizeof(Signature) > buf_end) throw std::runtime_error("Buffer overflow in Heartbeat serialize");
            memcpy(ptr, &hb.poc_sigs[i], sizeof(Signature));
            ptr += sizeof(Signature);
        }

        return ptr - buf;
    }

    static void deserialize(const uint8_t *buf, Heartbeat &hb)
    {
        const uint8_t *ptr = buf;

        // Deserialize from_node_id
        memcpy(&hb.from_node_id, ptr, sizeof(hb.from_node_id));
        ptr += sizeof(hb.from_node_id);

        // Deserialize seq_num
        memcpy(&hb.seq_num, ptr, sizeof(hb.seq_num));
        ptr += sizeof(hb.seq_num);

        // Deserialize num_pocs
        memcpy(&hb.num_pocs, ptr, sizeof(hb.num_pocs));
        ptr += sizeof(hb.num_pocs);

        // Deserialize signatures
        hb.poc_sigs.resize(hb.num_pocs);
        for (uint32_t i = 0; i < hb.num_pocs; i++)
        {
            memcpy(&hb.poc_sigs[i], ptr, sizeof(Signature));
            ptr += sizeof(Signature);
        }
    }
};

class PlainHeartbeatHelper
{
public:
    static uint32_t serialize(const Heartbeat &hb, uint8_t *buf)
    {
        uint8_t *ptr = buf;

        // Serialize message type
        MessageType type = MessageType::PLAIN_HB;
        memcpy(ptr, &type, sizeof(type));
        ptr += sizeof(type);

        // Serialize heartbeat
        uint32_t hb_size = HeartbeatHelper::serialize(hb, ptr);

        return sizeof(type) + hb_size;
    }

    static void deserialize(const uint8_t *buf, Heartbeat &hb)
    {
        const uint8_t *ptr = buf;

        // Deserialize and verify message type
        MessageType type;
        memcpy(&type, ptr, sizeof(type));
        assert(type == MessageType::PLAIN_HB);
        ptr += sizeof(type);

        // Deserialize heartbeat
        HeartbeatHelper::deserialize(ptr, hb);
    }
};

class HeartbeatAndEchoHelper
{
public:
    static uint32_t serialize(const Heartbeat &hb, const EchoMessage &echo_msg, uint8_t *buf)
    {
        uint8_t *ptr = buf;
        uint8_t *buf_end = buf + MAXBUF;

        // Check total size needed before serializing
        uint32_t hb_size = sizeof(hb.from_node_id) + sizeof(hb.seq_num) + 
                          sizeof(hb.num_pocs) + hb.num_pocs * sizeof(Signature);
        uint32_t echo_size = sizeof(echo_msg.from_node_id) + sizeof(echo_msg.seq_num) + 
                            sizeof(echo_msg.val_size) + echo_msg.val_size + sizeof(echo_msg.num_sigs) + 
                            echo_msg.num_sigs * sizeof(Signature);
        uint32_t needed_size = sizeof(MessageType) + hb_size + echo_size;
        
        if (needed_size > MAXBUF) {
            throw std::runtime_error("HeartbeatAndEcho serialize: buffer overflow, needed " + 
                                   std::to_string(needed_size) + " bytes but buffer is " + 
                                   std::to_string(MAXBUF) + " bytes");
        }

        // Serialize message type
        MessageType type = MessageType::ECHO;
        if (ptr + sizeof(type) > buf_end) throw std::runtime_error("Buffer overflow in HeartbeatAndEcho serialize");
        memcpy(ptr, &type, sizeof(type));
        ptr += sizeof(type);

        // Serialize heartbeat
        uint32_t actual_hb_size = HeartbeatHelper::serialize(hb, ptr);
        ptr += actual_hb_size;

        // Serialize echo message
        uint32_t actual_echo_size = EchoMessageHelper::serialize(echo_msg, ptr);

        return sizeof(type) + actual_hb_size + actual_echo_size;
    }

    static void deserialize(const uint8_t *buf, Heartbeat &hb, EchoMessage &echo_msg)
    {
        const uint8_t *ptr = buf;

        // Deserialize and verify message type
        MessageType type;
        memcpy(&type, ptr, sizeof(type));
        assert(type == MessageType::ECHO);
        ptr += sizeof(type);

        // Deserialize heartbeat
        HeartbeatHelper::deserialize(ptr, hb);

        // Calculate heartbeat size to advance pointer correctly
        uint32_t hb_size = sizeof(hb.from_node_id) + sizeof(hb.seq_num) +
                           sizeof(hb.num_pocs) + hb.num_pocs * sizeof(Signature);
        ptr += hb_size;

        // Deserialize echo message
        EchoMessageHelper::deserialize(ptr, echo_msg);
    }
};

class HeartbeatAndDeliverHelper
{
public:
    static uint32_t serialize(const Heartbeat &hb, const DeliverMessage &deliver_msg, uint8_t *buf)
    {
        uint8_t *ptr = buf;
        uint8_t *buf_end = buf + MAXBUF;

        // Check total size needed before serializing
        uint32_t hb_size = sizeof(hb.from_node_id) + sizeof(hb.seq_num) + 
                          sizeof(hb.num_pocs) + hb.num_pocs * sizeof(Signature);
        uint32_t deliver_size = sizeof(deliver_msg.from_node_id) + sizeof(deliver_msg.seq_num) + 
                               sizeof(deliver_msg.val_size) + deliver_msg.val_size + 
                               sizeof(deliver_msg.num_echo_sigs) + deliver_msg.num_echo_sigs * sizeof(Signature) +
                               sizeof(deliver_msg.num_deliver_sigs) + deliver_msg.num_deliver_sigs * sizeof(Signature);
        uint32_t needed_size = sizeof(MessageType) + hb_size + deliver_size;
        
        if (needed_size > MAXBUF) {
            throw std::runtime_error("HeartbeatAndDeliver serialize: buffer overflow, needed " + 
                                   std::to_string(needed_size) + " bytes but buffer is " + 
                                   std::to_string(MAXBUF) + " bytes");
        }

        // Serialize message type
        MessageType type = MessageType::DELIVER;
        if (ptr + sizeof(type) > buf_end) throw std::runtime_error("Buffer overflow in HeartbeatAndDeliver serialize");
        memcpy(ptr, &type, sizeof(type));
        ptr += sizeof(type);

        // Serialize heartbeat
        uint32_t actual_hb_size = HeartbeatHelper::serialize(hb, ptr);
        ptr += actual_hb_size;

        // Serialize deliver message
        uint32_t actual_deliver_size = DeliverMessageHelper::serialize(deliver_msg, ptr);

        return sizeof(type) + actual_hb_size + actual_deliver_size;
    }

    static void deserialize(const uint8_t *buf, Heartbeat &hb, DeliverMessage &deliver_msg)
    {
        const uint8_t *ptr = buf;

        // Deserialize and verify message type
        MessageType type;
        memcpy(&type, ptr, sizeof(type));
        assert(type == MessageType::DELIVER);
        ptr += sizeof(type);

        // Deserialize heartbeat
        HeartbeatHelper::deserialize(ptr, hb);

        // Calculate heartbeat size to advance pointer correctly
        uint32_t hb_size = sizeof(hb.from_node_id) + sizeof(hb.seq_num) +
                           sizeof(hb.num_pocs) + hb.num_pocs * sizeof(Signature);
        ptr += hb_size;

        // Deserialize deliver message
        DeliverMessageHelper::deserialize(ptr, deliver_msg);
    }
};

#endif