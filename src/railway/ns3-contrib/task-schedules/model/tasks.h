#ifndef _TASKS_H_
#define _TASKS_H_

#include "ns3/core-module.h"

#include <assert.h>
#include <map>
#include <set>
#include <string>

#define INCOMING (1)
#define OUTGOING (-1)

namespace ns3
{

enum TasksEnum
{
    GOVERNING,
    TRAIN_SENSOR_COLLECTS,
    TRAIN_UPDATE_LOCATION,
    TRAIN_CALC_SPEED,
    TRAIN_RECV_MA,
    TRAIN_ACTUATION,
    STATION_RECV_LOCATION_SPEED,
    STATION_COMPUTE_MA,
    DUMMY,
};

enum TaskState
{
    INTER_RECV_REMOTE,
    INTER_RECV_CROSS_CHECK,
    INTER_OUT_PRIM_SEND_INPUT,
    INTER_OUT_CALC,
    INTER_OUT_CROSS_CHECK,

    INTRA_USTRM_PRIM_INIT,
    INTRA_USTRM_PRIM_SEND,
    INTRA_USTRM_BKUP_RECV_INPUT,
    INTRA_DSTRM_PRIM_RECV_OUTPUT,
    INTRA_DSTRM_BKUP_RECV_OUTPUT,
    INTRA_USTRM_BKUP_RECV_CHECK,
};

class Task
{
  public:
    int id;
    int period;
    int offset;
    int wcet;
    int ddl;
    int predecessor;
    int successor;
    int inter_region;
    int incoming;

    Task(int id,
         int period,
         int offset,
         int wcet,
         int ddl,
         int pred,
         int succ,
         int inter,
         int incoming)
        : id(id),
          period(period),
          offset(offset),
          wcet(wcet),
          ddl(ddl),
          predecessor(pred),
          successor(succ),
          inter_region(inter),
          incoming(incoming)
    {
        if (inter == -1)
            NS_ASSERT(incoming == 0);
        else
            NS_ASSERT(incoming == INCOMING || incoming == OUTGOING);
    }
};

typedef struct TaskWithState
{
    int task;
    TaskState state;

    TaskWithState(int task, TaskState state)
        : task(task),
          state(state)
    {
    }
} TaskWithState;

bool operator==(const TaskWithState& l, const TaskWithState& r);
bool operator<(const TaskWithState& l, const TaskWithState& r);

/**
 * @brief For inter-regional identification purpose.
 * Has a region id and a node id.
 */
typedef struct NodeInfo
{
    int region_id;
    int node_id;

    bool operator<(const struct NodeInfo& other) const
    {
        return (region_id < other.region_id) ||
               (region_id == other.region_id && node_id < other.node_id);
    }

    bool operator==(const struct NodeInfo& other) const
    {
        return region_id == other.region_id && node_id == other.node_id;
    }

    bool operator!=(const struct NodeInfo& other) const
    {
        return !(*this == other);
    }

    std::string to_string() const
    {
        return std::to_string(region_id) + "." + std::to_string(node_id);
    }

    // NodeInfo(int rid, int nid)
    //     : region_id(rid),
    //       node_id(nid)
    // {
    // }

    // NodeInfo()
    //     : region_id(0),
    //       node_id(0)
    // {
    // }
} NodeInfo;

enum PoMType
{
    COMMISSION,
    OMISSION,
};
}; // namespace ns3

#endif