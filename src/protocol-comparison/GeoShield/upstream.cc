#include "upstream.h"
#include <iomanip>
#include <iostream>
#include <cstring>
#include <thread>
#include <arpa/inet.h>


const int UpstreamNode::exec_time_us = 0;

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

UpstreamNode::UpstreamNode(
    int port, NodeType node_type, uint64_t id, int num_nodes)
    : Node(port, node_type, id, num_nodes),
      num_poc_received_(0)
{
}

void UpstreamNode::primary_send_input()
{
    // std::cout << "Sending inputs...\n"
    //           << std::flush;
    profiler.mark_start();
    assert(my_id_ == 0); /* the primary */
    Message input_msg(MessageType::PRIMARY_SEND_INPUT);
    SendInputMsgHelper input_msg_helper(input_msg.content);
    *input_msg_helper.job_id = ++current_job_id;
    randombytes(input_msg_helper.input, input_len);

    unsigned long long siglen;
    crypto_sign_detached(input_msg_helper.sig,
                         &siglen,
                         reinterpret_cast<uint8_t *>(input_msg_helper.job_id),
                         sizeof(*input_msg_helper.job_id) + input_len,
                         my_sec_key_.data());
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    for (int i = 0; i < num_nodes_; ++i)
    {
        if (i == static_cast<int>(my_id_))
            continue;
        dest_addr.sin_port = htons(BASE_PORT_UPSTREAM + i);
        auto sent = sendto(socket_fd_,
                           &input_msg,
                           sizeof(MessageType) + sizeof(*SendInputMsgHelper::job_id) + input_len + crypto_sign_BYTES,
                           0,
                           (struct sockaddr *)&dest_addr,
                           sizeof(dest_addr));
        if (static_cast<uint64_t>(sent) != sizeof(MessageType) + sizeof(*SendInputMsgHelper::job_id) + input_len + crypto_sign_BYTES)
        {
            throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(sizeof(MessageType) + sizeof(*SendInputMsgHelper::job_id) + input_len + crypto_sign_BYTES));
        }
    }
    num_poc_received_ = 0;
    // std::cout << "Input sent to backups\n"
    //           << std::flush;
    exec_task(input_msg_helper.input, current_job_id);
    profiler.mark_end();
}

bool UpstreamNode::recv_input()
{
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    Message msg;
    int bytes_recvd = recvfrom(socket_fd_, &msg, MAXBUF, 0,
                               (struct sockaddr *)&client_addr, &len);
    if (bytes_recvd <= 0)
        return false;

    profiler.mark_start();

    MessageType msg_type = msg.type;

    int from_id = ntohs(client_addr.sin_port) - BASE_PORT_UPSTREAM;
    if (msg.type == MessageType::PRIMARY_SEND_INPUT)
    {
        assert(my_id_ != 0);
        // std::cout << "Got an input message\n"
        //           << std::flush;
        SendInputMsgHelper input_msg_helper(msg.content);
        if (crypto_sign_verify_detached(input_msg_helper.sig,
                                        reinterpret_cast<uint8_t *>(input_msg_helper.job_id),
                                        sizeof(*input_msg_helper.job_id) + input_len,
                                        upstream_pub_keys_.at(from_id).data()))
        {
            throw std::runtime_error("Invalid input signature from upstream " + std::to_string(from_id));
        }

        exec_task(input_msg_helper.input, *input_msg_helper.job_id);
        profiler.mark_end();
        return false;
    }
    else if (msg.type == MessageType::POC)
    {
        // std::cout << "Got a PoC message\n"
        //           << std::flush;
        auto ret = process_poc_msg(msg, from_id);
        profiler.mark_end();
        if (ret)
        {
            auto duration = profiler.reset_and_output();
            std::cerr << duration << std::endl
                      << std::flush;
        }
        return ret;
    }
    else
    {
        throw std::runtime_error("Unexpected type " + std::to_string((int)msg_type));
        return false;
    }
}

bool UpstreamNode::process_poc_msg(Message &poc_msg, int from_id)
{
    bool seen_before;
    PoCMsgHelper poc_msg_helper(poc_msg.content);
    if (current_job_id < *poc_msg_helper.job_id)
    {
        current_job_id = *poc_msg_helper.job_id;
        num_poc_received_ = 0;
        seen_before = false;
    }
    else
        seen_before = true;
    num_poc_received_++;

    /* make sure the hash matches */
    if (seen_before)
    {
        if (memcmp(poc_msg_helper.result_hash, PoC_combined_helper_.result_hash, crypto_generichash_BYTES))
        {
            std::cerr << "job id " << PoC_combined_helper_.job_id << std::endl;
            throw std::runtime_error("PoC hash mismatch");
        }
    }
    else
    {
        memcpy(PoC_combined_helper_.result_hash, poc_msg_helper.result_hash, crypto_generichash_BYTES);
    }

    /* verify the sig in poc */
    if (crypto_sign_verify_detached(
            poc_msg_helper.sigs[0].sig,
            poc_msg_helper.result_hash,
            crypto_generichash_BYTES,
            upstream_pub_keys_.at(from_id).data()))
    {
        throw std::runtime_error("Invalid signature in PoC from upstream node " + std::to_string(from_id));
    }

    memcpy(PoC_combined_helper_.sigs[from_id].sig, poc_msg_helper.sigs[0].sig,
           crypto_sign_BYTES);

    if (num_poc_received_ == num_nodes_)
    {
        /* Send the combined PoC to downstream nodes */
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        for (int i = 0; i < num_nodes_; ++i)
        {
            dest_addr.sin_port = htons(BASE_PORT_DOWNSTREAM + i);
            auto sent = sendto(socket_fd_,
                               &PoC_combined_,
                               sizeof(MessageType) + sizeof(current_job_id) + crypto_generichash_BYTES + (num_nodes_ + 1) * sizeof(PoCMsgHelper::sig_t),
                               0,
                               (struct sockaddr *)&dest_addr,
                               sizeof(dest_addr));
            if (static_cast<uint64_t>(sent) != sizeof(MessageType) + sizeof(current_job_id) + crypto_generichash_BYTES + (num_nodes_ + 1) * sizeof(PoCMsgHelper::sig_t))
            {
                throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(sizeof(MessageType) + sizeof(current_job_id) + crypto_generichash_BYTES + (num_nodes_ + 1) * sizeof(PoCMsgHelper::sig_t)));
            }
        }
        return true;
    }
    else
    {
        return false;
    }
}

