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
    : Node(port, node_type, id, 3 * FAULTY_NODES + 1), num_results_recvd_(0)
{
}

void DownstreamNode::process_result(Message &result_msg, int from_id)
{
    profiler.mark_start();
    ResultMsgHelper result_msg_helper(result_msg.content);

    /* verify the signature */
    if (crypto_sign_verify_detached(result_msg_helper.meta_tag,
                                    reinterpret_cast<uint8_t *>(result_msg_helper.job_id),
                                    sizeof(*result_msg_helper.job_id) + crypto_generichash_BYTES + crypto_generichash_BYTES,
                                    upstream_pub_keys_[from_id].data()) != 0)
    {
        throw std::runtime_error("Signature verification failed");
    }

    if (num_results_recvd_ == 0)
    {
        current_job_id_ = *result_msg_helper.job_id;
        memcpy(history_hash_, result_msg_helper.history_hash, crypto_generichash_BYTES);
        memcpy(result_hash_, result_msg_helper.result_hash, crypto_generichash_BYTES);
        memcpy(result_buf_, result_msg_helper.results, result_len);
    }
    else
    {
        if (memcmp(history_hash_, result_msg_helper.history_hash, crypto_generichash_BYTES) != 0)
        {
            throw std::runtime_error("History hash mismatch");
        }
        if (memcmp(result_hash_, result_msg_helper.result_hash, crypto_generichash_BYTES) != 0)
        {
            throw std::runtime_error("Result hash mismatch");
        }
        if (memcmp(result_buf_, result_msg_helper.results, result_len) != 0)
        {
            throw std::runtime_error("Result mismatch");
        }
    }
    num_results_recvd_++;
    profiler.mark_end();
    if (num_results_recvd_ == 3 * FAULTY_NODES + 1)
    {
        num_results_recvd_ = 0;
        current_job_id_++;
        auto duration = profiler.reset_and_output();
        std::cerr << duration << std::endl
                  << std::flush;
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
        int from_id = ntohs(client_addr.sin_port) - BASE_PORT_UPSTREAM;

        if (msg_type == MessageType::RESULT_TO_DOWNSTREAM)
        {
            process_result(msg, from_id);
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