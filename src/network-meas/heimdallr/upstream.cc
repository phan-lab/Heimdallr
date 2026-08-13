#include "upstream.h"

#include <iostream>
#include <cstring>
#include <arpa/inet.h>

UpstreamNode::UpstreamNode(
    int port, NodeType node_type, uint64_t id, int num_nodes)
    : Node(port, node_type, id, num_nodes)
{
    received_tags_.resize(num_nodes);
}

void UpstreamNode::run_tag_exchange(uint64_t round)
{
    profiler.mark_start();

    tag_senders_.clear();
    tag_senders_.insert(my_id_);

    struct Message msg;
    msg.type = MessageType::TAG_EXCHANGE;
    TagExchangeMsg tag_exchange_msg(
        reinterpret_cast<uint8_t *>(msg.content));
    memcpy(msg.content, &round, sizeof(round));
    unsigned long long siglen;
    crypto_sign_detached(tag_exchange_msg.tag,
                         &siglen,
                         msg.content,
                         sizeof(round),
                         my_sec_key_.data());
    memcpy(received_tags_.at(my_id_).data(),
           tag_exchange_msg.tag,
           crypto_sign_BYTES);

    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    for (uint64_t i = 0; i < static_cast<uint64_t>(num_nodes_); ++i)
    {
        /* Send the tag to each upstream server, excl. myself */
        if (i == my_id_)
            continue;
        client_addr.sin_port = htons(BASE_PORT_UPSTREAM + i);
        auto sent = sendto(socket_fd_, &msg,
                           sizeof(MessageType) + TagExchangeMsg::size, 0,
                           (struct sockaddr *)&client_addr, sizeof(client_addr));
        if (static_cast<uint64_t>(sent) !=
            TagExchangeMsg::size + sizeof(MessageType))
        {
            throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(TagExchangeMsg::size));
        }
    }

    profiler.mark_end();
}

/**
 * This function should run in a loop until the time to send HB arrives.
 */
void UpstreamNode::recv_tags(uint64_t round)
{
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    Message msg;
    int bytes_recvd = recvfrom(socket_fd_, &msg, MAXBUF, 0,
                               (struct sockaddr *)&client_addr, &len);
    if (bytes_recvd <= 0)
        return;

    profiler.mark_start();

    MessageType msg_type = msg.type;
    if (msg_type != MessageType::TAG_EXCHANGE)
    {
        throw std::runtime_error("Expect tag exchange, got " + std::to_string((int)msg_type));
    }

    TagExchangeMsg tag_ex_msg(msg.content);

    /* Verify the tag */
    int from_id = ntohs(client_addr.sin_port) - BASE_PORT_UPSTREAM;
    if (round != *tag_ex_msg.round)
    {
        throw std::runtime_error("Expect round " + std::to_string(round) + " got " + std::to_string(*tag_ex_msg.round));
    }

    if (crypto_sign_verify_detached(tag_ex_msg.tag,
                                    reinterpret_cast<const uint8_t *>(tag_ex_msg.round),
                                    sizeof(*tag_ex_msg.round),
                                    upstream_pub_keys_.at(from_id).data()))
    {
        throw std::runtime_error("Invalid signature from upstream " + std::to_string(from_id));
    }

    /* Add to storage */
    tag_senders_.insert(from_id);
    memcpy(received_tags_.at(from_id).data(), tag_ex_msg.tag, crypto_sign_BYTES);

    if (tag_senders_.size() < FAULTY_NODES + 1)
    {
        profiler.mark_end();
        return;
    }

    HeartbeatMsg hb_msg(prepared_hb_.content);
    prepared_hb_.type = MessageType::HEARTBEAT;
    *hb_msg.round = round;

    /* Check the address is continuous */
    assert(received_tags_.at(0).data() + crypto_sign_BYTES * FAULTY_NODES == received_tags_.at(FAULTY_NODES).data());
    memcpy(hb_msg.all_tags, received_tags_.data(),
           received_tags_.size() * crypto_sign_BYTES);
    unsigned long long siglen;
    crypto_sign_detached(hb_msg.sender_tag,
                         &siglen,
                         hb_msg.all_tags,
                         (FAULTY_NODES + 1) * crypto_sign_BYTES,
                         my_sec_key_.data());
    profiler.mark_end();
}

void UpstreamNode::run_send_hb()
{
    profiler.mark_start();
    /* Send hb to downstream nodes */
    if (tag_senders_.size() != FAULTY_NODES + 1)
    {
        throw std::runtime_error("Only got tags from " + std::to_string(tag_senders_.size()) + " nodes");
    }
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    for (uint64_t i = 0; i < static_cast<uint64_t>(num_nodes_); ++i)
    {
        /* Send the tag to each downstream server */
        client_addr.sin_port = htons(BASE_PORT_DOWNSTREAM + i);
        auto sent = sendto(socket_fd_, &prepared_hb_,
                           sizeof(MessageType) + HeartbeatMsg::size, 0,
                           (struct sockaddr *)&client_addr, sizeof(client_addr));
        if (static_cast<uint64_t>(sent) !=
            HeartbeatMsg::size + sizeof(MessageType))
        {
            throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(HeartbeatMsg::size));
        }
    }

    tag_senders_.clear();

    profiler.mark_end();
    uint64_t duration = profiler.reset_and_output();
    std::cerr << duration << std::endl;
}

void UpstreamNode::run()
{
    while (1)
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto ms_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(duration) -
            std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto ms = ms_duration.count();
        if (ms < TAG_EXCHANGE_TIME)
        {
            break;
        }
    }
    while (1)
    {
        int64_t seconds;
        while (1)
        {
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto ms_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(duration) -
                std::chrono::duration_cast<std::chrono::seconds>(duration);
            auto ms = ms_duration.count();
            if (ms >= TAG_EXCHANGE_TIME)
            {
                seconds =
                    std::chrono::duration_cast<std::chrono::seconds>(duration).count();
                break;
            }
        }
        run_tag_exchange(seconds);
        while (1)
        {
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto ms_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(duration) -
                std::chrono::duration_cast<std::chrono::seconds>(duration);
            auto ms = ms_duration.count();
            if (ms >= SEND_HB_TIME)
            {
                break;
            }
            recv_tags(seconds);
        }
        run_send_hb();
        while (1)
        {
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto ms_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(duration) -
                std::chrono::duration_cast<std::chrono::seconds>(duration);
            auto ms = ms_duration.count();
            if (ms < TAG_EXCHANGE_TIME)
            {
                break;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <node_id> <num_nodes>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    uint32_t server_id = std::stoi(argv[2]);
    int num_nodes = std::stoi(argv[3]);

    UpstreamNode node(port, NodeType::UPSTREAM, server_id, num_nodes);
    node.run();

    return 0;
}