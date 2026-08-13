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

DownstreamNode::DownstreamNode(int port, NodeType node_type, uint64_t id)
    : Node(port, node_type, id, 3 * FAULTY_NODES + 1), num_reply_recvd_(0)
{
}

void DownstreamNode::process_result(Message &result_msg)
{
    profiler.mark_start();
    ReplyMsgHelper reply_msg_helper(result_msg.content);
    uint64_t job_id = *reply_msg_helper.job_id;
    if (job_id > current_job_id_)
    {

        /* verify signature */
        if (crypto_sign_verify_detached(reply_msg_helper.sig,
                                        reinterpret_cast<uint8_t *>(reply_msg_helper.job_id),
                                        ReplyMsgHelper::size - crypto_sign_BYTES,
                                        upstream_pub_keys_[*reply_msg_helper.sender_id].data()) != 0)
        {
            std::cerr << *reply_msg_helper.sender_id << std::endl;
             dumpMemory(reply_msg_helper.sig, crypto_sign_BYTES);
            dumpMemory(reply_msg_helper.job_id, ReplyMsgHelper::size - crypto_sign_BYTES);
            dumpMemory(upstream_pub_keys_[my_id_].data(), crypto_sign_PUBLICKEYBYTES);
            throw std::runtime_error("Invalid signature in the first reply");
        }

        current_job_id_ = job_id;
        num_reply_recvd_ = 1;
        std::memcpy(result_buf_, reply_msg_helper.output, result_len);
        profiler.mark_end();
    }
    else
    {
        if (num_reply_recvd_ >= 2 * FAULTY_NODES + 1)
        {
            profiler.mark_end();
            return;
        }
        /* verify signature */
        if (crypto_sign_verify_detached(reply_msg_helper.sig,
                                        reinterpret_cast<uint8_t *>(reply_msg_helper.job_id),
                                        ReplyMsgHelper::size - crypto_sign_BYTES,
                                        upstream_pub_keys_[*reply_msg_helper.sender_id].data()) != 0)
        {
            std::cerr << *reply_msg_helper.sender_id << std::endl;
            dumpMemory(reply_msg_helper.sig, crypto_sign_BYTES);
            dumpMemory(reply_msg_helper.job_id, ReplyMsgHelper::size - crypto_sign_BYTES);
            dumpMemory(upstream_pub_keys_[my_id_].data(), crypto_sign_PUBLICKEYBYTES);
            throw std::runtime_error("Invalid signature in later replies");
        }
        if (std::memcmp(result_buf_, reply_msg_helper.output, result_len) != 0)
        {
            throw std::runtime_error("Inconsistent result");
        }
        num_reply_recvd_++;
        if (num_reply_recvd_ == 2 * FAULTY_NODES + 1)
        {
            profiler.mark_end();
            uint64_t duration = profiler.reset_and_output();
            std::cerr << duration << std::endl
                      << std::flush;
        }
        else
        {
            profiler.mark_end();
        }
    }
}

void DownstreamNode::run()
{
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        Message msg;
        int bytes_recvd = recvfrom(socket_fd_, &msg, MAXBUF, 0,
                                   (struct sockaddr *)&client_addr, &len);
        if (bytes_recvd <= 0)
            continue;

        MessageType msg_type = msg.type;

        if (msg_type == MessageType::REPLY)
        {
            process_result(msg);
        }
        else
        {
            throw std::runtime_error("Unexpected message type " + std::to_string((int)msg_type));
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <node_id>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    uint32_t node_id = std::stoi(argv[2]);

    DownstreamNode node(port, NodeType::DOWNSTREAM, node_id);
    node.run();
    return 0;
}