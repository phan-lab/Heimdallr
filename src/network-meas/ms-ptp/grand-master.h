#ifndef PTP_GRANDMASTER_H
#define PTP_GRANDMASTER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdint>
#include <vector>
#include <array>


#include "common.h"
#include "perf.h"

#if USE_CRYPTO

#include <sodium.h>
#endif

class Server
{
private:
    int socket_fd_;
    struct sockaddr_in address_;
    uint32_t server_id_;
    int num_clients_;

    uint64_t getCurrentTimestamp();
    void sendSync();
    void handleDelayReq();

    Profiler profiler;

#if USE_CRYPTO
    std::array<uint8_t, crypto_sign_SECRETKEYBYTES> my_sec_key_;
    std::vector<std::array<uint8_t, crypto_sign_PUBLICKEYBYTES>> client_pub_keys_;
#endif

public:
    Server(int port, uint32_t id, int num_clients);
    ~Server();
    void run();
};

#endif // PTP_GRANDMASTER_H