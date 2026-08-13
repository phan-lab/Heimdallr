#ifndef _NODE_H_
#define _NODE_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdint>
#include <vector>
#include <array>

#include "common.h"
#include "perf.h"


enum class NodeType
{
    INVALID = -1,
    UPSTREAM
};

class Node
{
protected:
    node_id_t my_id_;

    int num_nodes_; /* 3f+1 */
    int faulty_nodes_; /* f */
    int socket_fd_;
    struct sockaddr_in address_;

    seq_num_t current_job_id_;

    Profiler profiler;

    std::array<uint8_t, crypto_sign_SECRETKEYBYTES> my_sec_key_;
    std::vector<std::array<uint8_t, crypto_sign_PUBLICKEYBYTES>> upstream_pub_keys_;

public:
    Node(int port, NodeType node_type, node_id_t id, int num_nodes);
    ~Node();
};

#endif