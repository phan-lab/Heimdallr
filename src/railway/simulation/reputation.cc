#include "reputation.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Reputation");

#define NODE_LOG_UNCOND(msg) NS_LOG_UNCOND(owner.to_string() << " at " << Simulator::Now().GetMilliSeconds() << "||| " << msg);
#define NODE_LOG_LOGIC(msg) NS_LOG_LOGIC(owner.to_string() << " at " << Simulator::Now().GetMilliSeconds() << "||| " << msg);
#define NODE_LOG_ERROR(msg) NS_LOG_ERROR(owner.to_string() << " at " << Simulator::Now().GetMilliSeconds() << "||| " << msg);
#define NODE_LOG_WARN(msg) NS_LOG_WARN(owner.to_string() << " at " << Simulator::Now().GetMilliSeconds() << "||| " << msg);

CreditScores::CreditScores(NodeInfo owner,
                           int task,
                           int remote_region,
                           int local_region,
                           Schedule *remote_sche,
                           Schedule *local_sche,
                           int64_t timeout)
    : flag(0),
      owner(owner),
      task(task),
      remote_region(remote_region),
      local_region(local_region),
      remote_sche(remote_sche),
      local_sche(local_sche),
      timeout(timeout)
{
    NodeInfo remote, local;
    remote.region_id = remote_region;
    local.region_id = local_region;

    for (int remote_courier : remote_sche->couriers)
    {
        remote.node_id = remote_courier;
        scores.insert(std::make_pair(remote, SCORE_INIT));
    }
    for (int local_courier : local_sche->couriers)
    {
        local.node_id = local_courier;
        scores.insert(std::make_pair(local, SCORE_INIT));
    }
}

void CreditScores::round_init()
{
    NODE_LOG_LOGIC("init for " << remote_region);
    Simulator::Schedule(MicroSeconds(timeout), &CreditScores::round_finish, this);
    if (flag == 1)
        return;
    flag = 1;

    /* add all pairs to the not recvd set */
    round_not_recvd.clear();
    NodeInfo remote, local;

    remote.region_id = remote_region;
    local.region_id = local_region;

    for (int remote_courier : remote_sche->couriers)
    {
        remote.node_id = remote_courier;
        for (int local_courier : local_sche->couriers)
        {
            local.node_id = local_courier;
            round_not_recvd.insert(std::make_pair(remote, local));
        }
    }
}

void CreditScores::round_recv(const NodeInfo &remote, const NodeInfo &local)
{
    if (flag == 0)
        round_init();
    auto pair = std::make_pair(remote, local);
    NODE_LOG_LOGIC("RECV remote, local = " << remote.to_string() << " " << local.to_string() << " task " << task);
    if (round_not_recvd.find(pair) == round_not_recvd.end())
    {
        NODE_LOG_ERROR("remote, local = " << remote.to_string() << " " << local.to_string() << " task " << task);
        NS_ASSERT(0);
    }
    round_not_recvd.erase(pair);
    scores[remote] += SCORE_AWARD;
    if (scores[remote] > SCORE_MAX)
        scores[remote] = SCORE_MAX;
    scores[local] += SCORE_AWARD;
    if (scores[local] > SCORE_MAX)
        scores[local] = SCORE_MAX;
}

void CreditScores::round_finish()
{
    NODE_LOG_LOGIC("finish for " << remote_region);
    NS_ASSERT(flag == 1);

    for (auto &pair : round_not_recvd)
    {
        scores[pair.first] -= SCORE_PENALTY;
        if (scores[pair.first] < 0)
        {
            const NodeInfo &node = pair.first;
            NODE_LOG_WARN("Task " << task << " WARNING: The score of " << node.to_string() << " is now " << scores[node]);
        }
        scores[pair.second] -= SCORE_PENALTY;
        if (scores[pair.second] < 0)
        {
            const NodeInfo &node = pair.second;
            NODE_LOG_WARN("Task " << task << " WARNING: The score of " << node.to_string() << " is now " << scores[node]);
        }
        NODE_LOG_WARN("Task " << task << " The scores of" << pair.first.to_string() << " and " << pair.second.to_string()
                             << " are decreased to " << scores[pair.first]
                             << " and " << scores[pair.second]);
        // NS_ASSERT(0);
    }

    flag = 0;
}