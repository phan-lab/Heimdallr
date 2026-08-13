#include "upstream.h"
#include <iomanip>
#include <iostream>
#include <cstring>
#include <thread>
#include <set>
#include <arpa/inet.h>
#include <random>

/* high is inclusive */
int random_int(int low, int high)
{
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(low, high);
    return dist(gen);
}

const int UpstreamNode::exec_time_us = 0;

void dumpMemory(const void *addr, size_t length)
{
    const uint8_t *ptr = static_cast<const uint8_t *>(addr);

    for (size_t i = 0; i < length; ++i)
    {
        std::cerr << std::hex << std::setfill('0') << std::setw(2)
                  << static_cast<int>(ptr[i]) << " ";

        // Add extra space every 8 bytes
        if ((i + 1) % 8 == 0)
        {
            std::cerr << " ";
        }

        // New line every 16 bytes
        if ((i + 1) % 16 == 0)
        {
            std::cerr << std::endl;
        }
    }

    // Add final newline if needed
    if (length % 16 != 0)
    {
        std::cerr << std::endl;
    }

    std::cerr << std::dec << std::endl; // Reset to decimal format
}

UpstreamNode::UpstreamNode(
    int port, NodeType node_type, uint64_t id, int num_nodes)
    : Node(port, node_type, id, num_nodes),
      cur_seq_num(0)
{
    assert(num_nodes_ == 3 * FAULTY_NODES + 1);
}

bool UpstreamNode::check_timeout()
{
    seq_num_t timeout_seq = cur_seq_num - timeout_rounds_ + 1;
    if (proof_connectivity_.find(timeout_seq) == proof_connectivity_.end())
    {
        return true; // Timeout occurred, no signatures for this sequence number
    }
    else
    {
        const auto &sig_vec = proof_connectivity_.at(timeout_seq);
        if (static_cast<int>(sig_vec.size()) < 2 * faulty_nodes_ + 1)
        {
            /* remove the entry */
            proof_connectivity_.erase(timeout_seq);
            return true;
        }
    }
    proof_connectivity_.erase(timeout_seq); // Remove the entry after checking
    return false;
}

