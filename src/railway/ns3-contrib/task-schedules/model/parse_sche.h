#ifndef _PARSE_SCHE_H_
#define _PARSE_SCHE_H_

#include "tasks.h"

#include <map>
#include <set>

void set_prefix(const std::string& pre);

namespace ns3
{
    typedef struct
    {
        int primary;
        std::set<int> couriers;
        std::set<int> workers;
    } Schedule;

    typedef struct KnownFault
    {
        PoMType type;

        union
        {
            struct
            {
                NodeInfo node1;
                NodeInfo node2;
            } link;

            NodeInfo node;
        } u;
    } KnownFault;

    bool operator<(const KnownFault &l, const KnownFault &r);
    bool operator==(const KnownFault &l, const KnownFault &r);

    typedef struct TaskInRegion
    {
        int task;
        int region;
        TaskInRegion() = delete;

        TaskInRegion(int t, int r)
            : task(t),
              region(r)
        {
        }
    } TaskInRegion;

    bool operator<(const TaskInRegion &l, const TaskInRegion &r);
    bool operator==(const TaskInRegion &l, const TaskInRegion &r);

    void read_sche_per_region(std::map<TaskInRegion, Schedule> &schedules,
                              int region,
                              const std::string &mode_file,
                              std::map<KnownFault, std::string> &mode_change_map);

    /**
     * @brief read the schedule from file(s)
     * @param schedules where to store the tasks
     * @param task_file The file name of where info of tasks is stored
     */
    void read_schedules(std::map<TaskInRegion, Schedule> &schedules,
                        std::map<int, Task> &tasks,
                        const std::string &task_file,
                        std::map<KnownFault, std::string> &mode_change_map);
}; // namespace ns3
#endif