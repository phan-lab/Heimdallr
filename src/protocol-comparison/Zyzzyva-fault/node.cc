#include "node.h"

#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <sys/file.h>

Node::Node(int port, NodeType node_type, uint64_t id, int num_nodes)
    : my_id_(id), num_nodes_(num_nodes),
      current_job_id_(0)
{
    profiler.reset_and_output();

    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0)
    {
        std::cerr << "Socket creation failed" << std::endl;
        exit(1);
    }
    memset(&address_, 0, sizeof(address_));
    address_.sin_family = AF_INET;
    address_.sin_addr.s_addr = INADDR_ANY;
    address_.sin_port = htons(port);

    if (bind(socket_fd_, (struct sockaddr *)&address_, sizeof(address_)) < 0)
    {
        std::cerr << "Bind failed" << std::endl;
        exit(1);
    }

    int flags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

    /* Read the node's own secret key */
    std::string sec_filename;
    if (node_type == NodeType::UPSTREAM)
        sec_filename = "./keygen/keys/upstream/secret_" +
                       std::to_string(my_id_) + ".key";
    else if (node_type == NodeType::DOWNSTREAM)
        sec_filename = "./keygen/keys/downstream/secret_" +
                       std::to_string(my_id_) + ".key";
    else
        throw std::runtime_error("Invalid node type");

    std::cout << "Reading secret key from " << sec_filename << std::endl;

    std::ifstream sec_file(sec_filename, std::ios::binary);
    if (!sec_file)
    {
        throw std::runtime_error("Cannot open file " + sec_filename);
    }

    if (!sec_file.read(reinterpret_cast<char *>(my_sec_key_.data()),
                       crypto_sign_SECRETKEYBYTES))
    {
        throw std::runtime_error("Fail to read file " + sec_filename);
    }

    sec_file.close();

    /* Read the public keys of other nodes */
    upstream_pub_keys_.resize(num_nodes);
    downstream_pub_keys_.resize(num_nodes);
    for (int i = 0; i < num_nodes; ++i)
    {
        std::string pub_filename_up = "./keygen/keys/upstream/public_" +
                                      std::to_string(i) + ".key";
        std::ifstream pub_file_up(pub_filename_up, std::ios::binary);
        if (!pub_file_up)
        {
            throw std::runtime_error("Cannot open file " + pub_filename_up);
        }
        if (!pub_file_up.read(reinterpret_cast<char *>(
                                  upstream_pub_keys_.at(i).data()),
                              crypto_sign_PUBLICKEYBYTES))
        {
            throw std::runtime_error("Fail to read file " + pub_filename_up);
        }
        pub_file_up.close();

        std::string pub_filename_down = "./keygen/keys/downstream/public_" +
                                        std::to_string(i) + ".key";
        std::ifstream pub_file_down(pub_filename_down, std::ios::binary);
        if (!pub_file_down)
        {
            throw std::runtime_error("Cannot open file " + pub_filename_down);
        }
        if (!pub_file_down.read(reinterpret_cast<char *>(
                                    downstream_pub_keys_.at(i).data()),
                                crypto_sign_PUBLICKEYBYTES))
        {
            throw std::runtime_error("Fail to read file " + pub_filename_down);
        }
        pub_file_down.close();
    }

    std::cout << "Node " << my_id_ << " finished initialization\n"
              << std::flush;
}

Node::~Node()
{
    close(socket_fd_);
}