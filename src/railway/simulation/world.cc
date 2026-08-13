#include "world.h"

#include <cassert>
#include <fstream>
#include <set>
#include <sstream>

using namespace ns3;
#define NS3_LOG_ENABLE 1
NS_LOG_COMPONENT_DEFINE("World");

const int csma_interface = 1;
const int lte_interface = 2;

World::World()
{
    return;
}

World::~World()
{
    return;
}

int World::get_rid_by_gnid(int gnid)
{
    int offset = 0;
    for (auto ent : this->num_node_per_region)
    {
        offset += ent.second;
        if (gnid < offset)
            return ent.first;
    }
    NS_ASSERT(0);
    return -1;
}

int World::get_nid_by_gnid(int gnid)
{
    int nid = gnid;
    for (auto ent : this->num_node_per_region)
    {
        if (nid < ent.second)
            return nid;
        else
            nid -= ent.second;
    }
    return nid;
}

int World::get_gnid(int rid, int nid)
{
    int gnid = nid;
    for (auto ent : this->num_node_per_region)
    {
        if (ent.first < rid)
            gnid += ent.second;
    }
    return gnid;
}

void World::read_config(const std::string &inter_region_filename)
{
    /* Parse the config files */
    std::ifstream ifs(inter_region_filename, std::ifstream::in);
    if (!ifs.good())
    {
        NS_LOG_UNCOND("Fail to open " << inter_region_filename << ": " << strerror(errno));
        NS_ASSERT(0);
    }
    int num_lines = 0;
    int num_regions = 0;

    int total_nodes = 0;

    /* Set up LAN */
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("1Gbps"));
    csma.SetChannelAttribute("Delay",
                             StringValue(std::to_string(INTRA_REGION_LATENCY) + "us"));

    internetStack.SetRoutingHelper(staticRoutingHelper);
    Ipv4AddressHelper address_csma;
    address_csma.SetBase("123.1.0.0", "255.255.0.0");
    while (!ifs.eof())
    {
        std::string line = "";
        std::getline(ifs, line);
        if (line.size() == 0 || line[0] == '#')
            continue;
        if (num_lines == 0)
        {
            /* The first line is the number of regions */
            num_regions = atoi(line.c_str());
        }
        else if (num_lines > 0 && num_lines <= num_regions)
        {
            /* Read the config of a specific region */
            int region_id = num_lines - 1;
            std::stringstream ss(line);
            std::string file, type_str;
            ss >> file >> type_str;

            NodeContainer nc_csma;

            if (type_str == "station")
            {
                this->num_node_per_region[region_id] = NODE_PER_STATION;
                total_nodes += NODE_PER_STATION;

                for (int i = 0; i < NODE_PER_STATION; i++)
                {
                    Ptr<Station> station = CreateObject<Station>(region_id, i);

                    /* Install application */
                    auto app = CreateObject<StationApp>(region_id, i);
                    app->max_train_id = num_regions - 1;
                    this->apps.emplace_back(app);
                    this->station_apps.emplace_back(app);
                    station->AddApplication(app);

                    nodeContainer.Add(station);
                    nc_csma.Add(station);
                    station_nc.Add(station);
                    all_nodes.emplace_back(station);
                }
            }
            else
            {
                this->num_node_per_region[region_id] = NODE_PER_TRAIN;
                total_nodes += NODE_PER_TRAIN;

                for (int i = 0; i < NODE_PER_TRAIN; i++)
                {
                    Ptr<Train> train = CreateObject<Train>(region_id, i);
                    /* Install application */
                    auto app = CreateObject<TrainApp>(region_id, i);
                    this->apps.emplace_back(app);
                    this->train_apps.emplace_back(app);
                    train->AddApplication(app);

                    nodeContainer.Add(train);
                    nc_csma.Add(train);
                    train_nc.Add(train);
                    all_nodes.emplace_back(train);
                }
            }

            NetDeviceContainer csmaDevices = csma.Install(nc_csma);
            internetStack.Install(nc_csma);
            address_csma.Assign(csmaDevices);
            // if (num_lines > TRAIN_NODE)
        }
        num_lines++;
    }
    csma.EnablePcapAll("pcaps/csma");
    std::map<NodeInfo, Ipv4Address> id2ip_map;
    std::map<Ipv4Address, NodeInfo> ip2id_map;

    for (int i = 0; i < total_nodes; i++)
    {
        Ptr<Node> node = nodeContainer.Get(i);
        Ipv4Address addr = node->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
        apps[i]->setMainAddress(addr);

        int rid = get_rid_by_gnid(i);
        int nid = get_nid_by_gnid(i);
        NS_ASSERT(get_gnid(rid, nid) == i);

        // NS_LOG_UNCOND(rid << "." << nid << "  " << i);

        NodeInfo node_info;
        node_info.region_id = rid;
        node_info.node_id = nid;

        id2ip_map.insert(std::make_pair(node_info, addr));
        ip2id_map.insert(std::make_pair(addr, node_info));
    }

    /* Install the (address <-> id) mapping */
    for (int i = 0; i < total_nodes; i++)
    {
        apps[i]->setLanMapping(id2ip_map, ip2id_map);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    NS_LOG_UNCOND("train_nc: " << train_nc.GetN());
    NS_LOG_UNCOND("station_nc: " << station_nc.GetN());
    NS_LOG_UNCOND("total_nc: " << nodeContainer.GetN());
}

