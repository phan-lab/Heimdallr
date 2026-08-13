#include <iostream>
#include <thread>
#include <cmath>
#include <cstring>
#include <cassert>
#include <cerrno>
#include <fstream>
#include <arpa/inet.h>
#include <unistd.h>
#include <unordered_map>
#include <algorithm>
#include <map>

#include "follower.h"

uint64_t Client::getCurrentTimestamp()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void Client::run()
{
    int updated = 0;
    struct TimestampSet
    {
        uint64_t t1;
        uint64_t t2;
        uint64_t t3;
    };
    std::map<uint64_t, TimestampSet> timestamps; // key is server_id

    struct sockaddr_in recv_addr;
    socklen_t len = sizeof(recv_addr);

#if USE_CRYPTO
    size_t msg_len = sizeof(PTPMessage) + crypto_sign_BYTES;
    std::vector<uint8_t> recv_buf(msg_len);
    PTPMessage *msg = reinterpret_cast<PTPMessage *>(
        recv_buf.data() + crypto_sign_BYTES);
#else
    size_t msg_len = sizeof(PTPMessage);
    std::vector<uint8_t> recv_buf(msg_len);
    PTPMessage *msg = reinterpret_cast<PTPMessage *>(recv_buf.data());
#endif
    while (true)
    {
        int bytes_recvd = recvfrom(socket_fd_, recv_buf.data(), msg_len, 0,
                                   (struct sockaddr *)&recv_addr, &len);
        if (bytes_recvd <= 0)
            continue;

        profiler.mark_start();

#if USE_CRYPTO
        /* Verify the signature first */
        int server_id = ntohs(recv_addr.sin_port) - BASE_PORT_SERVER;
        if (crypto_sign_verify_detached(recv_buf.data(),
                                        reinterpret_cast<const uint8_t *>(msg),
                                        sizeof(PTPMessage),
                                        server_pub_keys_.at(server_id).data()))
        {
            throw std::runtime_error("Invalid signature from server " + std::to_string(server_id));
        }
#endif
        switch (msg->messageType)
        {
        case PTPMessageType::SYNC:
        {
            uint64_t server_id = msg->server_id;
            timestamps[server_id].t1 = msg->timestamp;
            timestamps[server_id].t2 = getCurrentTimestamp();

            // Send DELAY_REQ
            timestamps[server_id].t3 = getCurrentTimestamp();
            PTPMessage delay_req = {
                PTPMessageType::DELAY_REQ,
                timestamps[server_id].t3,
                server_id,
                timestamps[server_id].t2};

#if USE_CRYPTO
            std::array<uint8_t, sizeof(PTPMessage) + crypto_sign_BYTES> signed_buffer;
            unsigned long long signed_length;
            if (crypto_sign(signed_buffer.data(),
                            &signed_length,
                            reinterpret_cast<const uint8_t *>(&delay_req),
                            sizeof(delay_req),
                            my_sec_key_.data()))
            {
                throw std::runtime_error("Fail to sign");
            }
            assert(signed_length == signed_buffer.size());
            sendto(socket_fd_, signed_buffer.data(), sizeof(signed_buffer), 0,
                   (struct sockaddr *)&recv_addr, sizeof(recv_addr));
#else
            sendto(socket_fd_, &delay_req, sizeof(delay_req), 0,
                   (struct sockaddr *)&recv_addr, sizeof(recv_addr));
#endif
            profiler.mark_end();

            break;
        }

        case PTPMessageType::DELAY_RESP:
        {
            uint64_t server_id = msg->server_id;
            auto &ts = timestamps[server_id];
            uint64_t t4 = msg->timestamp;

            int64_t diff_down = static_cast<int64_t>(ts.t2) - static_cast<int64_t>(ts.t1);
            int64_t diff_up = static_cast<int64_t>(t4) - static_cast<int64_t>(ts.t3);

            int64_t delay = (diff_down + diff_up) / 2;
            int64_t offset = (diff_down - diff_up) / 2;

            server_measurements_[server_id] = {
                static_cast<uint64_t>(offset),
                static_cast<uint64_t>(delay)};

            if (updated++ == num_servers_)
            {
                updated = 0;
                if (server_measurements_.size() > FAULTY_NODES)
                {
                    auto agg_meas = aggregate_results();
#if NEED_OFFSET
                    std::cout << "Aggregated offset: " << agg_meas.offset << std::endl;
#endif
                    std::cout << "Aggregated latency: " << agg_meas.delay << std::endl;
                    std::cout << std::endl;
                }
                profiler.mark_end();
                uint64_t duration = profiler.reset_and_output();
                std::cerr << duration << std::endl << std::flush;
            }
            else
            {
                profiler.mark_end();
            }

            break;
        }
        default:
            throw std::runtime_error("Wrong type of message received!");
            break;
        }
    }
}

