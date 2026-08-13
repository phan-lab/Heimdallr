#ifndef _DOWNSTREAM_H_
#define _DOWNSTREAM_H_

#include "node.h"

#include <set>

class DownstreamNode : public Node
{
private:
    /* pairs of (upstream, downstream) nodes from which the hbs are received */
    uint64_t most_recent_round;
    bool hb_available_pairs_[FAULTY_NODES + 1][FAULTY_NODES + 1];
    uint8_t received_tags_[FAULTY_NODES + 1][crypto_sign_BYTES];
    std::pair<uint64_t, uint64_t> smallest_proposal_; /* value, proposer */
    Message evid_smallest_proposal;

    void verify_upstream_tags(uint64_t round, uint8_t* tag_buf);

    void receive_hb_and_proposal(uint64_t round);
    void send_accept(uint64_t round);
    void recv_accept(uint64_t round);

public:
    DownstreamNode(int port, NodeType node_type, uint64_t id, int num_nodes);
    void run();
};

#endif