void UpstreamNode::round_start()
{
    cur_seq_num++;

    /* Create a HB message */
    Heartbeat hb;
    hb.from_node_id = my_id_;
    hb.seq_num = cur_seq_num;

    /* Sign */
    Signature sig;
    sig.node_id = my_id_;
    sig.round_id = cur_seq_num;
    crypto_sign_detached(sig.sig, nullptr,
                         reinterpret_cast<uint8_t *>(&sig),
                         sizeof(sig.node_id) + sizeof(sig.round_id),
                         my_sec_key_.data());
    // std::cerr << "dumping HB sig from node " << my_id_ << " for round " << cur_seq_num << std::endl;
    // dumpMemory(sig.sig, crypto_sign_BYTES);
    // int ret = crypto_sign_verify_detached(
    //     sig.sig, reinterpret_cast<const uint8_t *>(&hb),
    //     sizeof(hb.from_node_id) + sizeof(hb.seq_num),
    //     upstream_pub_keys_[sig.node_id].data());
    // if (ret != 0)
    // {
    //     std::cerr << "Invalid signature from OWN " << sig.node_id
    //               << " for round " << sig.round_id << std::endl;

    //     dumpMemory(upstream_pub_keys_[sig.node_id].data(),
    //                crypto_sign_PUBLICKEYBYTES);
    //     dumpMemory(my_sec_key_.data(), crypto_sign_SECRETKEYBYTES);
    //     exit(1);
    // }

    if (proof_connectivity_.find(cur_seq_num) == proof_connectivity_.end())
    {
        proof_connectivity_[cur_seq_num] = std::vector<Signature>();
    }
    proof_connectivity_[cur_seq_num].push_back(sig);

    // hb.num_pocs = proof_connectivity_[cur_seq_num].size();
    // hb.poc_sigs = proof_connectivity_[cur_seq_num];
    hb.num_pocs = 0;
    hb.poc_sigs = std::vector<Signature>();
    for (const auto &poc : proof_connectivity_)
    {
        auto &sigs = poc.second;
        for (const auto &sig : sigs)
        {
            hb.num_pocs++;
            hb.poc_sigs.emplace_back(sig);
        }
    }

    if (my_id_ == 0 && cur_seq_num == 1)
    {
        /**
         * Node 0 will initiate a broadcast. Send echo message.
         */
        EchoMessage echo_msg;
        echo_msg.from_node_id = my_id_;
        echo_msg.seq_num = cur_seq_num;
        echo_msg.val_size = input_len;
        echo_msg.val.resize(input_len);
        std::fill(echo_msg.val.begin(), echo_msg.val.end(), 0x01);
        echo_msg.num_sigs = 1;

        echo_msg.echo_sigs.resize(1);
        /* Sign the echo message */
        // crypto_sign_detached(echo_msg.echo_sigs[0].sig, nullptr,
        //                      reinterpret_cast<uint8_t *>(&echo_msg),
        //                      sizeof(echo_msg.from_node_id) + sizeof(echo_msg.seq_num) +
        //                          sizeof(echo_msg.val_size) + echo_msg.val_size,
        //                      my_sec_key_.data());
        EchoMessageHelper::sign(my_sec_key_.data(), echo_msg, echo_msg.echo_sigs[0].sig);

        /* Add to the set*/
        if (echo_sigs_.find(cur_seq_num) == echo_sigs_.end())
        {
            echo_sigs_[cur_seq_num] = std::vector<Signature>();
        }
        echo_sigs_.at(cur_seq_num).push_back(echo_msg.echo_sigs[0]);
        echo_msgs_.insert({cur_seq_num, echo_msg});
    }
    /* if 2f+1 echos, generate a deliver message */
    for (auto &echo : echo_sigs_)
    {
        if (echo.second.size() >= 2 * FAULTY_NODES + 1 && deliver_msgs_.find(echo.first) == deliver_msgs_.end())
        {
            /* Create a deliver message */
            DeliverMessage deliver_msg;
            deliver_msg.from_node_id = my_id_;
            deliver_msg.seq_num = echo.first;
            deliver_msg.val_size = echo_msgs_[echo.first].val_size;
            deliver_msg.val = echo_msgs_[echo.first].val;
            deliver_msg.num_echo_sigs = echo.second.size();
            deliver_msg.echo_sigs = echo.second;
            deliver_msg.num_deliver_sigs = 1;

            /* Sign all fields except for deliver_sigs */
            DeliverMessageHelper::sign(my_sec_key_.data(), deliver_msg, my_id_, echo.first);
            // deliver_msg.deliver_sigs[0].node_id = my_id_;
            // deliver_msg.deliver_sigs[0].round_id = echo.first;

            deliver_sigs_[echo.first] = deliver_msg.deliver_sigs;

            deliver_msgs_.insert({echo.first, deliver_msg});

            // std::cerr << "Signed a delivery message due to 2f+1 echos" << std::endl;
        }
    }
    /* If there is any deliver sig, send hb+deliver */
    if (!deliver_sigs_.empty())
    {
        assert(deliver_msgs_.size() == 1);
        Message msg;
        msg.type = MessageType::DELIVER;
        uint32_t size = HeartbeatAndDeliverHelper::serialize(hb, deliver_msgs_.begin()->second, msg.buf);

        this->send_msg(msg, size + sizeof(msg.type));
        // std::cout << "Node " << my_id_ << " sent a deliver message for seq_num " << cur_seq_num << " with size "
        //           << size << std::endl;
    }
    else if (!echo_sigs_.empty())
    {
        /* If there is no deliver sig, send hb+echo */
        assert(echo_msgs_.size() == 1);
        Message msg;
        msg.type = MessageType::ECHO;
        uint32_t size = HeartbeatAndEchoHelper::serialize(hb, echo_msgs_.begin()->second, msg.buf);
        this->send_msg(msg, size + sizeof(msg.type));
        // std::cerr << "echo msg address " << static_cast<void *>(msg.buf) << std::endl;

        // std::cerr << "num sigs in hb message: "
        //           << hb.poc_sigs.size() << std::endl;
        // std::cerr << "num sigs in echo message: "
        //           << echo_msgs_.begin()->second.echo_sigs.size() << std::endl;
        // std::cout << "Node " << my_id_ << " sent an echo message for seq_num " << cur_seq_num << " with size "
        //           << size << std::endl;
        // EchoMessageHelper::dump(echo_msgs_.begin()->second);
    }
    else
    {
        /* If there is no echo sig, send only hb */
        Message msg;
        msg.type = MessageType::PLAIN_HB;
        uint32_t size = PlainHeartbeatHelper::serialize(hb, msg.buf);
        this->send_msg(msg, size + sizeof(msg.type));
        // std::cout << "Node " << my_id_ << " sent a plain heartbeat for seq_num " << cur_seq_num << " with size "
        //           << size << std::endl;
        // std::cerr << "dumping plain hb sent \n";
        // dumpMemory(msg.buf, size - sizeof(msg.type));
    }
}

