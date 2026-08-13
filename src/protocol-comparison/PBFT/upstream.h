#ifndef _UPSTREAM_H_
#define _UPSTREAM_H_

#include "node.h"

#include <unordered_set>

class UpstreamNode : public Node
{
private:
    const static int exec_time_us;

    uint8_t num_prepare_received_;
    uint8_t num_commit_received_;

    uint8_t digest_of_input[crypto_generichash_BYTES];
    uint8_t input[input_len];

    void primary_send_preprepare();
    void process_preprepare(Message &preprepare_msg);
    void process_prepare(Message &prepare_msg);
    void process_commit(Message &commit_msg);
    void recv_msg();
    /* Nonzero means datagrams were lost or reordered, i.e. the host was more
     * oversubscribed than the paper's one-process-per-core setup. */
    uint64_t num_desyncs_ = 0;
    uint64_t num_stale_msgs_ = 0;
    void exec_task();

public:
    UpstreamNode(int port, NodeType node_type, uint64_t id, int num_nodes);
    void run();
};

#endif