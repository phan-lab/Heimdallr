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
    memset(history_hash_, 0, crypto_generichash_BYTES);
    assert(num_nodes_ == 3 * FAULTY_NODES + 1);
}

void UpstreamNode::primary_send_input()
{
    // std::cout << "Sending inputs...\n"
    //           << std::flush;
    profiler.mark_start();
    assert(my_id_ == 0); /* the primary */
    Message input_msg(MessageType::PRIMARY_SEND_INPUT);
    SendInputMsgHelper input_msg_helper(input_msg.content);
    *input_msg_helper.current_job_id = ++current_job_id_;
    randombytes(input_msg_helper.input, input_len);

    /* sign the input */
    unsigned long long siglen;
    crypto_sign_detached(input_msg_helper.input_tag,
                         &siglen,
                         input_msg_helper.input,
                         input_len,
                         my_sec_key_.data());

    /* hash the signed input message */
    crypto_generichash(input_msg_helper.digest_of_signed_input, crypto_generichash_BYTES,
                       input_msg_helper.input_tag, crypto_sign_BYTES + input_len,
                       NULL, 0);
    /* hash the history || digest */
    crypto_generichash_state state;
    crypto_generichash_init(&state, NULL, 0, crypto_generichash_BYTES);
    crypto_generichash_update(&state, history_hash_, crypto_generichash_BYTES);
    crypto_generichash_update(&state, input_msg_helper.digest_of_signed_input, crypto_generichash_BYTES);
    crypto_generichash_final(&state, input_msg_helper.history_hash, crypto_generichash_BYTES);
    memcpy(history_hash_, input_msg_helper.history_hash, crypto_generichash_BYTES);
    /* sign the meta tag */
    crypto_sign_detached(input_msg_helper.meta_tag,
                         &siglen,
                         reinterpret_cast<uint8_t *>(input_msg_helper.current_job_id),
                         sizeof(*input_msg_helper.current_job_id) + crypto_generichash_BYTES * 2,
                         my_sec_key_.data());
    size_t message_size = sizeof(MessageType) + sizeof(*input_msg_helper.current_job_id) + crypto_sign_BYTES + crypto_generichash_BYTES * 2 + crypto_sign_BYTES + input_len;

    /* send to backups */
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
                           message_size,
                           0,
                           (struct sockaddr *)&dest_addr,
                           sizeof(dest_addr));
        if (static_cast<uint64_t>(sent) != message_size)
        {
            throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(message_size));
        }
    }
    num_poc_received_ = 0;
    // std::cout << "Input sent to backups\n"
    //           << std::flush;
    exec_task(input_msg);
    profiler.mark_end();
    auto duration = profiler.reset_and_output();
    std::cerr << duration << std::endl
              << std::flush;
}

void UpstreamNode::recv_input()
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

    int from_id = ntohs(client_addr.sin_port) - BASE_PORT_UPSTREAM;
    if (msg.type == MessageType::PRIMARY_SEND_INPUT)
    {
        assert(my_id_ != 0);

        SendInputMsgHelper input_msg_helper(msg.content);

        /* verify the input tag */
        if (crypto_sign_verify_detached(input_msg_helper.input_tag,
                                        input_msg_helper.input,
                                        input_len,
                                        upstream_pub_keys_[from_id].data()) != 0)
        {
            throw std::runtime_error("Invalid input tag");
        }

        /* compute the digest of input info */
        uint8_t digest_of_input_for_verification[crypto_generichash_BYTES];
        crypto_generichash(digest_of_input_for_verification, crypto_generichash_BYTES,
                           input_msg_helper.input_tag, crypto_sign_BYTES + input_len,
                           NULL, 0);
        /* verify the digest */
        if (memcmp(digest_of_input_for_verification, input_msg_helper.digest_of_signed_input, crypto_generichash_BYTES))
        {
            throw std::runtime_error("Digest mismatch");
        }
        /* Compute the new history hash for verification */
        uint8_t history_hash_for_verification[crypto_generichash_BYTES];
        crypto_generichash_state state;
        crypto_generichash_init(&state, NULL, 0, crypto_generichash_BYTES);
        crypto_generichash_update(&state, history_hash_, crypto_generichash_BYTES);
        crypto_generichash_update(&state, input_msg_helper.digest_of_signed_input, crypto_generichash_BYTES);
        crypto_generichash_final(&state, history_hash_for_verification, crypto_generichash_BYTES);
        /* verify the history hash */
        if (memcmp(history_hash_for_verification, input_msg_helper.history_hash, crypto_generichash_BYTES))
        {
            throw std::runtime_error("History hash mismatch");
        }
        memcpy(history_hash_, input_msg_helper.history_hash, crypto_generichash_BYTES);

        /* Verify the job id */
        if (*input_msg_helper.current_job_id != current_job_id_ + 1)
        {
            throw std::runtime_error("Invalid job id");
        }
        /* Verify the meta tag */
        if (crypto_sign_verify_detached(input_msg_helper.meta_tag,
                                        reinterpret_cast<uint8_t *>(input_msg_helper.current_job_id),
                                        sizeof(*input_msg_helper.current_job_id) + crypto_generichash_BYTES * 2,
                                        upstream_pub_keys_[from_id].data()) != 0)
        {
            throw std::runtime_error("Invalid meta tag");
        }
        /* update the job id*/
        current_job_id_ = *input_msg_helper.current_job_id;

        /* execute the task */
        exec_task(msg);
        profiler.mark_end();
        auto duration = profiler.reset_and_output();
        std::cerr << duration << std::endl
                  << std::flush;
        return;
    }
    else
    {
        throw std::runtime_error("Unexpected type " + std::to_string((int)msg_type));
        return;
    }
}

