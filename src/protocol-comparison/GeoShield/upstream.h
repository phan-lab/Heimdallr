#ifndef _UPSTREAM_H_
#define _UPSTREAM_H_

#include "node.h"

#include <unordered_set>

class UpstreamNode : public Node
{
private:
    int num_poc_received_;


    const static unsigned long long input_len = 1024;
    const static int exec_time_us; 

public:
    UpstreamNode(int port, NodeType node_type, uint64_t id, int num_nodes);
    void run();

    void primary_send_input();
    bool recv_input(); /* return true iff all poc received this round */
    void exec_task(uint8_t *input, uint64_t job_id);
    bool process_poc_msg(Message& poc_msg, int from_id); /* return true iff all poc received this round */
};

#endif