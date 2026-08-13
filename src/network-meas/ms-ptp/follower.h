// follower.h
#ifndef PTP_FOLLOWER_H
#define PTP_FOLLOWER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

#include "common.h"
#include "perf.h"

#if USE_CRYPTO
#include <vector>
#include <array>
#include <sodium.h>
#endif


struct ServerMeasurement
{
    uint64_t offset{0};
    uint64_t delay{0};
};

class Client
{
private:
    Profiler profiler;
    int client_id_;
    int num_servers_;
    int socket_fd_;
    struct sockaddr_in address_;
    std::unordered_map<uint64_t, ServerMeasurement> server_measurements_;

    uint64_t getCurrentTimestamp();
    ServerMeasurement aggregate_results();

#if USE_CRYPTO
    std::array<uint8_t, crypto_sign_SECRETKEYBYTES> my_sec_key_;
    std::vector<std::array<uint8_t, crypto_sign_PUBLICKEYBYTES>> server_pub_keys_;
#endif

public:
    Client(int port, int client_id, int num_servers);
    ~Client();
    void run();
};

#endif // PTP_FOLLOWER_H