void UpstreamNode::exec_task(uint8_t *input, uint64_t job_id)
{
    bool seen_before;
    if (current_job_id < job_id && my_id_ != 0)
    {
        current_job_id = job_id;
        num_poc_received_ = 0;
        seen_before = false;
    }
    else
        seen_before = true;

    Message result_msg(MessageType::RESULT_TO_DOWNSTREAM);
    ResultMsgHelper result_msg_helper(result_msg.content);

    /** @note Assume result equal to input. Add an artificial delay later */
    memcpy(result_msg_helper.results, input, input_len);
    *result_msg_helper.job_id = current_job_id;
    std::this_thread::sleep_for(std::chrono::microseconds(exec_time_us));

    crypto_generichash(job_id_result_hash.data(), crypto_generichash_BYTES,
                       reinterpret_cast<uint8_t *>(result_msg_helper.job_id),
                       sizeof(current_job_id) + input_len,
                       NULL, 0);

    /* sign the result message */
    unsigned long long siglen;
    crypto_sign_detached(result_msg_helper.sig,
                         &siglen,
                         job_id_result_hash.data(),
                         job_id_result_hash.size(),
                         my_sec_key_.data());

    /* if courier, send to downstream couriers */
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (my_id_ < num_couriers)
    {
        for (uint64_t i = 0; i < num_couriers; ++i)
        {
            dest_addr.sin_port = htons(BASE_PORT_DOWNSTREAM + i);
            auto sent = sendto(socket_fd_,
                               &result_msg,
                               sizeof(MessageType) + sizeof(current_job_id) + crypto_sign_BYTES + input_len,
                               0,
                               (struct sockaddr *)&dest_addr,
                               sizeof(dest_addr));
            if (static_cast<uint64_t>(sent) != sizeof(MessageType) + sizeof(current_job_id) + crypto_sign_BYTES + input_len)
            {
                throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(sizeof(MessageType) + sizeof(current_job_id) + crypto_sign_BYTES + input_len));
            }
        }
    }

    /* generate and send PoCs */
    // std::cout << "Preparing to send PoCs\n"
    //   << std::flush;
    Message poc_msg(MessageType::POC);
    PoCMsgHelper poc_msg_helper(poc_msg.content);

    *poc_msg_helper.job_id = current_job_id;
    memcpy(poc_msg_helper.result_hash, job_id_result_hash.data(), crypto_generichash_BYTES);
    memcpy(poc_msg_helper.sigs[0].sig, result_msg_helper.sig, crypto_sign_BYTES);

    /* Store the combined PoC */
    if (!seen_before || my_id_ == 0)
    {
        *PoC_combined_helper_.job_id = current_job_id;
        memcpy(PoC_combined_helper_.result_hash, job_id_result_hash.data(), crypto_generichash_BYTES);
    }
    else
    {
        if (*PoC_combined_helper_.job_id != current_job_id)
        {
            std::cerr << *PoC_combined_helper_.job_id << " " << current_job_id << std::endl;
            throw std::runtime_error("Unexpected job id from input");
        }
        if (memcmp(job_id_result_hash.data(), PoC_combined_helper_.result_hash, crypto_generichash_BYTES))
        {
            throw std::runtime_error("Hash mismatch with prev PoC");
        }
    }
    memcpy(PoC_combined_helper_.sigs[my_id_].sig, result_msg_helper.sig, crypto_sign_BYTES);

    num_poc_received_++;

    /* Send PoC to upstream measurers */
    for (int i = 0; i < num_nodes_; ++i)
    {
        if (static_cast<uint64_t>(i) == my_id_)
            continue;
        dest_addr.sin_port = htons(BASE_PORT_UPSTREAM + i);
        auto sent = sendto(socket_fd_,
                           &poc_msg,
                           sizeof(MessageType) + sizeof(current_job_id) + crypto_generichash_BYTES + sizeof(PoCMsgHelper::sig_t),
                           0,
                           (struct sockaddr *)&dest_addr,
                           sizeof(dest_addr));
        if (static_cast<uint64_t>(sent) != sizeof(MessageType) + sizeof(current_job_id) + crypto_generichash_BYTES + sizeof(PoCMsgHelper::sig_t))
        {
            throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(sizeof(MessageType) + sizeof(current_job_id) + crypto_generichash_BYTES + sizeof(PoCMsgHelper::sig_t)));
        }
        // std::cout << "PoC sent to " << i << std::endl
        //           << std::flush;
    }
}

void UpstreamNode::run()
{
    if (my_id_ == 0)
        std::this_thread::sleep_for(std::chrono::microseconds(1000000));
    while (1)
    {
        if (my_id_ == 0)
        {
            primary_send_input();
            while (!recv_input())
                ;
            std::this_thread::sleep_for(std::chrono::microseconds(100000));
        }
        else
        {
            recv_input();
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