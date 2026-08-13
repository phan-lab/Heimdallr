#include "station_app.h"

#include "ns3/core-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StationApp");

void StationApp::StartApplication()
{
    NS_LOG_LOGIC("StationApp Start");
#if USE_TCP
    TypeId tid_lte = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_lte_socket = Socket::CreateSocket(GetNode(), tid_lte);
    InetSocketAddress local_wireless = InetSocketAddress(m_wirelessAddr, LTE_PORT);
    m_lte_socket->Bind(local_wireless);
    m_lte_socket->Listen();
    NS_LOG_LOGIC(node_info.to_string() << ": Listening");
    m_lte_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                                    MakeCallback(&StationApp::ProcessAccept, this));
    m_lte_socket->SetCloseCallbacks(MakeCallback(&StationApp::HandlePeerClose, this),
                                    MakeCallback(&StationApp::HandlePeerError, this));
#endif

    for (int i = 1 + STATION_RID; i <= max_train_id; i++)
    {
        if (is_worker(GOVERNING))
            Simulator::Schedule(MicroSeconds(PING_OFFSET_BY_ID(i) +
                                             CONNECTION_SETUP_OFFSET + PING_INTERVAL - INTRA_REGION_LATENCY + SCHEDULE_VAR),
                                &GeneralApp::SendPingPrepare,
                                this, i);
        Simulator::Schedule(MicroSeconds(PING_OFFSET_BY_ID(i) +
                                         CONNECTION_SETUP_OFFSET + PING_INTERVAL + PING_TIMEOUT),
                            &GeneralApp::SendPingAccept,
                            this, i);
    }

    /* schedule all application-level tasks */

    for (const auto &sche : schedules)
    {
        if (sche.first.region != node_info.region_id)
            continue;
        if (!is_worker(sche.first.task))
            continue;
        switch (sche.first.task % MAX_TRAINS)
        {
        case TasksEnum::STATION_COMPUTE_MA:
        {
            auto offset = MilliSeconds(
                PING_OFFSET_BY_ID(sche.first.task / MAX_TRAINS) / 1000 +
                CONNECTION_SETUP_OFFSET / 1000 +
                tasks.at(sche.first.task).offset);
            Simulator::Schedule(
                offset, &StationApp::ComputeMA, this, sche.first.task / MAX_TRAINS);
        }
        break;

        default:
            break;
        }
    }

#if REPUTATION_SYSTEM
    /* Setup the reputation system for PING */
    auto &local_sche = schedules.at({GOVERNING, node_info.region_id});

    for (int i = 1 + STATION_RID; i <= max_train_id; i++)
    {
        auto &remote_sche = schedules.at({GOVERNING, i});
        scores.emplace(std::make_pair(
            i * MAX_TRAINS + GOVERNING,
            CreditScores(this->node_info, GOVERNING, i,
                         this->node_info.region_id,
                         &remote_sche,
                         &local_sche,
                         PING_TIMEOUT)));
    }
#endif
    GeneralApp::StartApplication();
}

void StationApp::ProcessLanMsg(Ptr<Socket> socket)
{
    Address sourceAddr;
    Ptr<Packet> packet = socket->RecvFrom(sourceAddr);
    InetSocketAddress inetSocketAddr = InetSocketAddress::ConvertFrom(sourceAddr);
    Ipv4Address sourceAddress = inetSocketAddr.GetIpv4();
    // uint16_t sourcePort = inetSocketAddr.GetPort();
    NodeInfo source_node = m_lan_ip2id_map.at(sourceAddress);
    NS_LOG_LOGIC(node_info.to_string() << " " << Simulator::Now().GetSeconds()
                                       << " LAN Message from " << source_node.to_string() << "\n");

    GeneralMessage msg;
    packet->RemoveHeader(msg);
    ProcessMessage(msg, source_node);
}

void StationApp::ProcessWirelessMsg(Ptr<Socket> socket)
{
    Address sourceAddr;
    Ptr<Packet> packet = socket->RecvFrom(sourceAddr);
    InetSocketAddress inetSocketAddr = InetSocketAddress::ConvertFrom(sourceAddr);
    Ipv4Address sourceAddress = inetSocketAddr.GetIpv4();
    // uint16_t sourcePort = inetSocketAddr.GetPort();
    NodeInfo source_node = m_lte_ip2id_map.at(sourceAddress);
    NS_LOG_LOGIC(node_info.to_string()
                 << " " << Simulator::Now().GetSeconds() << " Wireless Message from "
                 << source_node.to_string() << "\n");

    GeneralMessage msg;
    packet->RemoveHeader(msg);
    ProcessMessage(msg, source_node);
}

