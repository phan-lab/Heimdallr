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
    UPSTREAM,
    DOWNSTREAM
};

class Node
{
protected:
    uint64_t my_id_;

    int num_nodes_; /* Assuming # upstream == # downstream; should be f+1 */
    int socket_fd_;
    struct sockaddr_in address_;

    uint64_t current_job_id_;

    Profiler profiler;

    std::array<uint8_t, crypto_sign_SECRETKEYBYTES> my_sec_key_;
    std::vector<std::array<uint8_t, crypto_sign_PUBLICKEYBYTES>> upstream_pub_keys_;
    std::vector<std::array<uint8_t, crypto_sign_PUBLICKEYBYTES>> downstream_pub_keys_;

public:
    Node(int port, NodeType node_type, uint64_t id, int num_nodes);
    ~Node();
};

#endif