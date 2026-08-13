#include "downstream.h"

#include <cstring>
#include <iostream>
#include <iomanip>
#include <arpa/inet.h>

void dumpMemory(const void *addr, size_t length)
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

DownstreamNode::DownstreamNode(int port, NodeType node_type, uint64_t id, int num_nodes)
    : Node(port, node_type, id, num_nodes)
{
    memset(hb_available_pairs_, 0, sizeof(hb_available_pairs_));
}

void DownstreamNode::verify_upstream_tags(uint64_t round, uint8_t *tag_buf)
{
    for (uint64_t i = 0; i < FAULTY_NODES + 1; ++i)
    {
        if (crypto_sign_verify_detached(
                tag_buf + i * crypto_sign_BYTES,
                reinterpret_cast<uint8_t *>(&round),
                sizeof(round),
                upstream_pub_keys_.at(i).data()))
        {
            throw std::runtime_error("Fail to verify tag in HB from upstream " + std::to_string(i));
        }
    }
    memcpy(received_tags_, tag_buf, sizeof(received_tags_));
}

/**
 * This function should run in a loop until the time to send ACCEPT arrives.
 */
void DownstreamNode::receive_hb_and_proposal(uint64_t round)
{
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    Message msg;
    int bytes_recvd = recvfrom(socket_fd_, &msg, MAXBUF, 0,
                               (struct sockaddr *)&client_addr, &len);
    if (bytes_recvd <= 0)
        return;

    profiler.mark_start();

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto ms_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration) -
        std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto ms = ms_duration.count();

    MessageType msg_type = msg.type;

    if (msg_type == MessageType::HEARTBEAT)
    {
        
        /* Verify the round and the tag */
        int from_id = ntohs(client_addr.sin_port) - BASE_PORT_UPSTREAM;
        // std::cout << "Receiving a HB from " << from_id << std::endl;

        HeartbeatMsg hb_msg(msg.content);
        if (*hb_msg.round != round)
        {
            throw std::runtime_error("Expect round " + std::to_string(round) + " got " + std::to_string(*hb_msg.round));
        }
        if (crypto_sign_verify_detached(hb_msg.sender_tag,
                                        hb_msg.all_tags,
                                        (FAULTY_NODES + 1) * crypto_sign_BYTES,
                                        upstream_pub_keys_.at(from_id).data()))
        {
            throw std::runtime_error("Invalid HB signature from upstream " + std::to_string(from_id));
        }

        /* check if it is the first HB and verify all tags */
        if (most_recent_round < round)
        {
            /* The first hb or proposal in this round */
            most_recent_round = round;
            memset(hb_available_pairs_, 0, sizeof(hb_available_pairs_));

            verify_upstream_tags(round, hb_msg.all_tags);

            smallest_proposal_ = std::make_pair(ms - SEND_HB_TIME, my_id_);
            // std::cout << "Propose " << smallest_proposal_.first << " from HB\n";
            memcpy(&evid_smallest_proposal, &msg, sizeof(MessageType) + HeartbeatMsg::size);
        }
        else
        {
            if (memcmp(hb_msg.all_tags, received_tags_, sizeof(received_tags_)))
            {
                dumpMemory(hb_msg.all_tags, sizeof(received_tags_));
                dumpMemory(received_tags_, sizeof(received_tags_));
                throw std::runtime_error("Inconsistent tag in HB from " + std::to_string(from_id));
            }
            if (std::make_pair(static_cast<uint64_t>(ms) - SEND_HB_TIME, my_id_) < smallest_proposal_)
            {
                throw std::runtime_error("Smallest proposal must be the first message received");
            }
        }

        /* Send the proposal based on the HB */
        /* Construct the proposal */
        Message msg_proposal;
        msg_proposal.type = MessageType::PROPOSAL;
        ProposalMsg msg_proposal_helper(msg_proposal.content);
        *msg_proposal_helper.latency = ms - SEND_HB_TIME;
        *msg_proposal_helper.upstream_id = from_id;
        memcpy(msg_proposal_helper.heartbeat_msg_buf, msg.content, HeartbeatMsg::size);

        unsigned long long siglen;
        crypto_sign_detached(
            msg_proposal_helper.tag,
            &siglen,
            msg_proposal.content,
            ProposalMsg::size - crypto_sign_BYTES,
            my_sec_key_.data());

        if (crypto_sign_verify_detached(msg_proposal_helper.tag,
                                        msg_proposal.content,
                                        ProposalMsg::size - crypto_sign_BYTES,
                                        downstream_pub_keys_.at(my_id_).data()))
        {
            throw std::runtime_error("Cannot verify itself!! " + std::to_string(my_id_));
        }

        /* Send the proposal */
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        for (int i = 0; i < num_nodes_; ++i)
        {
            if (i == static_cast<int>(my_id_))
                continue;
            dest_addr.sin_port = htons(BASE_PORT_DOWNSTREAM + i);
            auto sent = sendto(socket_fd_,
                               &msg_proposal,
                               sizeof(MessageType) + ProposalMsg::size,
                               0,
                               (struct sockaddr *)&dest_addr,
                               sizeof(dest_addr));
            if (static_cast<uint64_t>(sent) != sizeof(MessageType) + ProposalMsg::size)
            {
                throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(TagExchangeMsg::size));
            }
        }

        /* Update the available pairs */
        hb_available_pairs_[from_id][my_id_] = true;
    }
    else if (msg_type == MessageType::PROPOSAL)
    {
        /* Verify the round and the tag */
        uint64_t from_id = ntohs(client_addr.sin_port) - BASE_PORT_DOWNSTREAM;
        // std::cout << "Receiving a proposal from " << from_id << std::endl;

        ProposalMsg proposal_msg(msg.content);
        HeartbeatMsg hb_attached(proposal_msg.heartbeat_msg_buf);

        if (crypto_sign_verify_detached(proposal_msg.tag,
                                        msg.content,
                                        ProposalMsg::size - crypto_sign_BYTES,
                                        downstream_pub_keys_.at(from_id).data()))
        {
            dumpMemory(proposal_msg.tag, crypto_sign_BYTES);
            throw std::runtime_error("Invalid proposal signature from downstream " + std::to_string(from_id));
        }

        /* check if it is the first HB and verify all tags */
        if (most_recent_round < round)
        {
            /* The first hb or proposal in this round */
            most_recent_round = round;
            memset(hb_available_pairs_, 0, sizeof(hb_available_pairs_));
            verify_upstream_tags(round, hb_attached.all_tags);
            smallest_proposal_ = std::make_pair(ms - SEND_HB_TIME, from_id);
            memcpy(&evid_smallest_proposal, &msg, sizeof(MessageType) + HeartbeatMsg::size);
        }
        else
        {
            if (memcmp(hb_attached.all_tags, received_tags_, sizeof(received_tags_)))
            {
                throw std::runtime_error("Inconsistent tag in HB from " + std::to_string(from_id));
            }
            if (std::make_pair(static_cast<uint64_t>(ms) - SEND_HB_TIME, from_id) < smallest_proposal_)
            {
                smallest_proposal_ =
                    std::make_pair(static_cast<uint64_t>(ms) - SEND_HB_TIME, from_id);
                memcpy(&evid_smallest_proposal,
                       &msg,
                       sizeof(MessageType) + HeartbeatMsg::size);
            }
        }
        hb_available_pairs_[*proposal_msg.upstream_id][from_id] = true;
    }
    else
    {
        throw std::runtime_error("Expect HB or proposal, got " + std::to_string((int)msg_type));
    }
    profiler.mark_end();
}

