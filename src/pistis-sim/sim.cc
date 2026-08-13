/* PISTIS operational-robustness simulator.
 *
 * Reproduces the PISTIS rows of Table III and the PISTIS curves of Fig. 8:
 * for a given timeout-to-heartbeat ratio T/d it reports
 *   - Pr(T_robust >= horizon), the fraction of runs that never timed out, and
 *   - the normalised mean T_robust.
 *
 * Parameterised replacement for micros/reputation/pistis/sim.cc, which fixed
 * the instance count and horizon in main() and printed prose.  This version
 * takes every knob on the command line and emits one CSV row per T/d so the
 * plotting stage needs no manual transcription.
 */
#include "node.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Options {
    int instances = 10000;
    int rounds = 86400 * 30;
    double loss_rate = 0.001;
    node_id_t faulty = 2;
    unsigned threads = 0;  // 0 -> hardware_concurrency
    std::vector<seq_num_t> timeouts{8, 12, 14, 16};
    std::string out;
    bool quiet = false;
};

void usage(const char *argv0) {
    std::fprintf(stderr,
                 "Usage: %s [options]\n"
                 "  --instances N     runs per T/d setting        (default 10000)\n"
                 "  --rounds N        heartbeat rounds per run    (default 2592000 = 30 d @ 1 Hz)\n"
                 "  --timeouts LIST   comma separated T/d values  (default 8,12,14,16)\n"
                 "  --loss RATE       message loss rate 1-P_norm  (default 0.001)\n"
                 "  --faulty F        Byzantine nodes, n = 3F+1   (default 2)\n"
                 "  --threads N       worker threads              (default: all cores)\n"
                 "  --out PATH        CSV output                  (default: stdout)\n"
                 "  --quiet           suppress progress lines\n",
                 argv0);
}

std::vector<seq_num_t> parse_list(const char *s) {
    std::vector<seq_num_t> out;
    const char *p = s;
    while (*p) {
        char *end = nullptr;
        long long v = std::strtoll(p, &end, 10);
        if (end == p) break;
        if (v > 0) out.push_back(static_cast<seq_num_t>(v));
        p = (*end == ',') ? end + 1 : end;
    }
    return out;
}

}  // namespace

int main(int argc, char *argv[]) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char * {
            if (i + 1 >= argc) {
                usage(argv[0]);
                std::exit(1);
            }
            return argv[++i];
        };
        if (a == "--instances") opt.instances = std::atoi(next());
        else if (a == "--rounds") opt.rounds = std::atoi(next());
        else if (a == "--timeouts") opt.timeouts = parse_list(next());
        else if (a == "--loss") opt.loss_rate = std::atof(next());
        else if (a == "--faulty") opt.faulty = static_cast<node_id_t>(std::atoi(next()));
        else if (a == "--threads") opt.threads = static_cast<unsigned>(std::atoi(next()));
        else if (a == "--out") opt.out = next();
        else if (a == "--quiet") opt.quiet = true;
        else { usage(argv[0]); return a == "-h" || a == "--help" ? 0 : 1; }
    }

    unsigned num_threads = opt.threads ? opt.threads : std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::FILE *out = stdout;
    if (!opt.out.empty()) {
        out = std::fopen(opt.out.c_str(), "w");
        if (!out) {
            std::perror("fopen");
            return 1;
        }
    }
    std::fprintf(out, "timeout_rounds,survive_rate,avg_survive_time,instances,rounds\n");

    for (seq_num_t timeout : opt.timeouts) {
        SimConfig cfg;
        cfg.faulty_nodes = opt.faulty;
        cfg.total_nodes = 3 * opt.faulty + 1;
        cfg.destination_size = opt.faulty;
        cfg.loss_rate = opt.loss_rate;
        cfg.timeout_rounds = timeout;

        std::atomic<int> next_task{0};
        std::atomic<int> success{0};
        std::atomic<long long> total_rounds{0};

        auto worker = [&]() {
            for (;;) {
                int task = next_task.fetch_add(1);
                if (task >= opt.instances) return;
                seq_num_t survived = sim_round(opt.rounds, cfg);
                if (survived == static_cast<seq_num_t>(opt.rounds)) success.fetch_add(1);
                total_rounds.fetch_add(static_cast<long long>(survived));
            }
        };

        std::vector<std::thread> pool;
        for (unsigned t = 0; t < num_threads; ++t) pool.emplace_back(worker);
        for (auto &t : pool) t.join();

        double survive_rate = static_cast<double>(success.load()) / opt.instances;
        double avg = static_cast<double>(total_rounds.load()) /
                     (static_cast<double>(opt.instances) * opt.rounds);
        std::fprintf(out, "%llu,%.6f,%.6f,%d,%d\n",
                     static_cast<unsigned long long>(timeout), survive_rate, avg,
                     opt.instances, opt.rounds);
        std::fflush(out);
        if (!opt.quiet) {
            std::fprintf(stderr, "[pistis] T=%llud  Pr=%.4f  mean=%.4f\n",
                         static_cast<unsigned long long>(timeout), survive_rate, avg);
        }
    }

    if (out != stdout) std::fclose(out);
    return 0;
}
