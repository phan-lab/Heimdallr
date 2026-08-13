#include "downstream.h"

#include <string.h>
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
    : Node(port, node_type, id, 3 * FAULTY_NODES + 1), num_results_recvd_(0),
      num_local_commits_recvd_(0),
      commit_msg_(MessageType::COMMIT), commit_msg_helper_(commit_msg_.content)
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

        /* reset the commit message */
        *commit_msg_helper_.job_id = current_job_id_;
        memcpy(commit_msg_helper_.history_hash, history_hash_, crypto_generichash_BYTES);
        memcpy(commit_msg_helper_.reply_hash, result_hash_, crypto_generichash_BYTES);
        memset(commit_msg_helper_.matching_replica_ids, 0, (2 * FAULTY_NODES + 1) * sizeof(uint64_t));
        memset(commit_msg_helper_.matching_tags, 0, (2 * FAULTY_NODES + 1) * crypto_sign_BYTES);

        commit_msg_helper_.matching_replica_ids[0] = from_id;
        memcpy(&commit_msg_helper_.matching_tags[0], result_msg_helper.meta_tag, crypto_sign_BYTES);
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
        commit_msg_helper_.matching_replica_ids[num_results_recvd_] = from_id;
        memcpy(commit_msg_helper_.matching_tags + crypto_sign_BYTES * num_results_recvd_, result_msg_helper.meta_tag, crypto_sign_BYTES);
    }
    num_results_recvd_++;
    profiler.mark_end();

    if (num_results_recvd_ == 2 * FAULTY_NODES + 1)
    {
        /* sign the commit message */
        unsigned long long siglen;
        crypto_sign_detached(commit_msg_helper_.tag, &siglen,
                             reinterpret_cast<uint8_t *>(commit_msg_helper_.job_id),
                             CommitMsgHelper::size - crypto_sign_BYTES,
                             my_sec_key_.data());
        /* send to all upstream nodes */
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        for (int i = 0; i < num_nodes_; ++i)
        {
            dest_addr.sin_port = htons(BASE_PORT_UPSTREAM + i);
            auto sent = sendto(socket_fd_,
                               &commit_msg_,
                               CommitMsgHelper::size + sizeof(MessageType),
                               0,
                               (struct sockaddr *)&dest_addr,
                               sizeof(dest_addr));
            if (static_cast<uint64_t>(sent) != CommitMsgHelper::size + sizeof(MessageType))
            {
                throw std::runtime_error("Error! " + std::to_string(sent) + " bytes sent instead of " + std::to_string(CommitMsgHelper::size + sizeof(MessageType)));
            }
        }
        num_local_commits_recvd_ = 0;
    }

    if (num_results_recvd_ == 3 * FAULTY_NODES + 1)
    {
        num_results_recvd_ = 0;
        current_job_id_++;
        auto duration = profiler.reset_and_output();
        std::cerr << duration << std::endl
                  << std::flush;
    }
}

void DownstreamNode::process_local_commit(Message &local_commit_msg, int from_id)
{
    if (num_local_commits_recvd_ >= 2 * FAULTY_NODES + 1)
        return;
    profiler.mark_start();
    LocalCommitMsgHelper local_commit_msg_helper(local_commit_msg.content);
    /* verify the tag */
    if (crypto_sign_verify_detached(local_commit_msg_helper.tag,
                                    reinterpret_cast<uint8_t *>(local_commit_msg_helper.job_id),
                                    LocalCommitMsgHelper::size - crypto_sign_BYTES,
                                    upstream_pub_keys_[from_id].data()) != 0)
    {
        throw std::runtime_error("Signature verification failed");
    }

    /* verify result hash and history hash */
    if (memcmp(history_hash_, local_commit_msg_helper.history_hash, crypto_generichash_BYTES) != 0)
    {
        throw std::runtime_error("History hash mismatch");
    }
    if (memcmp(result_hash_, local_commit_msg_helper.reply_hash, crypto_generichash_BYTES) != 0)
    {
        throw std::runtime_error("Result hash mismatch");
    }

    profiler.mark_end();
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
        else if (msg_type == MessageType::LOCAL_COMMIT)
        {
            process_local_commit(msg, from_id);
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