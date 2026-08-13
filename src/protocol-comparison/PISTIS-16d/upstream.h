#ifndef _UPSTREAM_H_
#define _UPSTREAM_H_

#include "node.h"

#include <unordered_set>
#include <unordered_map>
#include <map>

#include <sodium.h>

class UpstreamNode : public Node
{
private:
    const static int exec_time_us;
    const static int timeout_rounds_ = 16; // Number of rounds to wait before checking for timeouts

    std::unordered_map<seq_num_t, std::vector<Signature>> proof_connectivity_;
    std::unordered_map<seq_num_t, std::vector<Signature>> echo_sigs_;
    std::unordered_map<seq_num_t, std::vector<Signature>> deliver_sigs_;

    std::unordered_map<seq_num_t, EchoMessage> echo_msgs_;
    std::unordered_map<seq_num_t, DeliverMessage> deliver_msgs_;

    seq_num_t cur_seq_num;

    void round_start();
    bool check_timeout();

    void send_msg(const Message &msg, uint64_t size);


    void wait_and_receive_msg();
    void process_msg(Message &msg);

    void process_heartbeat(Heartbeat &hb);
    void process_echo(EchoMessage &echo_msg);
    void process_deliver(DeliverMessage &deliver_msg);

public:
    UpstreamNode(int port, NodeType node_type, uint64_t id, int num_nodes);
    void run();
};

#endif