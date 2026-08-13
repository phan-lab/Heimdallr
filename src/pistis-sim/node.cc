#include "node.h"

#include <random>

namespace {

std::mt19937 &rng() {
    static thread_local std::mt19937 gen(std::random_device{}());
    return gen;
}

inline uint32_t random_index(uint32_t n) {
    std::uniform_int_distribution<uint32_t> dist(0, n - 1);
    return dist(rng());
}

inline double rand_double() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng());
}

const seq_num_t EMPTY = ~static_cast<seq_num_t>(0);

}  // namespace

Node::Node(node_id_t id, Node **all_nodes, const SimConfig &cfg)
    : id(id), cur_seq_num(0), all_nodes(all_nodes), cfg_(cfg),
      window_(static_cast<size_t>(cfg.timeout_rounds) + 4),
      stamp_(window_, EMPTY), mask_(window_, 0) {
    tx_rounds_.reserve(window_);
    tx_masks_.reserve(window_);
}

/* Returns the mask slot for `round`, resetting it first if the slot currently
 * holds a different (older) round. */
uint64_t &Node::slot_mask(seq_num_t round) {
    size_t idx = static_cast<size_t>(round % window_);
    if (stamp_[idx] != round) {
        stamp_[idx] = round;
        mask_[idx] = 0;
    }
    return mask_[idx];
}

void Node::round_start() { cur_seq_num++; }

bool Node::check_timeout() {
    if (id < cfg_.faulty_nodes) {
        return false;  // faulty node never reports a timeout
    }
    if (cur_seq_num < cfg_.timeout_rounds) {
        return false;
    }
    seq_num_t timeout_seq = cur_seq_num - cfg_.timeout_rounds + 1;
    size_t idx = static_cast<size_t>(timeout_seq % window_);
    bool ok = stamp_[idx] == timeout_seq &&
              __builtin_popcountll(mask_[idx]) >= static_cast<int>(2 * cfg_.faulty_nodes + 1);
    // Consume the entry either way, exactly as the original erased it.
    stamp_[idx] = EMPTY;
    mask_[idx] = 0;
    return !ok;
}

void Node::send_heartbeat() {
    if (id < cfg_.faulty_nodes) {
        return;  // faulty node stays silent
    }

    slot_mask(cur_seq_num) |= (static_cast<uint64_t>(1) << id);

    /* Flatten the live part of the ring buffer into the outgoing heartbeat. */
    tx_rounds_.clear();
    tx_masks_.clear();
    for (size_t i = 0; i < window_; ++i) {
        if (stamp_[i] != EMPTY && mask_[i] != 0) {
            tx_rounds_.push_back(stamp_[i]);
            tx_masks_.push_back(mask_[i]);
        }
    }
    Heartbeat hb{tx_rounds_.data(), tx_masks_.data(),
                 static_cast<uint32_t>(tx_rounds_.size())};

    /* Pick `destination_size` distinct peers other than self. */
    uint64_t chosen = 0;
    node_id_t picked = 0;
    while (picked < cfg_.destination_size) {
        node_id_t dest = random_index(cfg_.total_nodes);
        uint64_t bit = static_cast<uint64_t>(1) << dest;
        if (dest == id || (chosen & bit)) {
            continue;
        }
        chosen |= bit;
        ++picked;
        if (rand_double() < cfg_.loss_rate) {
            continue;  // message lost
        }
        all_nodes[dest]->process_heartbeat(&hb);
    }
}

void Node::process_heartbeat(const Heartbeat *hb) {
    if (id < cfg_.faulty_nodes) {
        return;  // faulty nodes never act on what they receive
    }
    for (uint32_t i = 0; i < hb->count; ++i) {
        seq_num_t round = hb->rounds[i];
        /* Ignore rounds already consumed by check_timeout or too far ahead;
         * neither can affect a future timeout decision. */
        if (cur_seq_num + 2 < round || round + cfg_.timeout_rounds < cur_seq_num) {
            continue;
        }
        slot_mask(round) |= hb->masks[i];
    }
}

seq_num_t sim_round(int rounds, const SimConfig &cfg) {
    std::vector<Node *> nodes(cfg.total_nodes, nullptr);
    for (node_id_t i = 0; i < cfg.total_nodes; ++i) {
        nodes[i] = new Node(i, nodes.data(), cfg);
    }

    seq_num_t survived = static_cast<seq_num_t>(rounds);
    for (int round = 1; round <= rounds; ++round) {
        for (node_id_t i = 0; i < cfg.total_nodes; ++i) {
            nodes[i]->round_start();
            nodes[i]->send_heartbeat();
        }
        bool timed_out = false;
        for (node_id_t i = 0; i < cfg.total_nodes; ++i) {
            if (nodes[i]->check_timeout()) {
                timed_out = true;
                break;
            }
        }
        if (timed_out) {
            survived = static_cast<seq_num_t>(round);
            break;
        }
    }

    for (node_id_t i = 0; i < cfg.total_nodes; ++i) {
        delete nodes[i];
    }
    return survived;
}
