#ifndef _DOWNSTREAM_H_
#define _DOWNSTREAM_H_

#include "node.h"

class DownstreamNode : public Node
{
private:
    void process_result(Message &result_msg, int from_id);
    void process_local_commit(Message &local_commit_msg, int from_id);

    const static unsigned long long result_len = 1024;
    uint8_t result_buf_[result_len];
    uint8_t result_hash_[crypto_generichash_BYTES];
    uint8_t history_hash_[crypto_generichash_BYTES];

    int num_results_recvd_;
    int num_local_commits_recvd_;

    Message commit_msg_;
    CommitMsgHelper commit_msg_helper_;

public:
    DownstreamNode(int port, NodeType node_type, uint64_t id);
    void run();
};

#endif