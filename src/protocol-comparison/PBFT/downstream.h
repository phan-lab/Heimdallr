#ifndef _DOWNSTREAM_H_
#define _DOWNSTREAM_H_

#include "node.h"


class DownstreamNode : public Node
{
private:
    void process_result(Message &result_msg);

    const static unsigned long long result_len = 1024;
    uint8_t result_buf_[result_len];
    int num_reply_recvd_;

public:
    DownstreamNode(int port, NodeType node_type, uint64_t id);
    void run();
};

#endif