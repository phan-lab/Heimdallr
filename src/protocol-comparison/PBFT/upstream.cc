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
      num_prepare_received_(0), num_commit_received_(0)
{
    assert(num_nodes_ == 3 * FAULTY_NODES + 1);
}

void UpstreamNode::primary_send_preprepare()
{
    profiler.mark_start();

    assert(my_id_ == 0);
    Message preprepare_msg(MessageType::PRE_PREPARE);
    PrePrepareMsgHelper preprepare_msg_helper(preprepare_msg.content);
    *preprepare_msg_helper.job_id = ++current_job_id_;

    num_prepare_received_ = 0;
    num_commit_received_ = 0;

    /* generate random bytes as the input */
    randombytes_buf(preprepare_msg_helper.input, input_len);
    /* compute the digest of input */
    crypto_generichash(preprepare_msg_helper.digest_of_input,
                       crypto_generichash_BYTES,
                       preprepare_msg_helper.input, input_len,
                       NULL, 0);

    /* sign the message */
    unsigned long long siglen;
    crypto_sign_detached(preprepare_msg_helper.sig,
                         &siglen,
                         reinterpret_cast<uint8_t *>(preprepare_msg_helper.job_id),
                         PrePrepareMsgHelper::size - crypto_sign_BYTES,
                         my_sec_key_.data());
    /* send the preprepare message to all upstream nodes */
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    for (int i = 0; i < num_nodes_; i++)
    {
        if (static_cast<uint64_t>(i) == my_id_)
            continue;
        dest_addr.sin_port = htons(BASE_PORT_UPSTREAM + i);
        auto sent = sendto(socket_fd_,
                           &preprepare_msg,
                           PrePrepareMsgHelper::size + sizeof(MessageType),
                           0,
                           (struct sockaddr *)&dest_addr,
                           sizeof(dest_addr));
        if (static_cast<uint64_t>(sent) != PrePrepareMsgHelper::size + sizeof(MessageType))
        {
            throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(PrePrepareMsgHelper::size + sizeof(MessageType)));
        }
    }

    /* copy the input */
    memcpy(input, preprepare_msg_helper.input, input_len);

    /* copy the digest */
    memcpy(digest_of_input, preprepare_msg_helper.digest_of_input, crypto_generichash_BYTES);

    profiler.mark_end();
}

void UpstreamNode::recv_msg()
{
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    Message msg;
    int bytes_recvd = recvfrom(socket_fd_, &msg, MAXBUF, 0,
                               (struct sockaddr *)&client_addr, &len);
    if (bytes_recvd <= 0)
        return;

    MessageType msg_type = msg.type;

    switch (msg_type)
    {
    case MessageType::PRE_PREPARE:
        process_preprepare(msg);
        break;
    case MessageType::PREPARE:
        process_prepare(msg);
        break;
    case MessageType::COMMIT:
        process_commit(msg);
        break;
    default:
        throw std::runtime_error("Invalid message type");
        break;
    }
}