void World::setup_lte()
{
    std::map<NodeInfo, Ipv4Address> id2ip_map;
    std::map<Ipv4Address, NodeInfo> ip2id_map;

    /* Basic LTE */

    // Config::SetDefault("ns3::LteHelper::UseCa", BooleanValue(true));
    // Config::SetDefault("ns3::LteHelper::NumberOfComponentCarriers", UintegerValue(5));
    // Config::SetDefault("ns3::LteHelper::EnbComponentCarrierManager",
    //                    StringValue("ns3::RrComponentCarrierManager"));

    lteHelper = CreateObject<LteHelper>();
    epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

    epcHelper->SetAttribute("S1uLinkPcapPrefix", StringValue("pcaps/s1-u"));
    // epcHelper->SetAttribute("X2LinkPcapPrefix", StringValue("pcaps/x2"));
    // epcHelper->SetAttribute("X2LinkEnablePcap", BooleanValue(true));
    epcHelper->SetAttribute("S1uLinkEnablePcap", BooleanValue(true));

    pgw = epcHelper->GetPgwNode();

    // Create the internet
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

    /* Each node in the station is a remote host */
    NetDeviceContainer internetDevices[station_nc.GetN()];
    for (unsigned int i = 0; i < station_nc.GetN(); i++)
    {
        internetDevices[i] = p2ph.Install(pgw, station_nc.Get(i));
    }

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (unsigned int i = 0; i < station_nc.GetN(); i++)
    {
        Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices[i]);
        // interface 0 is localhost, 1 is the p2p device
        Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);
        NS_LOG_UNCOND(i << " " << remoteHostAddr << " " << internetIpIfaces.GetAddress(0));

        station_apps[i]->setWirelessAddress(remoteHostAddr);
        /* add to map entry */
        id2ip_map.insert(std::make_pair(station_apps[i]->get_node_info(), remoteHostAddr));
        ip2id_map.insert(std::make_pair(remoteHostAddr, station_apps[i]->get_node_info()));

        Ipv4StaticRoutingHelper ipv4RoutingHelper;
        Ptr<Ipv4StaticRouting> remoteHostStaticRouting;
        remoteHostStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(station_nc.Get(i)->GetObject<Ipv4>());
        remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"),
                                                   Ipv4Mask("255.0.0.0"),
                                                   lte_interface);
        // remoteHostStaticRouting->PrintRoutingTable(Create<OutputStreamWrapper>(&std::cout));

        ipv4RoutingHelper.GetStaticRouting(pgw->GetObject<Ipv4>())
            ->AddNetworkRouteTo(remoteHostAddr, Ipv4Mask("255.255.255.255"), 3 + i);
    }

    // NodeContainer enbNodes;
    enbNodes.Create(NUM_ENBS);

    MobilityHelper mobility_enb;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (int i = 0; i < NUM_ENBS; i++)
    {
        positionAlloc->Add(Vector(i * DISTANCE_ENBS + DISTANCE_ENBS / 2, 0, 0));
    }

    mobility_enb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility_enb.SetPositionAllocator(positionAlloc);
    mobility_enb.Install(enbNodes);

    MobilityHelper mobility_train;
    mobility_train.SetMobilityModel("ns3::ConstantAccelerationMobilityModel");
    mobility_train.Install(train_nc);

    enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    ueLteDevs = lteHelper->InstallUeDevice(train_nc);

    // assign IP address to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    for (uint32_t u = 0; u < train_nc.GetN(); ++u)
    {
        Ptr<Node> ue = train_nc.Get(u);
        Ipv4Address ueAddr = ueIpIface.GetAddress(u);
        NS_LOG_UNCOND(u << " " << ueAddr);
        train_apps[u]->setWirelessAddress(ueAddr);

        /* add to map entry */
        id2ip_map.insert(std::make_pair(train_apps[u]->get_node_info(), ueAddr));
        ip2id_map.insert(std::make_pair(ueAddr, train_apps[u]->get_node_info()));

        // set the default gateway for the UE
        NS_LOG_UNCOND("Default gateway: " << epcHelper->GetUeDefaultGatewayAddress());
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), lte_interface);
        // ueStaticRouting->PrintRoutingTable(Create<OutputStreamWrapper>(&std::cout));
    }

    // ipv4RoutingHelper.GetStaticRouting(enbNodes.Get(0)->GetObject<Ipv4>())
    //     ->PrintRoutingTable(Create<OutputStreamWrapper>(&std::cout));

    // ipv4RoutingHelper.GetStaticRouting(pgw->GetObject<Ipv4>())
    //     ->PrintRoutingTable(Create<OutputStreamWrapper>(&std::cout));

    for (unsigned int u = 0; u < train_nc.GetN(); u++)
        lteHelper->Attach(ueLteDevs.Get(u), enbLteDevs.Get((u / NODE_PER_TRAIN + 1) / 5));

    for (size_t i = 0; i < apps.size(); i++)
    {
        apps[i]->setLteMapping(id2ip_map, ip2id_map);
    }

    lteHelper->AddX2Interface(enbNodes);

    NS_LOG_UNCOND("LTE setup successful");
}