void StationApp::ProcessMessage(const GeneralMessage &msg, const NodeInfo &src)
{
    // msg.Print(std::cout);
    int type = msg.get_type();
    switch (type)
    {
    case MsgType::PING_PREPARE:
        this->ProcessPingPrepare(msg);
        break;
    case MsgType::PING_INTER:
        this->ProcessPingInter(msg, src);
        break;
    case MsgType::PING_PROPOSE:
        this->ProcessPingPropose(msg, src);
        break;
    case MsgType::PING_ACCEPT:
        this->ProcessPingAccept(msg, src);
        break;
    case MsgType::CONTROL_UPDATE_SPEED_TO_STATION:
        this->ProcessControlUpdate(msg, src);
        break;
    case MsgType::STATION_SEND_TRAIN_STATE_TO_PEERS:
        this->ProcessControlUpdatePeers(msg, src);
        break;
    case MsgType::POC:
        this->ProcessPoC(msg, src);
        break;
    default:
        NS_LOG_ERROR("Unknown type " << type);
        NS_ASSERT(0);
        break;
    }
}

#if USE_TCP
void StationApp::ProcessAccept(Ptr<Socket> socket, const Address &address)
{
    socket->SetRecvCallback(MakeCallback(&GeneralApp::ProcessWirelessMsg, this));
    NodeInfo client_info = m_lte_ip2id_map.at(InetSocketAddress::ConvertFrom(address).GetIpv4());
    m_connections.insert(std::make_pair(client_info, socket));
    NODE_LOG_UNCOND("Accept from " << client_info.to_string());
}

void StationApp::HandlePeerClose(Ptr<Socket> socket)
{
    NS_LOG_WARN("Peer close " << this << socket);
}

void StationApp::HandlePeerError(Ptr<Socket> socket)
{
    NS_LOG_ERROR("Peer error " << this << socket);
}

void StationApp::SendLteMessage(const NodeInfo &dest, const GeneralMessage &msg)
{
    // NS_ASSERT(0);
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(msg);
    // NODE_LOG_UNCOND(packet->GetSize());
    this->m_connections.at(dest)->Send(packet);
}
#endif

/**
 * @note Invoked when the couriers receive the update from trains.
 * The couriers need to:
 * 1. send the information to backups (if not done before)
 * 2. store the info locally (if not done before)
 * 3. switch to a state for waiting PoC (if not done before)
 *
 */
void StationApp::ProcessControlUpdate(const GeneralMessage &msg,
                                      const NodeInfo &src)
{
    int64_t cur_time = Simulator::Now().GetMicroSeconds();
    int which_train = src.region_id;
    int task_id = STATION_RECV_LOCATION_SPEED + MAX_TRAINS * which_train;

    /* You cannot receive a message that is sent in the future */
    if (msg.get_seq_num() > cur_time)
    {
        NODE_LOG_ERROR(msg.get_seq_num() << " " << cur_time);
        NS_ASSERT(0);
    }

#if REPUTATION_SYSTEM
    // NODE_LOG_UNCOND("update by remote");
    scores.at(task_id).round_recv(src, node_info);
#endif
    TaskInRegion tir(task_id, node_info.region_id);
    TaskInRegion train_tir(TRAIN_UPDATE_LOCATION, which_train);


    double location_and_speed[2];

    memcpy(location_and_speed, msg.get_constant_content_buf(),
           sizeof(location_and_speed));
    NS_ASSERT(sizeof(location_and_speed) == msg.get_size());

    train_locations[which_train] = location_and_speed[0];
    train_speed[which_train] = location_and_speed[1];
    std::array<uint8_t, MULTISIG_SIG_LENGTH> sig_arr;
    memcpy(sig_arr.data(), msg.get_constant_sig_buf(), MULTISIG_SIG_LENGTH);
    train_sigs[which_train] = sig_arr;

    LocationSpeedPeer lsp(src,
                          location_and_speed[0],
                          location_and_speed[1],
                          msg.get_constant_sig_buf());

    GeneralMessage msg_to_peers(MsgType::STATION_SEND_TRAIN_STATE_TO_PEERS,
                                msg.get_seq_num(),
                                0,
                                nullptr,
                                &secret_key, &public_key);
    msg_to_peers.set_size(lsp.serialize(msg_to_peers.get_content_buf()));

    for (int dest : schedules.at(tir).workers)
    {
        if (dest != node_info.node_id)
            SendLanMessage({node_info.region_id, dest}, msg_to_peers);
    }

    /* Schedule the timeout for PoC check */
    int64_t timeout = msg.get_seq_num() +
                      tasks.at(task_id).period * MILLI_TO_MICRO - cur_time;
    Simulator::Schedule(MicroSeconds(timeout), &GeneralApp::CheckForPoC,
                        this, train_tir, msg.get_seq_num());

    scheduled_check[train_tir] = msg.get_seq_num();
}

