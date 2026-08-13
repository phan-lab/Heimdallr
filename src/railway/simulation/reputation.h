#ifndef _REPUTATION_H_
#define _REPUTATION_H_

#include "ns3/log-helper.h"
#include "ns3/core-module.h"
#include "ns3/parse_sche.h"

#include "defs.h"
#include <map>
#include <set>

#define INTER_REGION_LATENCY_VAR (5000) //!< The variation. In microseconds
#define SCORE_MAX (1.0)
#define SCORE_INIT (1.0)
#define P_NORMAL (0.99)
#define SCORE_ALPHA (0.75)
#define SCORE_BETA (1)
#define SCORE_PENALTY (SCORE_MAX / SCORE_BETA)
#define SCORE_AWARD (SCORE_PENALTY * (1 - P_NORMAL) / SCORE_ALPHA / P_NORMAL)

namespace ns3
{
    /**
     * @brief This class stores the credit scores of all couriers
     * that work for the same task.
     */
    class CreditScores
    {
    public:
        CreditScores() = delete;
        CreditScores(NodeInfo owner,
                     int task,    
                     int remote_region,
                     int local_region,
                     Schedule *remote_sche,
                     Schedule *local_sche,
                     int64_t timeout);

        /**
         * @brief To log received packets and add credit scores to the
         * nodes correspondingly. See the description of `round_init()`
         * for the whole process.
         */
        void round_recv(const NodeInfo &remote, const NodeInfo &local);

    protected:
        int flag; /* to check if the round has begun */
        std::map<NodeInfo, double> scores;
        std::set<std::pair<NodeInfo, NodeInfo>> round_not_recvd;

        NodeInfo owner;
        int task;
        int remote_region, local_region;
        Schedule *remote_sche, *local_sche;
        int64_t timeout; //!< in microseconds

        /**
         * @brief Initialize a round. In each round, the node expects
         * to receive messages from all remote couriers. If so, the node
         * will call the `round_recv()` function to log that there is a
         * message received. When the round finishes, the node should call
         * `round_finish()` to check if there is any messages dropped. If
         * so, their credit scores will be decreased.
         *
         * @param courier_role Is the function called when the node acts as
         * a courier? It is also possible to be "false" even if the node is a
         * courier, but it's in the stage of crosschecking messages from other
         * couriers.
         */
        void round_init();

        void round_finish();
    };
}
#endif