void UpstreamNode::exec_task(Message &input_msg)
{
    Message result_msg(MessageType::RESULT_TO_DOWNSTREAM);
    ResultMsgHelper result_msg_helper(result_msg.content);

    SendInputMsgHelper input_msg_helper(input_msg.content);

    /** @note Assume result equal to input. Add an artificial delay later */
    memcpy(result_msg_helper.results, input_msg_helper.input, input_len);
    *result_msg_helper.job_id = current_job_id_;
    std::this_thread::sleep_for(std::chrono::microseconds(exec_time_us));

    /* hash the result */
    crypto_generichash(result_msg_helper.result_hash, crypto_generichash_BYTES,
                       result_msg_helper.results, input_len,
                       NULL, 0);

    /* copy the new history hash */
    memcpy(result_msg_helper.history_hash, history_hash_, crypto_generichash_BYTES);

    /* assign job id */
    *result_msg_helper.job_id = current_job_id_;

    /* copy input message */
    size_t input_message_size = crypto_sign_BYTES + input_len;
    memcpy(result_msg_helper.send_input_msg_ptr, input_msg_helper.input, input_message_size);

    /* Compute the meta-tag */
    unsigned long long siglen;
    crypto_sign_detached(result_msg_helper.meta_tag,
                         &siglen,
                         reinterpret_cast<uint8_t *>(result_msg_helper.job_id),
                         sizeof(*result_msg_helper.job_id) + crypto_generichash_BYTES + crypto_generichash_BYTES,
                         my_sec_key_.data());

    size_t result_msg_size = sizeof(MessageType) + sizeof(*result_msg_helper.job_id) + crypto_sign_BYTES + crypto_generichash_BYTES + crypto_generichash_BYTES + input_message_size + input_len;

    /* send the result message to downstream node */
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    dest_addr.sin_port = htons(BASE_PORT_DOWNSTREAM);
    auto sent = sendto(socket_fd_,
                       &result_msg,
                       result_msg_size,
                       0,
                       (struct sockaddr *)&dest_addr,
                       sizeof(dest_addr));
    if (static_cast<uint64_t>(sent) != result_msg_size)
    {
        throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(result_msg_size));
    }
}

void UpstreamNode::run()
{
    if (my_id_ == 0)
    {
        while (1)
        {
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto ms_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(duration) -
                std::chrono::duration_cast<std::chrono::seconds>(duration);
            auto ms = ms_duration.count();
            if (ms % 100 < SEND_INPUT_TIME)
            {
                break;
            }
        }
        /**
         * Send preprepare once 100 ms, right after the milliseconds % 100
         * passes SEND_PREPREPARE_TIME.
         * In the meantime, receive messages.
         */
        int64_t last_sent = -1;
        while (1)
        {
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto ms_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(duration) -
                std::chrono::duration_cast<std::chrono::seconds>(duration);
            auto ms = ms_duration.count();
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
            if (ms % 100 >= SEND_INPUT_TIME && seconds * 10 + ms / 100 > last_sent)
            {
                primary_send_input();
                last_sent = seconds * 10 + ms / 100;
            }
            recv_input();
        }
    }
    else
    {
        while (1)
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