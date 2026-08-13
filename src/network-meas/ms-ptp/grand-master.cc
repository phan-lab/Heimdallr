#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <arpa/inet.h>

#include "grand-master.h"

uint64_t Server::getCurrentTimestamp()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

Server::Server(int port, uint32_t id, int num_clients)
    : server_id_(id), num_clients_(num_clients)
{
    assert(num_clients >= 3 * FAULTY_NODES + 1);
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
    /* Load its own secret key and clients' public keys */
    /* Secret key first */
    std::string sec_filename = "./keygen/keys/server/secret_" +
                               std::to_string(server_id_) + ".key";
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

    /* Now read the public keys of the clients */
    client_pub_keys_.resize(num_clients_);
    for (int i = 0; i < num_clients_; ++i)
    {
        std::string pub_filename = "./keygen/keys/client/public_" +
                                   std::to_string(i) + ".key";
        std::ifstream pub_file(pub_filename, std::ios::binary);
        if (!pub_file)
        {
            throw std::runtime_error("Cannot open file " + pub_filename);
        }
        if (!pub_file.read(reinterpret_cast<char *>(client_pub_keys_.at(i).data()),
                           crypto_sign_PUBLICKEYBYTES))
        {
            throw std::runtime_error("Fail to read file " + pub_filename);
        }
        pub_file.close();
    }

#endif
}

Server::~Server()
{
    close(socket_fd_);
}

void Server::sendSync()
{
    while (true)
    {
        uint64_t duration = profiler.reset_and_output();
        if (duration > 0)
            std::cerr << duration << std::endl
                      << std::flush;

        profiler.mark_start();
        PTPMessage sync_msg = {
            PTPMessageType::SYNC,
            getCurrentTimestamp(), // t1
            server_id_,
            0};

#if USE_CRYPTO
        std::array<uint8_t, sizeof(PTPMessage) + crypto_sign_BYTES> signed_buffer;
        unsigned long long signed_length;
        if (crypto_sign(signed_buffer.data(),
                        &signed_length,
                        reinterpret_cast<const uint8_t *>(&sync_msg),
                        sizeof(sync_msg),
                        my_sec_key_.data()))
        {
            throw std::runtime_error("Fail to sign");
        }
        assert(signed_length == signed_buffer.size());
#endif

        // Send to each client port (9000, 9001, ...)
        for (int i = 0; i < num_clients_; i++)
        {
            struct sockaddr_in client_addr;
            client_addr.sin_family = AF_INET;
            client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            client_addr.sin_port = htons(BASE_PORT_CLIENT + i);
#if USE_CRYPTO
            sendto(socket_fd_, signed_buffer.data(), signed_buffer.size(), 0,
                   (struct sockaddr *)&client_addr, sizeof(client_addr));
#else
            sendto(socket_fd_, &sync_msg, sizeof(sync_msg), 0,
                   (struct sockaddr *)&client_addr, sizeof(client_addr));
#endif
        }
        profiler.mark_end();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Server::handleDelayReq()
{
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);

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
                                   (struct sockaddr *)&client_addr, &len);
        if (bytes_recvd <= 0)
            continue;

        profiler.mark_start();
#if USE_CRYPTO
        /* Verify the signature first */
        int client_id = ntohs(client_addr.sin_port) - BASE_PORT_CLIENT;
        if (crypto_sign_verify_detached(recv_buf.data(),
                                        reinterpret_cast<const uint8_t *>(msg),
                                        sizeof(PTPMessage),
                                        client_pub_keys_.at(client_id).data()))
        {
            throw std::runtime_error("Invalid signature from client " + std::to_string(client_id));
        }
#endif
        if (msg->messageType == PTPMessageType::DELAY_REQ)
        {
            PTPMessage delay_resp = {
                PTPMessageType::DELAY_RESP,
                getCurrentTimestamp(),
                server_id_,
                msg->t2 // Pass back the t2 time
            };
#if USE_CRYPTO
            std::array<uint8_t, sizeof(PTPMessage) + crypto_sign_BYTES> signed_buffer;
            unsigned long long signed_length;
            if (crypto_sign(signed_buffer.data(),
                            &signed_length,
                            reinterpret_cast<const uint8_t *>(&delay_resp),
                            sizeof(delay_resp),
                            my_sec_key_.data()))
            {
                throw std::runtime_error("Fail to sign");
            }
            assert(signed_length == signed_buffer.size());
            sendto(socket_fd_, signed_buffer.data(), sizeof(signed_buffer), 0,
                   (struct sockaddr *)&client_addr, sizeof(client_addr));
#else
            sendto(socket_fd_, &delay_resp, sizeof(delay_resp), 0,
                   (struct sockaddr *)&client_addr, sizeof(client_addr));
#endif
        }
        else
        {
            std::cout << "Wrong type " << (int)msg->messageType << std::endl;
            exit(1);
        }
        profiler.mark_end();
    }
}

void Server::run()
{
    std::cout << "Server " << server_id_ << " started on port "
              << ntohs(address_.sin_port) << " for " << num_clients_ << " clients" << std::endl;

    // Start SYNC thread
    std::thread sync_thread(&Server::sendSync, this);

    // Start DELAY_REQ handling thread
    std::thread delay_thread(&Server::handleDelayReq, this);

    // Wait for threads (they will run forever)
    sync_thread.join();
    delay_thread.join();
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <server_id> <num_clients>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    uint32_t server_id = std::stoi(argv[2]);
    int num_clients_ = std::stoi(argv[3]);

    Server server(port, server_id, num_clients_);
    server.run();
    return 0;
}