#ifndef _WORLD_H_
#define _WORLD_H_

#include "defs.h"
#include "general_app.h"
#include "station.h"
#include "train.h"

#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-helper.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-generator.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/lte-helper.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/task-schedules.h"

#include <array>
#include <vector>

namespace ns3
{
/**
 * @brief Simulate the time, network, and the location of trains
 */
class World
{
  public:
    World();
    ~World();

    void read_config(const std::string& inter_region_filename);
    void init_schedule(const std::string& task_file);
    void start_simulation();

    //!< True for occupied and false for unoccupied
    // std::array<int, NUM_BLOCKS> track_occupancy;

  private:
    /**
     * @brief Get the region id by the global node id
     */
    int get_rid_by_gnid(int gnid);
    /**
     * @brief Get the local node id by the global node id
     */
    int get_nid_by_gnid(int gnid);
    /**
     * @brief Get the global node id by region and local node id
     */
    int get_gnid(int rid, int nid);
    void init_crypto();

    /* Nodes */
    std::vector<Ptr<CNode>> all_nodes;
    std::vector<Ptr<Train>> trains;
    std::vector<Ptr<Station>> stations;

    std::map<int, int> num_node_per_region;
    std::vector<Ptr<GeneralApp>> apps;
    std::vector<Ptr<StationApp>> station_apps;
    std::vector<Ptr<TrainApp>> train_apps;
    NodeContainer nodeContainer;
    NodeContainer station_nc, train_nc;
    NodeContainer enbNodeContainer;

    /* Below for LTE use */
    void setup_lte();
    void handover_check();
    InternetStackHelper internetStack;
    Ipv4StaticRoutingHelper staticRoutingHelper;

    Ptr<Node> pgw;
    Ptr<Node> remoteHost;
    NodeContainer ueNodes;
    NodeContainer enbNodes;

    Ptr<LteHelper> lteHelper;
    Ptr<PointToPointEpcHelper> epcHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting;
    Ptr<ListPositionAllocator> positionAlloc;
    Ptr<EpcTft> tft;

    NetDeviceContainer enbLteDevs;
    NetDeviceContainer ueLteDevs;

    /* For task schedules */


    std::map<TaskInRegion, Schedule> schedules;
    std::map<int, Task> tasks;
    const std::string task_file;
    std::map<KnownFault, std::string> mode_change_map;
};
}; // namespace ns3

#endif