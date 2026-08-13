#ifndef _UPSTREAM_H_
#define _UPSTREAM_H_

#include "node.h"

#include <unordered_set>

class UpstreamNode : public Node
{
private:
    int num_poc_received_;

    const static int exec_time_us;

    uint8_t history_hash_[crypto_generichash_BYTES];

public:
    UpstreamNode(int port, NodeType node_type, uint64_t id, int num_nodes);
    void run();

    void primary_send_input();
    void recv_input(); /* return true iff all poc received this round */
    void exec_task(Message &input_msg);
};

#endif