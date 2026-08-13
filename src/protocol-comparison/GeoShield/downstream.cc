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
    : Node(port, node_type, id, num_nodes), num_poc_recvd_(0)
{
}

void DownstreamNode::process_result(Message &result_msg, int from_id)
{
    profiler.mark_start();
    ResultMsgHelper result_msg_helper(result_msg.content);

    if (current_job_id < *result_msg_helper.job_id)
    {
        /* it is the first result or poc for this job */
        current_job_id = *result_msg_helper.job_id;
        memcpy(result_buf_, result_msg_helper.results, result_len);
        crypto_generichash(job_id_result_hash.data(),
                           crypto_generichash_BYTES,
                           reinterpret_cast<uint8_t *>(result_msg_helper.job_id),
                           sizeof(current_job_id) + result_len,
                           NULL, 0);
    }
    else
    {
        assert(current_job_id == *result_msg_helper.job_id);
        if (memcmp(result_buf_, result_msg_helper.results, result_len))
        {
            throw std::runtime_error("Result mismatch for job id " + std::to_string(current_job_id));
        }
    }

    /* Verify the signature */
    if (crypto_sign_verify_detached(result_msg_helper.sig,
                                    job_id_result_hash.data(),
                                    crypto_generichash_BYTES,
                                    upstream_pub_keys_.at(from_id).data()))
    {
        throw std::runtime_error("Invalid signature of result message");
    }
    profiler.mark_end();
}

bool DownstreamNode::process_poc(Message &poc_msg, int from)
{
    profiler.mark_start();
    PoCMsgHelper poc_msg_helper(poc_msg.content);
    const size_t poc_msg_size = sizeof(MessageType) + sizeof(PoCMsgHelper::job_id) + crypto_generichash_BYTES + (FAULTY_NODES + 1) * crypto_sign_BYTES;
    if (num_poc_recvd_ == 0)
    {
        memcpy(&PoC_combined_.type, &poc_msg.type, poc_msg_size);

        /* Verify all tags in the PoC */
        for (int i = 0; i < FAULTY_NODES + 1; ++i)
        {
            uint8_t *sig = &PoC_combined_helper_.sigs[i].sig[0];
            if (crypto_sign_verify_detached(
                    sig, PoC_combined_helper_.result_hash,
                    crypto_generichash_BYTES,
                    upstream_pub_keys_.at(i).data()))
            {
                dumpMemory(sig, crypto_sign_BYTES);
                throw std::runtime_error("Invalid signature in PoC from " + std::to_string(i));
            }
        }
        if (current_job_id < *poc_msg_helper.job_id)
        {
            current_job_id = *poc_msg_helper.job_id;
            memcpy(poc_msg_helper.result_hash, job_id_result_hash.data(), crypto_generichash_BYTES);
        }
        else
        {
            if (memcmp(poc_msg_helper.result_hash, job_id_result_hash.data(), crypto_generichash_BYTES))
            {
                std::cout << "from " << from;
                std::cout << current_job_id << " " << *poc_msg_helper.job_id << std::endl
                          << std::flush;
                throw std::runtime_error("Hash in PoC does not match previous results");
            }
        }
    }
    else
    {
        if (memcmp(&PoC_combined_, &poc_msg, poc_msg_size))
        {
            dumpMemory(&PoC_combined_, poc_msg_size);
            dumpMemory(&poc_msg, poc_msg_size);
            throw std::runtime_error("PoC mismatch");
        }
    }
    num_poc_recvd_++;

    /* Forward PoC to downstream fellows */
    if (from < BASE_PORT_DOWNSTREAM)
    {
        // std::cout << "Forwarding poc with job id " << current_job_id << std::endl
        //           << std::flush;

        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        for (uint64_t i = 0; i < static_cast<uint64_t>(num_nodes_); ++i)
        {
            if (i == my_id_)
                continue;
            dest_addr.sin_port = htons(BASE_PORT_DOWNSTREAM + i);
            auto sent = sendto(socket_fd_,
                               &PoC_combined_,
                               poc_msg_size,
                               0,
                               (struct sockaddr *)&dest_addr,
                               sizeof(dest_addr));
            if (static_cast<uint64_t>(sent) != poc_msg_size)
            {
                throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(poc_msg_size));
            }
        }
    }
    // std::cout << "Processed one PoC successfully\n"
    //           << std::flush;

    if (num_poc_recvd_ == (FAULTY_NODES + 1) * (FAULTY_NODES + 1))
    {
        num_poc_recvd_ = 0;
        profiler.mark_end();
        return true;
    }
    else
    {
        profiler.mark_end();
        return false;
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
        else if (msg_type == MessageType::POC)
        {
            bool finished = process_poc(msg, ntohs(client_addr.sin_port));
            if (finished)
            {
                auto duration = profiler.reset_and_output();
                std::cerr << duration << std::endl;
            }
        }
        else
        {
            throw std::runtime_error("Unexpected message type " + std::to_string((int)msg_type));
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

    DownstreamNode node(port, NodeType::DOWNSTREAM, server_id, num_nodes);
    node.run();
    return 0;
}