Client::Client(int port, int client_id, int num_servers)
    : client_id_(client_id), num_servers_(num_servers)
{
    assert(num_servers_ >= 3 * FAULTY_NODES + 1);
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

#if USE_CRYPTO
    /* Load its own secret key and servers' public keys */
    /* Secret key first */
    std::string sec_filename = "./keygen/keys/client/secret_" +
                               std::to_string(client_id_) + ".key";
    std::ifstream sec_file(sec_filename, std::ios::binary);
    if (!sec_file)
    {
        throw std::runtime_error("Cannot open file " + sec_filename + strerror(errno));
    }

    if (!sec_file.read(reinterpret_cast<char *>(my_sec_key_.data()),
                       crypto_sign_SECRETKEYBYTES))
    {
        throw std::runtime_error("Fail to read file " + sec_filename);
    }

    /* Public keys of the servers */
    server_pub_keys_.resize(num_servers_);
    for (int i = 0; i < num_servers_; ++i)
    {
        std::string pub_filename = "./keygen/keys/server/public_" +
                                   std::to_string(i) + ".key";
        std::ifstream pub_file(pub_filename, std::ios::binary);
        if (!pub_file)
        {
            throw std::runtime_error("Cannot open file " + pub_filename);
        }
        if (!pub_file.read(reinterpret_cast<char *>(server_pub_keys_.at(i).data()),
                           crypto_sign_PUBLICKEYBYTES))
        {
            throw std::runtime_error("Fail to read file " + pub_filename);
        }
        pub_file.close();
    }
#endif
}

Client::~Client()
{
    close(socket_fd_);
}

struct ServerMeasurement Client::aggregate_results()
{
    struct ServerMeasurement agg_meas;
#if NEED_OFFSET
    std::vector<std::pair<double, size_t>> offset_scores;
#endif
    std::vector<std::pair<double, size_t>> latency_scores;

    for (const auto &[server_id, measurement] : server_measurements_)
    {
#if NEED_OFFSET
        double offset_score = 0;
#endif
        double latency_score = 0;
        for (const auto &[server_id_inner, measurement_inner] : server_measurements_)
        {
#if NEED_OFFSET
            offset_score += pow(measurement.offset - measurement_inner.offset, 2);
#endif
            latency_score += pow(measurement.delay - measurement_inner.delay, 2);
        }
#if NEED_OFFSET
        offset_scores.emplace_back(std::make_pair(offset_score, server_id));
#endif
        latency_scores.emplace_back(std::make_pair(latency_score, server_id));
    }
#if NEED_OFFSET
    std::sort(offset_scores.begin(), offset_scores.end());
#endif
    std::sort(latency_scores.begin(), latency_scores.end());

    for (int i = 0; i < FAULTY_NODES + 1; i++)
    {
#if NEED_OFFSET
        agg_meas.offset += server_measurements_[offset_scores.at(i).second].offset / (FAULTY_NODES + 1);
#endif
        agg_meas.delay += server_measurements_[latency_scores.at(i).second].delay / (FAULTY_NODES + 1);
    }
    return agg_meas;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << "<client_port> <client_id> <num_servers>"
                  << std::endl;
        return 1;
    }

    int clientPort = std::stoi(argv[1]);
    Client client(clientPort, std::stoi(argv[2]), std::stoi(argv[3]));
    std::cout << "Client started on port " << clientPort << std::endl;
    client.run();
    return 0;
}