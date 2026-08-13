#ifndef _UPSTREAM_H_
#define _UPSTREAM_H_

#include "node.h"

#include <unordered_set>

class UpstreamNode : public Node
{
private:
    /* indicating whose tags have arrived */
    std::unordered_set<uint64_t> tag_senders_;
    std::vector<std::array<uint8_t, crypto_sign_BYTES>> received_tags_;

    Message prepared_hb_;

    void run_tag_exchange(uint64_t round);
    void recv_tags(uint64_t round);
    void run_send_hb();

public:
    UpstreamNode(int port, NodeType node_type, uint64_t id, int num_nodes);
    void run();
};

#endif