void World::init_crypto()
{
    MSObj_init();
    std::map<NodeInfo, MultiSigObj *> pk_map;
    for (size_t i = 0; i < apps.size(); i++)
    {
        MultiSigObj pk, sk;
        MSObj_init_pub(pk);
        MSObj_init_sec(sk);
        MSObj_gen_keypair(pk, sk);
        apps[i]->installKeys(pk, sk);

        pk_map.insert(std::make_pair(apps[i]->get_node_info(), &apps[i]->public_key));
    }

    for (size_t i = 0; i < apps.size(); i++)
    {
        apps[i]->installKeyMap(pk_map);
    }
}

void World::handover_check()
{
    Simulator::Schedule(MilliSeconds(ACTUATE_FREQ), &World::handover_check, this);

    /* Sync the positions for nodes on the same train */
    double speed = 0, pos = 0, acc = 0;
    for (size_t i = 0; i < train_apps.size(); i++)
    {
        auto app = train_apps.at(i);
        if (app->get_node_info().node_id == 0)
        {
            speed = app->get_speed();
            pos = app->get_position();
            acc = app->get_acc();
        }
        else
        {
            app->setMobilityInfo(speed, acc, pos);
        }
    }

    const int track_block = 100;
    const int total_blocks = 50;
    int track_occupied[total_blocks] = {0};
    std::cerr << Simulator::Now().GetMilliSeconds() << "ms\t";
    for (size_t i = 0; i < train_apps.size(); i++)
    {
        auto app = train_apps.at(i);
        double pos = app->get_position();
        if (app->cur_enb != FIND_CLOSEST_ENB(pos))
        {
            /* Handover required */
            NS_LOG_UNCOND("Train node " << app->get_node_info().to_string()
                                        << " starting handover from " << app->cur_enb << " to "
                                        << FIND_CLOSEST_ENB(pos));
            NS_LOG_UNCOND("Total enbLteDevices " << enbLteDevs.GetN());
            lteHelper->HandoverRequest(Time(0),
                                       ueLteDevs.Get(i),
                                       enbLteDevs.Get(app->cur_enb),
                                       enbLteDevs.Get(FIND_CLOSEST_ENB(pos)));
            app->cur_enb = FIND_CLOSEST_ENB(pos);
        }
        if (app->get_node_info().node_id == 0)
        {
            // NS_LOG_UNCOND(Simulator::Now().GetMilliSeconds()
            //               << " " << app->get_node_info().region_id
            //               << " Position " << pos << " Speed "
            //               << app->get_speed());
            track_occupied[((int)pos % (track_block * total_blocks)) / track_block] = app->get_node_info().region_id;
        }
    }
    for (int i = 0; i < total_blocks; i++)
    {
        std::cerr << track_occupied[i];
    }
    std::cerr << "\r";
}

void World::init_schedule(const std::string &task_file)
{
    read_schedules(schedules, tasks, task_file, mode_change_map);
    for (size_t i = 0; i < apps.size(); i++)
    {
        apps[i]->installTaskSchedules(schedules, tasks, mode_change_map);
    }
}

void World::start_simulation()
{
    setup_lte();
    init_crypto();
    Simulator::Stop(Seconds(END_TIME));
    Simulator::Schedule(MilliSeconds(2000), &World::handover_check, this);
    Simulator::Run();
    Simulator::Destroy();
}