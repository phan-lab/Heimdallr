/* PISTIS reliable-broadcast round simulator - node model.
 *
 * Parameterised copy of micros/reputation/pistis/node.h.  The original fixed
 * the fault bound, the fan-out and the loss rate as compile-time constants;
 * here they are runtime configuration so one binary covers every T/d setting
 * reported in Table III / Fig. 8.
 *
 * Two behaviour-preserving optimisations relative to the original, without
 * which a 30-day horizon is impractical on a workstation:
 *
 *  1. The per-round signature set is a bitmask over node ids held in a ring
 *     buffer of the last (T + 4) rounds, instead of an
 *     unordered_map<round, vector<Signature>> that is rebuilt and re-scanned
 *     every round.  A heartbeat only ever carries rounds inside the sender's
 *     own T-round window, so nothing observable is dropped.
 *  2. Faulty nodes (ids < f) discard what they receive.  They never send a
 *     heartbeat and never report a timeout, so their state cannot influence
 *     the outcome - but in the original it was the one map that was never
 *     pruned, growing to hundreds of MB per run.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

typedef uint32_t node_id_t;
typedef uint64_t seq_num_t;

struct SimConfig {
    node_id_t faulty_nodes = 2;      // f
    node_id_t total_nodes = 7;       // 3f + 1
    node_id_t destination_size = 2;  // heartbeat fan-out
    double loss_rate = 0.001;        // 1 - P_norm
    seq_num_t timeout_rounds = 12;   // T expressed in heartbeat periods d
};

/* A heartbeat carries every (round, signer-set) pair the sender currently
 * holds, i.e. its whole ring buffer. */
struct Heartbeat {
    const seq_num_t *rounds;
    const uint64_t *masks;
    uint32_t count;
};

class Node {
  public:
    Node(node_id_t id, Node **all_nodes, const SimConfig &cfg);

    node_id_t id;
    seq_num_t cur_seq_num;
    Node **all_nodes;

    void round_start();
    bool check_timeout();
    void send_heartbeat();
    void process_heartbeat(const Heartbeat *hb);

  private:
    const SimConfig &cfg_;
    size_t window_;                  // ring size = T + 4
    std::vector<seq_num_t> stamp_;   // round id currently stored in each slot
    std::vector<uint64_t> mask_;     // signers of that round, as a bitmask

    // Scratch buffers for the outgoing heartbeat, reused across rounds.
    std::vector<seq_num_t> tx_rounds_;
    std::vector<uint64_t> tx_masks_;

    uint64_t &slot_mask(seq_num_t round);
};

/* Runs one instance; returns the round in which a correct node timed out, or
 * `rounds` if none did. */
seq_num_t sim_round(int rounds, const SimConfig &cfg);