void UpstreamNode::send_msg(const Message &msg, uint64_t size)
{
    /* Generate `destination_size` distinct random numbers from the set of node IDs */
    std::set<node_id_t> destinations;
    while (static_cast<int>(destinations.size()) < faulty_nodes_ + 1)
    {
        node_id_t random_id = random_int(0, num_nodes_ - 1);
        if (random_id != my_id_ && destinations.find(random_id) == destinations.end())
        {
            destinations.insert(random_id);
        }
    }
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    for (const auto &dest : destinations)
    {
        dest_addr.sin_port = htons(BASE_PORT_UPSTREAM + dest);
        auto sent = sendto(socket_fd_,
                           &msg,
                           size,
                           0,
                           (struct sockaddr *)&dest_addr,
                           sizeof(dest_addr));
        if (static_cast<uint64_t>(sent) != size)
        {
            throw std::runtime_error("Error! " + std::to_string(sent) +
                                     " bytes sent instead of " + std::to_string(size));
        }
        // std::cout << "Node " << my_id_ << " sent a message of type "
        //           << static_cast<int>(msg.type) << " to node " << dest << std::endl;
    }
}

void UpstreamNode::wait_and_receive_msg()
{
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    Message msg;

    /* set a timeout for receive */
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000; // 10 milliseconds
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    // Wait for a message
    while (1)
    {
        int bytes_recvd = recvfrom(socket_fd_, &msg, sizeof(msg), 0,
                                   (struct sockaddr *)&client_addr, &len);
        if (bytes_recvd <= 0)
            return;
        profiler.mark_start();
        process_msg(msg);
        profiler.mark_end();
    }
}

void UpstreamNode::process_msg(Message &msg)
{
    switch (msg.type)
    {
    case MessageType::PLAIN_HB:
    {
        Heartbeat hb;
        // std::cerr << "dumping plain hb got \n";
        // dumpMemory(msg.buf, 96);
        PlainHeartbeatHelper::deserialize(msg.buf, hb);
        this->process_heartbeat(hb);
        break;
    }
    case MessageType::ECHO:
    {
        Heartbeat hb;
        EchoMessage echo_msg;
        HeartbeatAndEchoHelper::deserialize(msg.buf, hb, echo_msg);
        this->process_heartbeat(hb);
        this->process_echo(echo_msg);
        break;
    }
    case MessageType::DELIVER:
    {
        Heartbeat hb;
        DeliverMessage deliver_msg;
        HeartbeatAndDeliverHelper::deserialize(msg.buf, hb, deliver_msg);
        this->process_heartbeat(hb);
        this->process_deliver(deliver_msg);
        break;
    }
    default:
        std::cerr << "Unknown message type: " << static_cast<int>(msg.type) << std::endl;
    }
}