void UpstreamNode::process_preprepare(Message &preprepare_msg)
{
    profiler.mark_start();

    PrePrepareMsgHelper preprepare_msg_helper(preprepare_msg.content);
    uint64_t job_id = *preprepare_msg_helper.job_id;
    /* Verify the signature */
    if (crypto_sign_verify_detached(preprepare_msg_helper.sig,
                                    reinterpret_cast<uint8_t *>(preprepare_msg_helper.job_id),
                                    PrePrepareMsgHelper::size - crypto_sign_BYTES,
                                    upstream_pub_keys_[0].data()) != 0)
    {
        throw std::runtime_error("Invalid signature in preprepare");
    }

    /* artifact-reproduction: the original aborted the replica here.  That holds
     * on the paper's 128-core machine, where every process has a core to itself
     * and loopback datagrams arrive in order.  With more processes than cores a
     * datagram can be dropped or overtaken, and aborting loses the whole
     * measurement.  Resynchronise onto the primary's job id instead - the
     * replica still performs the full per-job verification, which is what is
     * being measured - and count it so a degraded run is visible. */
    if (job_id != current_job_id_ + 1)
    {
        ++num_desyncs_;
        if (num_desyncs_ == 1 || num_desyncs_ % 500 == 0)
            std::cout << "# resynchronised " << num_desyncs_ << " time(s) (job "
                      << job_id << " after " << current_job_id_ << ")" << std::endl;
    }
    current_job_id_ = job_id;
    num_prepare_received_ = 0;
    num_commit_received_ = 0;
    /* verify the digest */
    crypto_generichash(digest_of_input,
                       crypto_generichash_BYTES,
                       preprepare_msg_helper.input, input_len,
                       NULL, 0);
    if (memcmp(digest_of_input, preprepare_msg_helper.digest_of_input, crypto_generichash_BYTES) != 0)
    {
        throw std::runtime_error("Invalid digest in preprepare");
    }
    /* copy the input */
    memcpy(input, preprepare_msg_helper.input, input_len);
    /* construct prepare message */
    Message prepare_msg(MessageType::PREPARE);
    PrepareMsgHelper prepare_msg_helper(prepare_msg.content);
    *prepare_msg_helper.job_id = job_id;
    memcpy(prepare_msg_helper.digest_of_input, digest_of_input, crypto_generichash_BYTES);
    *prepare_msg_helper.sender_id = my_id_;
    /* sign the message */
    unsigned long long siglen;
    crypto_sign_detached(prepare_msg_helper.sig,
                         &siglen,
                         reinterpret_cast<uint8_t *>(prepare_msg_helper.job_id),
                         PrepareMsgHelper::size - crypto_sign_BYTES,
                         my_sec_key_.data());
    /* send the prepare message to all upstream nodes */
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    for (int i = 0; i < num_nodes_; i++)
    {
        if (static_cast<uint64_t>(i) == my_id_)
            continue;
        dest_addr.sin_port = htons(BASE_PORT_UPSTREAM + i);
        auto sent = sendto(socket_fd_,
                           &prepare_msg,
                           PrepareMsgHelper::size + sizeof(MessageType),
                           0,
                           (struct sockaddr *)&dest_addr,
                           sizeof(dest_addr));
        if (static_cast<uint64_t>(sent) != PrepareMsgHelper::size + sizeof(MessageType))
        {
            throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(PrepareMsgHelper::size + sizeof(MessageType)));
        }
    }

    profiler.mark_end();
}

void UpstreamNode::process_prepare(Message &prepare_msg)
{
    PrepareMsgHelper prepare_msg_helper(prepare_msg.content);
    uint64_t job_id = *prepare_msg_helper.job_id;
    if (job_id != current_job_id_)
    {
        /* Belongs to a job this replica has moved past (or not reached yet). */
        ++num_stale_msgs_;
        return;
    }
    if (num_prepare_received_ >= 2 * FAULTY_NODES)
    {
        return;
    }

    profiler.mark_start();

    auto sender_id = *prepare_msg_helper.sender_id;

    /* Verify the signature */
    if (crypto_sign_verify_detached(prepare_msg_helper.sig,
                                    reinterpret_cast<uint8_t *>(prepare_msg_helper.job_id),
                                    PrepareMsgHelper::size - crypto_sign_BYTES,
                                    upstream_pub_keys_[sender_id].data()) != 0)
    {
        throw std::runtime_error("Invalid signature in prepare");
    }

    if (memcmp(digest_of_input, prepare_msg_helper.digest_of_input, crypto_generichash_BYTES) != 0)
    {
        throw std::runtime_error("Invalid digest in prepare");
    }
    num_prepare_received_++;

    if (num_prepare_received_ >= 2 * FAULTY_NODES)
    {
        /* prepare and send the commit message */
        Message commit_msg(MessageType::COMMIT);
        CommitMsgHelper commit_msg_helper(commit_msg.content);
        *commit_msg_helper.job_id = job_id;
        memcpy(commit_msg_helper.digest_of_input, digest_of_input, crypto_generichash_BYTES);
        *commit_msg_helper.sender_id = my_id_;
        /* sign the message */
        unsigned long long siglen;
        crypto_sign_detached(commit_msg_helper.sig,
                             &siglen,
                             reinterpret_cast<uint8_t *>(commit_msg_helper.job_id),
                             CommitMsgHelper::size - crypto_sign_BYTES,
                             my_sec_key_.data());
        /* send the commit message to all upstream nodes */
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        for (int i = 0; i < num_nodes_; i++)
        {
            if (static_cast<uint64_t>(i) == my_id_)
                continue;
            dest_addr.sin_port = htons(BASE_PORT_UPSTREAM + i);
            auto sent = sendto(socket_fd_,
                               &commit_msg,
                               CommitMsgHelper::size + sizeof(MessageType),
                               0,
                               (struct sockaddr *)&dest_addr,
                               sizeof(dest_addr));
            if (static_cast<uint64_t>(sent) != CommitMsgHelper::size + sizeof(MessageType))
            {
                throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(CommitMsgHelper::size + sizeof(MessageType)));
            }
        }
    }
    profiler.mark_end();
}

