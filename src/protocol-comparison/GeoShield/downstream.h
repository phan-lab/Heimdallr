#ifndef _DOWNSTREAM_H_
#define _DOWNSTREAM_H_

#include "node.h"

#include <set>

class DownstreamNode : public Node
{
private:
    void process_result(Message &result_msg, int from_id);
    bool process_poc(Message &poc_msg, int port);

    const static unsigned long long result_len = 1024;
    uint8_t result_buf_[result_len];

    int num_poc_recvd_;

public:
    DownstreamNode(int port, NodeType node_type, uint64_t id, int num_nodes);
    void run();
};

#endif