/**
 * @note Invoked when the couriers receive the update from trains.
 * The couriers need to:
 * 1. send the information to backups (if not done before)
 * 2. store the info locally (if not done before)
 * 3. switch to a state for waiting PoC (if not done before)
 */
void StationApp::ProcessControlUpdatePeers(const GeneralMessage &msg,
                                           const NodeInfo &src)
{
    int64_t cur_time = Simulator::Now().GetMicroSeconds();
    LocationSpeedPeer lsp(msg.get_constant_content_buf());
    int task_id = STATION_RECV_LOCATION_SPEED + MAX_TRAINS * lsp.train_node.region_id;

    /* You cannot receive a message that is sent in the future */
    if (msg.get_seq_num() > cur_time)
        NS_ASSERT(0);
#if REPUTATION_SYSTEM
    // NODE_LOG_UNCOND("update by local");
    scores.at(task_id).round_recv(lsp.train_node, src);
#endif
    TaskInRegion tir(task_id, node_info.region_id);
    TaskInRegion train_tir(TRAIN_UPDATE_LOCATION, lsp.train_node.region_id);
    // if (scheduled_check.find(train_tir) != scheduled_check.end() &&
    //     scheduled_check.at(train_tir) >= msg.get_seq_num())
    // {
    //     /* already processed. */
    //     return;
    // }
    train_locations[lsp.train_node.region_id] = lsp.location;
    train_speed[lsp.train_node.region_id] = lsp.speed;
    std::array<uint8_t, MULTISIG_SIG_LENGTH> sig_arr;
    memcpy(sig_arr.data(), lsp.signature, MULTISIG_SIG_LENGTH);
    train_sigs[lsp.train_node.region_id] = sig_arr;
}

/**
 * @brief Find the train that is directly in the front.
 *          Output its speed and location.
 * @note
 */
void StationApp::ComputeMA(int train_id)
{
    // NODE_LOG_UNCOND("Compute MA " << train_id);
    int station_task_id = TasksEnum::STATION_COMPUTE_MA +
                          MAX_TRAINS * train_id;
    int period = MILLI_TO_MICRO * tasks.at(station_task_id).period;
    int offset = MILLI_TO_MICRO * tasks.at(station_task_id).offset + PING_OFFSET_BY_ID(train_id);
    int64_t cur_time = Simulator::Now().GetMicroSeconds();
    Simulator::Schedule(MicroSeconds(period),
                        &StationApp::ComputeMA, this, train_id);

    const auto &station_sche = schedules.at(TaskInRegion(
        station_task_id, node_info.region_id));
    const auto &train_sche = schedules.at(TaskInRegion(
        TasksEnum::TRAIN_RECV_MA, train_id));

    double target_loc = train_locations.at(train_id);
    double closest_loc = -1;
    double closest_id = -1;
    for (const auto &ent : train_locations)
    {
        if (ent.first == train_id)
            continue;

        if ((closest_loc < 0 && ent.second > target_loc) ||
            (ent.second < closest_loc && ent.second > target_loc))
        {
            closest_loc = ent.second;
            closest_id = ent.first;
        }
    }
    if (closest_loc <= target_loc)
    {
        closest_id = -1;
        closest_loc = -1;
    }
    double closet_speed = closest_id == -1
                              ? MAX_SPEED
                              : train_speed.at(closest_id);

    MoveAuthMsg mam(train_id, closest_loc, closet_speed);

    NS_LOG_LOGIC(closest_loc << " " << closet_speed);

    if (station_sche.couriers.find(node_info.node_id) != station_sche.couriers.end())
    {
        /* send MA to train */
        GeneralMessage
            msg(MsgType::STATION_MA_TO_TRAIN,
                CLOSEST_SCHEDULED_TIME(cur_time, period, offset),
                0,
                nullptr,
                &secret_key, &public_key);
        msg.set_size(mam.serialize(msg.get_content_buf()));
        for (int dest : train_sche.couriers)
        {
            NODE_LOG_LOGIC("sending ma to " << train_id << "." << dest);
            SendLteMessage({train_id, dest}, msg);
        }
    }

    /* Prepare the PoC and send it to the measurers */
    PoCMessage poc(station_task_id, 0, nullptr);
    poc.poc_size = mam.serialize(poc.poc_contents);
    GeneralMessage poc_msg(POC,
                           CLOSEST_SCHEDULED_TIME(cur_time, period, offset),
                           0, nullptr,
                           &secret_key, &public_key);
    poc_msg.set_size(poc.serialize(poc_msg.get_content_buf()));
    for (int dest : schedules.at(TaskInRegion(GOVERNING, node_info.region_id))
                        .workers)
    {
        SendLanMessage({node_info.region_id, dest}, poc_msg);
    }
}