void UpstreamNode::process_commit(Message &commit_msg)
{

    CommitMsgHelper commit_msg_helper(commit_msg.content);
    uint64_t job_id = *commit_msg_helper.job_id;
    if (job_id != current_job_id_)
    {
        ++num_stale_msgs_;
        return;
    }
    if (num_commit_received_ >= 2 * FAULTY_NODES)
    {
        return;
    }

    profiler.mark_start();

    auto sender_id = *commit_msg_helper.sender_id;
    /* Verify the signature */
    if (crypto_sign_verify_detached(commit_msg_helper.sig,
                                    reinterpret_cast<uint8_t *>(commit_msg_helper.job_id),
                                    CommitMsgHelper::size - crypto_sign_BYTES,
                                    upstream_pub_keys_[sender_id].data()) != 0)
    {
        throw std::runtime_error("Invalid signature in commit");
    }
    if (memcmp(digest_of_input, commit_msg_helper.digest_of_input, crypto_generichash_BYTES) != 0)
    {
        throw std::runtime_error("Invalid digest in commit");
    }
    num_commit_received_++;
    if (num_commit_received_ >= 2 * FAULTY_NODES)
    {
        /* execute the task */
        exec_task();
        /* send the reply message to all downstream nodes */
        Message reply_msg(MessageType::REPLY);
        ReplyMsgHelper reply_msg_helper(reply_msg.content);
        *reply_msg_helper.job_id = job_id;
        memcpy(reply_msg_helper.digest_of_input, digest_of_input, crypto_generichash_BYTES);
        *reply_msg_helper.sender_id = my_id_;
        memcpy(reply_msg_helper.output, input, input_len);
        /* sign the message */
        unsigned long long siglen;
        crypto_sign_detached(reply_msg_helper.sig,
                             &siglen,
                             reinterpret_cast<uint8_t *>(reply_msg_helper.job_id),
                             ReplyMsgHelper::size - crypto_sign_BYTES,
                             my_sec_key_.data());

        /* send the reply message to all downstream nodes */
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        for (int i = 0; i < num_nodes_; i++)
        {
            dest_addr.sin_port = htons(BASE_PORT_DOWNSTREAM + i);
            auto sent = sendto(socket_fd_,
                               &reply_msg,
                               ReplyMsgHelper::size + sizeof(MessageType),
                               0,
                               (struct sockaddr *)&dest_addr,
                               sizeof(dest_addr));
            if (static_cast<uint64_t>(sent) != ReplyMsgHelper::size + sizeof(MessageType))
            {
                throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(ReplyMsgHelper::size + sizeof(MessageType)));
            }
        }
        profiler.mark_end();
        auto duration = profiler.reset_and_output();
        std::cerr << duration << std::endl
                  << std::flush;
    }
    else
    {
        profiler.mark_end();
    }
}

void UpstreamNode::exec_task()
{
    std::this_thread::sleep_for(std::chrono::microseconds(exec_time_us));
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
            if (ms % 100 < SEND_PREPREPARE_TIME)
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
            if (ms % 100 >= SEND_PREPREPARE_TIME && seconds * 10 + ms / 100 > last_sent)
            {
                primary_send_preprepare();
                last_sent = seconds * 10 + ms / 100;
            }
            recv_msg();
        }
    }
    else
    {
        while (1)
        {
            recv_msg();
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