void DownstreamNode::send_accept(uint64_t round)
{
    profiler.mark_start();

    Message acc_msg;
    acc_msg.type = MessageType::ACCEPT;

    AcceptMsg acc_msg_helper(acc_msg.content);
    *acc_msg_helper.round = round;
    *acc_msg_helper.latency = smallest_proposal_.first;

    // std::cout << "sending in ACC: " << *acc_msg_helper.latency << std::endl;

    unsigned long long siglen;
    crypto_sign_detached(acc_msg_helper.tag,
                         &siglen,
                         acc_msg.content,
                         AcceptMsg::size - crypto_sign_BYTES, my_sec_key_.data());

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    for (int i = 0; i < num_nodes_; ++i)
    {
        if (i == static_cast<int>(my_id_))
            continue;
        dest_addr.sin_port = htons(BASE_PORT_DOWNSTREAM + i);
        auto sent = sendto(socket_fd_,
                           &acc_msg,
                           sizeof(MessageType) + AcceptMsg::size,
                           0,
                           (struct sockaddr *)&dest_addr,
                           sizeof(dest_addr));
        if (static_cast<uint64_t>(sent) != sizeof(MessageType) + AcceptMsg::size)
        {
            throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(AcceptMsg::size));
        }
    }

    profiler.mark_end();
}

void DownstreamNode::recv_accept(uint64_t round)
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
    if (msg_type != MessageType::ACCEPT)
    {
        throw std::runtime_error("Expect ACC, got " + std::to_string((int)msg_type));
    }
    uint64_t from_id = ntohs(client_addr.sin_port) - BASE_PORT_DOWNSTREAM;
    // std::cout << "Receiving an ACC from " << from_id << std::endl;

    AcceptMsg acc_msg_helper(msg.content);
    if (crypto_sign_verify_detached(acc_msg_helper.tag,
                                    msg.content,
                                    AcceptMsg::size - crypto_sign_BYTES,
                                    downstream_pub_keys_.at(from_id).data()))
    {
        throw std::runtime_error("Invalid ACC signature from upstream " + std::to_string(from_id));
    }
    if (*acc_msg_helper.round != round)
    {
        throw std::runtime_error("Expect round " + std::to_string(round) + " got " + std::to_string(*acc_msg_helper.round));
    }
    if (*acc_msg_helper.latency != smallest_proposal_.first)
    {
        throw std::runtime_error("ACC latency not equal. Expect " + std::to_string(smallest_proposal_.first) + " got " + std::to_string(*acc_msg_helper.latency));
    }
    profiler.mark_end();
}

void DownstreamNode::run()
{
    while (1)
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto ms_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(duration) -
            std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto ms = ms_duration.count();
        if (ms < SEND_ACC_TIME)
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
            seconds =
                std::chrono::duration_cast<std::chrono::seconds>(duration).count();
            auto ms_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(duration) -
                std::chrono::duration_cast<std::chrono::seconds>(duration);
            auto ms = ms_duration.count();
            if (ms >= SEND_ACC_TIME)
            {
                break;
            }
            receive_hb_and_proposal(seconds);
        }
        send_accept(seconds);
        while (1)
        {
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto ms_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(duration) -
                std::chrono::duration_cast<std::chrono::seconds>(duration);
            auto ms = ms_duration.count();
            if (ms < SEND_ACC_TIME)
            {
                break;
            }
            recv_accept(seconds);
        }
        uint64_t duration = profiler.reset_and_output();
        std::cerr << duration << std::endl;
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

    DownstreamNode node(port, NodeType::DOWNSTREAM, server_id, num_nodes);
    node.run();
    return 0;
}