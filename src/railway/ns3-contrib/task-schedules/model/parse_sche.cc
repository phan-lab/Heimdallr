#include "parse_sche.h"

#include "ns3/core-module.h"

#include <assert.h>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

static std::string prefix = "./";

void set_prefix(const std::string& pre)
{
    prefix = pre + "/";
}


namespace ns3
{
bool
operator<(const TaskInRegion& l, const TaskInRegion& r)
{
    return l.task < r.task || (l.task == r.task && l.region < r.region);
}

bool
operator==(const TaskInRegion& l, const TaskInRegion& r)
{
    return l.task == r.task && l.region == r.region;
}

bool
operator<(const KnownFault& l, const KnownFault& r)
{
    if (l.type < r.type)
        return true;
    if (l.type > r.type)
        return false;
    if (l.type == PoMType::COMMISSION)
        return l.u.node < r.u.node;
    else
        return (l.u.link.node1 < r.u.link.node1 ||
                (l.u.link.node1 == r.u.link.node1 && l.u.link.node2 < r.u.link.node2));
}

bool
operator==(const KnownFault& l, const KnownFault& r)
{
    if (l.type != r.type)
        return false;
    if (l.type == PoMType::COMMISSION)
    {
        return l.u.node == r.u.node;
    }
    else
    {
        return l.u.link.node1 == r.u.link.node1 && l.u.link.node2 == r.u.link.node2;
    }
}

static void
getline_ignore_comments(std::ifstream& ifs, string& line)
{
    while (!ifs.eof())
    {
        getline(ifs, line);
        if (line.size() == 0 || line[0] == '#')
            continue;
        else
            return;
    }
}

void read_tasks_per_region(std::map<TaskInRegion, Schedule>& schedules,
                           int region,
                           std::map<int, Task>& tasks,
                           const std::string& region_task_file,
                           std::map<KnownFault, std::string>& mode_change_map);

void
read_schedules(std::map<TaskInRegion, Schedule>& schedules,
               std::map<int, Task>& tasks,
               const std::string& task_file,
               std::map<KnownFault, std::string>& mode_change_map)
{
    /* otherwise too many opened files at the same time, will panic */
    ifstream ifs(task_file, ifstream::in);
    if (!ifs.good())
    {
        NS_LOG_UNCOND("Fail to open" << strerror(errno));
        NS_ASSERT(0);
    }
    while (!ifs.eof())
    {
        string line;
        int region;
        string region_tasks_file;
        getline(ifs, line);
        if (line.size() == 0 || line[0] == '#')
            continue;
        stringstream ss(line);
        ss >> region >> region_tasks_file;
        region_tasks_file = prefix + region_tasks_file;
        read_tasks_per_region(schedules, region, tasks, region_tasks_file, mode_change_map);
    }
    ifs.close();
}

void
read_tasks_per_region(std::map<TaskInRegion, Schedule>& schedules,
                      int region,
                      std::map<int, Task>& tasks,
                      const std::string& region_task_file,
                      std::map<KnownFault, std::string>& mode_change_map)
{
    ifstream ifs;

    ifs.open(region_task_file, ifstream::in);

    if (!ifs.good())
    {
        NS_LOG_UNCOND("Fail to open " << region_task_file << ": " << strerror(errno));
        NS_ASSERT(0);
    }
    /* read all tasks */
    int lines = 0;

    string mode_0_file;
    while (!ifs.eof())
    {
        string line;
        getline_ignore_comments(ifs, line);
        if (lines == 0)
        {
            /* The first line should be the file name */
            mode_0_file = prefix + line;
            lines++;
            continue;
        }
        int id, period, offset, wcet, ddl, pred, succ, inter, incoming;
        string desc;
        stringstream ss(line);
        ss >> id >> desc >> period >> offset >> wcet >> ddl >> pred >> succ >> inter >> incoming;
        Task task(id, period, offset, wcet, ddl, pred, succ, inter, incoming);
        tasks.insert(make_pair(id, task));
    }
    ifs.close();

    read_sche_per_region(schedules, region, mode_0_file, mode_change_map);
}

void
read_sche_per_region(std::map<TaskInRegion, Schedule>& schedules,
                     int region,
                     const std::string& mode_file,
                     std::map<KnownFault, std::string>& mode_change_map)
{
    ifstream ifs(mode_file, ifstream::in);
    if (!ifs.good())
    {
        NS_LOG_UNCOND("Fail to open" << strerror(errno));
        NS_ASSERT(0);
    }

    while (!ifs.eof())
    {
        Schedule sche;
        string line_id_prim;
        string line_workers;
        string line_couriers;
        getline_ignore_comments(ifs, line_id_prim);
        /* end of schedule */
        if (line_id_prim == "." or line_id_prim.size() == 0)
            break;
        getline_ignore_comments(ifs, line_workers);
        getline_ignore_comments(ifs, line_couriers);
        // LOG_DEBUG("%s", line_id_prim.c_str());
        // LOG_DEBUG("%s", line_workers.c_str());
        // LOG_DEBUG("%s", line_couriers.c_str());
        stringstream ss_id_prim(line_id_prim);
        int id;
        ss_id_prim >> id >> sche.primary;
        stringstream ss_workers(line_workers);
        while (!ss_workers.eof())
        {
            int worker;
            ss_workers >> worker;
            sche.workers.insert(worker);
        }
        stringstream ss_couriers(line_couriers);
        while (!ss_couriers.eof())
        {
            int cour;
            ss_couriers >> cour;
            if (cour == -1)
                break;
            sche.couriers.insert(cour);
        }
        /* Finish parsing 3 lines, which are for 1 task */
        if (schedules.find(TaskInRegion(id, region)) != schedules.end())
            schedules[TaskInRegion(id, region)] = sche;
        else
            schedules.insert(make_pair(TaskInRegion(id, region), sche));
    }
    while (!ifs.eof())
    {
        string line;
        getline_ignore_comments(ifs, line);
        if (line == "")
            break;
        stringstream ss(line);
        KnownFault kf;
        string new_mode_file;
        int type;
        ss >> type;
        kf.type = (PoMType)type;
        if (kf.type == COMMISSION)
        {
            NodeInfo faulty_node;
            faulty_node.region_id = region;
            ss >> faulty_node.node_id;
            ss >> new_mode_file;
            kf.u.node = faulty_node;
            if (mode_change_map.find(kf) == mode_change_map.end())
                mode_change_map.insert(std::make_pair(kf, new_mode_file));
            else
                mode_change_map[kf] = new_mode_file;
        }
        else
        {
            NodeInfo node1, node2;
            node1.region_id = region;
            node2.region_id = region;
            ss >> node1.node_id >> node2.node_id;
            ss >> new_mode_file;
            kf.u.link.node1 = node1;
            kf.u.link.node1 = node2;
            if (mode_change_map.find(kf) == mode_change_map.end())
                mode_change_map.insert(std::make_pair(kf, new_mode_file));
            else
                mode_change_map[kf] = new_mode_file;
        }
    }
    ifs.close();
}
}; // namespace ns3