void UpstreamNode::process_heartbeat(Heartbeat &hb)
{
    //     std::cout << "Node " << my_id_ << " received a heartbeat from node "
    //               << hb.from_node_id << " for seq_num " << hb.seq_num
    //               << " with " << hb.num_pocs << " proofs of connectivity." << std::endl;
    assert(hb.num_pocs == hb.poc_sigs.size());

    for (node_id_t i = 0; i < hb.num_pocs; i++)
    {
        const Signature &sig = hb.poc_sigs[i];
        if (proof_connectivity_.find(sig.round_id) == proof_connectivity_.end())
        {
            proof_connectivity_[sig.round_id] = std::vector<Signature>();
        }
        auto &sig_vec = proof_connectivity_.at(sig.round_id);
        bool found = false;
        for (const auto &existing_sig : sig_vec)
        {
            if (existing_sig.node_id == sig.node_id)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            /* Before pushing, verify the signature */

            int ret = crypto_sign_verify_detached(
                sig.sig, reinterpret_cast<const uint8_t *>(&sig),
                sizeof(sig.node_id) + sizeof(sig.round_id),
                upstream_pub_keys_[sig.node_id].data());
            if (ret != 0)
            {
                std::cerr << "Invalid signature from node " << sig.node_id
                          << " of round " << hb.seq_num << std::endl;
                std::cerr << "The sig is from the HB sent by node "
                          << hb.from_node_id << " with seq_num "
                          << hb.seq_num << std::endl;
                std::cerr << "This is the " << i + 1 << "th signature in the HB." << std::endl;
                std::cerr << "Dumping the sig:\n";
                dumpMemory(sig.sig, crypto_sign_BYTES);
                std::cerr << "Dumping the data:\n";
                dumpMemory(reinterpret_cast<const uint8_t *>(&sig),
                           sizeof(sig.node_id) + sizeof(sig.round_id));
                // std::cerr << "HB address is " << &hb << std::endl;
                // std::cerr << "hb.from_node_id address is " << &hb.from_node_id
                //           << " and hb.seq_num address is " << &hb.seq_num << std::endl;
                exit(1);
            }

            sig_vec.push_back(sig);
        }
    }
}

void UpstreamNode::process_echo(EchoMessage &echo_msg)
{
    // std::cout << "Node " << my_id_ << " received an echo message from node "
    //           << echo_msg.from_node_id << " for seq_num " << echo_msg.seq_num
    //           << std::endl;
    seq_num_t echo_seq = echo_msg.seq_num;
    if (echo_sigs_.find(echo_seq) == echo_sigs_.end())
    {
        echo_sigs_[echo_seq] = std::vector<Signature>();
    }
    auto &sig_vec = echo_sigs_.at(echo_seq);
    for (auto &echo_msg_sig : echo_msg.echo_sigs)
    {
        bool found = false;
        for (const auto &existing_sig : sig_vec)
        {
            if (existing_sig.node_id == echo_msg_sig.node_id &&
                existing_sig.round_id == echo_msg_sig.round_id)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            /* verify the signature */
            int ret = EchoMessageHelper::verify(
                upstream_pub_keys_[echo_msg_sig.node_id].data(), echo_msg,
                echo_msg_sig.sig);
            if (ret != 0)
            {
                std::cerr << "Invalid signature from node " << echo_msg_sig.node_id
                          << " for echo message of round " << echo_seq << std::endl;
                dumpMemory(echo_msg_sig.sig, crypto_sign_BYTES);
                exit(1);
            }
            sig_vec.push_back(echo_msg_sig);
        }
    }
    /* add its own sig if it does not exist yet */
    bool found = false;
    for (const auto &existing_sig : sig_vec)
    {
        if (existing_sig.node_id == my_id_ && existing_sig.round_id == echo_seq)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        Signature sig;
        sig.node_id = my_id_;
        sig.round_id = echo_seq;
        // crypto_sign_detached(sig.sig, nullptr,
        //                      reinterpret_cast<uint8_t *>(&echo_msg),
        //                      sizeof(echo_msg.from_node_id) + sizeof(echo_msg.seq_num) +
        //                          sizeof(echo_msg.val_size) + echo_msg.val_size,
        //                      my_sec_key_.data());
        EchoMessageHelper::sign(my_sec_key_.data(), echo_msg, sig.sig);
        sig_vec.push_back(sig);
    }

    /* add to echo_msgs_ if necessary */
    if (echo_msgs_.find(echo_seq) == echo_msgs_.end())
    {
        echo_msgs_[echo_seq] = echo_msg;
    }
    echo_msgs_.at(echo_seq).echo_sigs = sig_vec;
    echo_msgs_.at(echo_seq).num_sigs = sig_vec.size();
}

void UpstreamNode::process_deliver(DeliverMessage &deliver_msg)
{
    // std::cout << "Node " << my_id_ << " received a deliver message from node "
    //           << deliver_msg.from_node_id << " for seq_num " << deliver_msg.seq_num
    //           << std::endl;
    seq_num_t deliver_seq = deliver_msg.seq_num;

    if (deliver_sigs_.find(deliver_seq) == deliver_sigs_.end())
    {
        // deliver_sigs_[deliver_seq] = std::vector<Signature>();
        deliver_sigs_.insert({deliver_seq, std::vector<Signature>()});
        deliver_msgs_.insert({deliver_seq, deliver_msg});
    }

    auto &sig_vec = deliver_sigs_.at(deliver_seq);
    for (auto &deliver_msg_sig : deliver_msg.deliver_sigs)
    {
        bool found = false;
        for (const auto &existing_sig : sig_vec)
        {
            // std::cerr << "Checking existing sig for deliver " << existing_sig.node_id
            //           << " at round " << existing_sig.round_id << std::endl;
            if (existing_sig.node_id == deliver_msg_sig.node_id &&
                existing_sig.round_id == deliver_msg_sig.round_id)
            {
                found = true;
                break;
            }
        }
        // std::cerr << "Found existing sig for deliver " << deliver_msg_sig.node_id << "? " << found << " at round " << cur_seq_num << std::endl;
        if (!found)
        {
            /* verify the signature */
            int ret = DeliverMessageHelper::verify(upstream_pub_keys_, deliver_msg);
            if (ret != 0)
            {
                std::cerr << "Invalid signature from node " << deliver_msg_sig.node_id
                          << " for deliver message in round " << cur_seq_num << std::endl;
                dumpMemory(deliver_msg_sig.sig, crypto_sign_BYTES);
                // exit(1);
            }
            sig_vec.push_back(deliver_msg_sig);
        }
    }
    /* add its own sig if it does not exist yet */
    bool found = false;
    for (const auto &existing_sig : sig_vec)
    {
        if (existing_sig.node_id == my_id_ && existing_sig.round_id == deliver_seq)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        // DeliverMessage deliver_msg = deliver_msgs_.at(deliver_seq);
        // std::cerr << "Assert before sign \n";
        // assert(DeliverMessageHelper::verify(upstream_pub_keys_, deliver_msg) == 0);
        DeliverMessageHelper::sign(my_sec_key_.data(), deliver_msg, my_id_, deliver_seq);
        // std::cerr << "Assert after sign \n";
        // assert(DeliverMessageHelper::verify(upstream_pub_keys_, deliver_msg) == 0);

        deliver_sigs_.at(deliver_seq) = deliver_msg.deliver_sigs;

        deliver_msgs_.at(deliver_seq) = deliver_msg;
    }
}

void UpstreamNode::run()
{
    for (int i = 0; i < timeout_rounds_; i++)
    {
        profiler.mark_start();
        round_start();
        profiler.mark_end();

        wait_and_receive_msg();

        profiler.mark_start();
        check_timeout();
        profiler.mark_end();
        std::this_thread::sleep_for(std::chrono::microseconds(20000));
    }
    auto time = profiler.reset_and_output();
    std::cerr << time << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <node_id> <num_nodes>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    uint32_t server_id = std::stoi(argv[2]);
    int num_nodes = std::stoi(argv[3]);

    while (1)
    {
        UpstreamNode node(port, NodeType::UPSTREAM, server_id, num_nodes);
        node.run();
    }
    return 0;
}