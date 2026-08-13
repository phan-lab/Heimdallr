#include "train_app.h"

#include "ns3/lte-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrainApp");

const double emergency_stop = -100;

void TrainApp::StartApplication()
{
    NS_LOG_LOGIC("TrainApp Start");
    /* Setup the socket */
    GeneralApp::StartApplication();
    mobility_model = m_node->GetObject<ConstantAccelerationMobilityModel>();
    // auto speed = mobility_model->GetVelocity();
#if FIG10_SCENARIO
    /* artifact-reproduction, Figure 10: "Two trains are initially separated by
     * 10 km and travel at 90 m/s" (paper Sec. VII-A).  The shipped default
     * places regions 2 km apart and starts them at rest. */
    mobility_model->SetVelocityAndAcceleration({(double)MAX_SPEED, 0.0, 0.0},
                                               {0.0, 0.0, 0.0});
    mobility_model->SetPosition({(node_info.region_id - 1) * 10000.0, 0.0, 0.0});
#else
    mobility_model->SetVelocityAndAcceleration({0.0, 0.0, 0.0},
                                               {0.0, 0.0, 0.0});
    mobility_model->SetPosition({(node_info.region_id) * 2000.0, 0.0, 0.0});
#endif
    ma_speed = MAX_SPEED;
#if FIG10_SCENARIO
    /* artifact-reproduction: "no movement authority yet" must be a negative
     * sentinel, not 0.  CalcTargetSpeed reads a restriction at 0 m as one the
     * train has already passed and commands an emergency stop, so every train
     * brakes from its first Actuate tick (t = 5 s) until a real authority
     * arrives.  It never surfaced in the shipped configuration because there
     * the trains start stationary 2 km apart.  `front_loc < 0` is already the
     * code's own "unrestricted" case. */
    ma_location = -1;
#else
    ma_location = 0;
#endif
    cur_enb = FIND_CLOSEST_ENB((node_info.region_id) * 2000.0);

#if ENABLE_TRACK_LOG
    track_log_ofs = std::ofstream(TRACK_LOG_FILE(node_info.region_id));
    NS_ASSERT(track_log_ofs.good());
    track_log_ofs << "Time(ms),Location(m),Speed(m/s)" << std::endl;
#endif

#if USE_TCP
    if (node_info.node_id != 0)
        Simulator::Schedule(MilliSeconds(100 * node_info.region_id + 50 * node_info.node_id), &TrainApp::SetupConnections, this);
#endif

    if (is_worker(GOVERNING))
        Simulator::Schedule(MicroSeconds(PING_OFFSET_BY_ID(node_info.region_id) +
                                         CONNECTION_SETUP_OFFSET + PING_INTERVAL - INTRA_REGION_LATENCY + SCHEDULE_VAR),
                            &GeneralApp::SendPingPrepare,
                            this, STATION_RID);

    Simulator::Schedule(MicroSeconds(PING_OFFSET_BY_ID(node_info.region_id) +
                                     CONNECTION_SETUP_OFFSET + PING_INTERVAL + PING_TIMEOUT),
                        &GeneralApp::SendPingAccept,
                        this, STATION_RID);

    /* schedule all application-level tasks */

    for (const auto &sche : schedules)
    {
        if (sche.first.region != node_info.region_id)
            continue;
        /* Schedule the primary */
        if (sche.second.primary == node_info.node_id)
        {
            switch (sche.first.task)
            {
            case TasksEnum::TRAIN_SENSOR_COLLECTS:
            {
                auto offset = MilliSeconds(CONNECTION_SETUP_OFFSET / 1000 +
                                           tasks.at(sche.first.task).offset);
                Simulator::Schedule(
                    offset, &TrainApp::SensorCollectsData, this);
            }
            break;
            case TasksEnum::TRAIN_UPDATE_LOCATION:
            {
                auto offset = MilliSeconds(
                    PING_OFFSET_BY_ID(node_info.region_id) / 1000 +
                    CONNECTION_SETUP_OFFSET / 1000 +
                    tasks.at(sche.first.task).offset);
                Simulator::Schedule(offset,
                                    &TrainApp::ControllerUploadLocationSpeed,
                                    this);
            }
            break;
            case TasksEnum::TRAIN_CALC_SPEED:
            {
                auto offset = MilliSeconds(CONNECTION_SETUP_OFFSET / 1000 +
                                           tasks.at(sche.first.task).offset);
                Simulator::Schedule(offset, &TrainApp::SendControlInput, this);
            }
            break;
            default:
                break;
            }
        }
    }
    if (node_info.node_id == ACTUATOR_NID)
        Simulator::Schedule(MilliSeconds(CONNECTION_SETUP_OFFSET / 1000), &TrainApp::Actuate, this);
#if REPUTATION_SYSTEM
    /* Setup the reputation system for PING */
    auto &local_sche = schedules.at({GOVERNING, node_info.region_id});

    auto &remote_sche = schedules.at({GOVERNING, STATION_RID});
    scores.emplace(std::make_pair(
        GOVERNING,
        CreditScores(this->node_info, GOVERNING, STATION_RID,
                     this->node_info.region_id,
                     &remote_sche,
                     &local_sche,
                     PING_TIMEOUT)));

#endif
}

void TrainApp::setMobilityInfo(double speed, double acc, double pos)
{
    mobility_model->SetVelocityAndAcceleration({speed, 0.0, 0.0},
                                               {acc, 0.0, 0.0});
    mobility_model->SetPosition({pos, 0.0, 0.0});
}

void TrainApp::ProcessLanMsg(Ptr<Socket> socket)
{
    Address sourceAddr;
    Ptr<Packet> packet = socket->RecvFrom(sourceAddr);
    InetSocketAddress inetSocketAddr = InetSocketAddress::ConvertFrom(sourceAddr);
    Ipv4Address sourceAddress = inetSocketAddr.GetIpv4();
    // uint16_t sourcePort = inetSocketAddr.GetPort();
    NodeInfo source_node = m_lan_ip2id_map.at(sourceAddress);

    GeneralMessage msg;
    packet->RemoveHeader(msg);
    NS_LOG_LOGIC(node_info.to_string() << " " << Simulator::Now().GetSeconds()
                                       << " LAN Message from " << source_node.to_string() << " type " << msg.get_type());
    ProcessMessage(msg, source_node);
}

void TrainApp::ProcessWirelessMsg(Ptr<Socket> socket)
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

    // NODE_LOG_UNCOND("packet size" << packet->GetSerializedSize());
    GeneralMessage msg;
    packet->RemoveHeader(msg);
    ProcessMessage(msg, source_node);
}

void TrainApp::ProcessMessage(const GeneralMessage &msg, const NodeInfo &src)
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
    case MsgType::SENSOR_LOCATION_SPEED:
        this->ProcessSensorData(msg, src);
        break;
    case MsgType::CONTROL_UPDATE_SPEED_LOCAL:
        this->ProcessControlUpdateLocal(msg, src);
        break;
    case MsgType::POC:
        this->ProcessPoC(msg, src);
        break;
    case MsgType::STATION_MA_TO_TRAIN:
        this->ProcessMA(msg, src);
        break;
    case MsgType::CONTROL_MA_FWD:
        this->ProcessMAFwd(msg, src);
        break;
    case MsgType::CONTROL_MA_AS_INPUT:
        this->ProcessControlInput(msg, src);
        break;
    case MsgType::CONTROL_SPEED_TO_ACTUATOR:
        this->ProcessTargetSpeed(msg, src);
        break;
    default:
        NS_LOG_ERROR("Unknown type " << type);
        NS_ASSERT(0);
        break;
    }
}

#if USE_TCP

void TrainApp::SetupConnections()
{
    NODE_LOG_UNCOND("Set up connections");

    /* Connect to all server measurers */
    for (const auto &ent : this->m_lte_ip2id_map)
    {
        if (ent.second.region_id != STATION_RID)
            continue;
        const auto &serv_ip = ent.first;
        InetSocketAddress remote_addr = InetSocketAddress(serv_ip, LTE_PORT);
        NS_LOG_LOGIC(node_info.to_string() << ": Try Connecting..." << serv_ip);
        TypeId tid_lte = TypeId::LookupByName("ns3::TcpSocketFactory");
        m_lte_socket = Socket::CreateSocket(GetNode(), tid_lte);
        m_lte_socket->Bind(m_wirelessAddr);
        m_lte_socket->Connect(remote_addr);
        m_lte_socket->SetConnectCallback(MakeCallback(&TrainApp::ConnectionSucceeded, this),
                                         MakeCallback(&TrainApp::ConnectionFailed, this));
    }
}

void TrainApp::ConnectionSucceeded(Ptr<Socket> socket)
{
    Address dest;
    socket->GetPeerName(dest);
    NodeInfo server_info = this->m_lte_ip2id_map.at(InetSocketAddress::ConvertFrom(dest).GetIpv4());
    NS_LOG_LOGIC("Connected Success to: " << server_info.to_string());
    m_connections.insert(std::make_pair(server_info, socket));
    socket->SetRecvCallback(MakeCallback(&GeneralApp::ProcessWirelessMsg, this));
}

void TrainApp::ConnectionFailed(Ptr<Socket> socket)
{
    Address dest;
    socket->GetPeerName(dest);
    NodeInfo server_info = this->m_lte_ip2id_map.at(InetSocketAddress::ConvertFrom(dest).GetIpv4());
    NS_LOG_ERROR("Connected Failure to: " << server_info.to_string());
}

void TrainApp::SendLteMessageReal(const NodeInfo &dest, const GeneralMessage &msg)
{
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(msg);
    this->m_connections.at(dest)->Send(packet);
}

void TrainApp::SendLteMessage(const NodeInfo &dest, const GeneralMessage &msg)
{
    // /* Suppose there is round robin so that they won't be sent at the exact same time */
    Simulator::Schedule(MicroSeconds(node_info.region_id * 5000 + node_info.node_id * 10000 + dest.node_id * 2500),
                        &TrainApp::SendLteMessageReal, this, dest, msg);
    NS_ASSERT(dest.region_id == STATION_RID);
    // SendLteMessageReal(dest, msg);
}
#endif

double TrainApp::get_position()
{
    return mobility_model->GetPosition().x;
}

double TrainApp::get_speed()
{
    return mobility_model->GetVelocity().x;
}

double TrainApp::get_acc()
{
    return acc;
}

void TrainApp::SensorCollectsData()
{
    /* Sensor collects data and sends to the primary of next task */
    Simulator::Schedule(MilliSeconds(tasks.at(TRAIN_SENSOR_COLLECTS).period),
                        &TrainApp::SensorCollectsData,
                        this);
    double position = get_position();
    double speed = mobility_model->GetVelocity().x;
    double content[2];
    content[0] = position;
    content[1] = speed;

    int64_t cur_time = Simulator::Now().GetMicroSeconds();
    const auto &task_info = tasks.at(TasksEnum::TRAIN_SENSOR_COLLECTS);
    int period = MILLI_TO_MICRO * task_info.period;
    int offset = MILLI_TO_MICRO * task_info.offset;
    GeneralMessage msg(SENSOR_LOCATION_SPEED,
                       CLOSEST_SCHEDULED_TIME(cur_time, period, offset),
                       sizeof(content),
                       (uint8_t *)content,
                       &secret_key,
                       &public_key);

    const auto &down_sche = schedules.at(
        TaskInRegion(task_info.successor, node_info.region_id));

    for (int dest : down_sche.workers)
        SendLanMessage({node_info.region_id, dest}, msg);
}

/**
 * @note Called by the primary that receives data from the sensor.
 * Need to:
 *    Send the data with the sensor's signature to the backups,
 *    so that the backups and further verify the result and
 *    generate the PoC (and send the data to the station if courier)
 */
void TrainApp::ProcessSensorData(const GeneralMessage &msg, const NodeInfo &src)
{
    double content[2];
    memcpy(content, msg.get_constant_content_buf(), sizeof(content));
    double position = content[0];
    double speed = content[1];
    NS_LOG_LOGIC("Time: " << Simulator::Now().GetMilliSeconds()
                          << " " << node_info.to_string()
                          << ": (loc, speed)=" << position
                          << ", " << speed);

    /* Store the data locally */
    this->position = position;
    this->speed = speed;
    memcpy(this->sig_of_sensor,
           msg.get_constant_sig_buf(), MULTISIG_SIG_LENGTH);
}

/**
 * @note Called by the primary. The primary needs to do the following:
 * 1. Send the location and speed to the station couriers.
 * 2. Send the location, speed, and the sensor's signature to backups,
 *    so that the courier can send the location/speed, and other backups
 *    can generate the PoC.
 *
 * @note In theory, the train controllers can simply attach the signature
 * from the sensor to show its correctness. We do this way to make it more
 * general (the PoC solution).
 */
void TrainApp::ControllerUploadLocationSpeed()
{
    int period = MILLI_TO_MICRO * tasks.at(TRAIN_UPDATE_LOCATION).period;
    int offset = MILLI_TO_MICRO * tasks.at(TRAIN_UPDATE_LOCATION).offset +
                 PING_OFFSET_BY_ID(node_info.region_id);

    Simulator::Schedule(MicroSeconds(period),
                        &TrainApp::ControllerUploadLocationSpeed, this);

    const auto &train_sche = schedules.at(TaskInRegion(
        TasksEnum::TRAIN_UPDATE_LOCATION,
        node_info.region_id));
    const auto &station_sche = schedules.at(TaskInRegion(
        TasksEnum::STATION_RECV_LOCATION_SPEED +
            MAX_TRAINS * node_info.region_id,
        STATION_RID));

    int64_t cur_time = Simulator::Now().GetMicroSeconds();

    /* Send to its peers */
    uint8_t buf_to_peer[sizeof(speed) + sizeof(position) + MULTISIG_SIG_LENGTH];

    memcpy(buf_to_peer, &position, sizeof(position));
    memcpy(buf_to_peer + sizeof(position), &speed, sizeof(speed));
    memcpy(buf_to_peer + sizeof(position) + sizeof(speed),
           sig_of_sensor, MULTISIG_SIG_LENGTH);

    NODE_LOG_LOGIC(cur_time << " " << period << " " << offset << " "
                             << CLOSEST_SCHEDULED_TIME(cur_time, period, offset));

    GeneralMessage msg_to_peer(MsgType::CONTROL_UPDATE_SPEED_LOCAL,
                               CLOSEST_SCHEDULED_TIME(cur_time, period, offset),
                               sizeof(buf_to_peer),
                               (uint8_t *)buf_to_peer,
                               &secret_key,
                               &public_key);
    for (int dest : train_sche.workers)
    {
        if (dest != node_info.node_id)
            SendLanMessage({node_info.region_id, dest}, msg_to_peer);
    }

    /* Send to the station */
    uint8_t buf_to_station[sizeof(speed) + sizeof(position)];
    memcpy(buf_to_station, &position, sizeof(position));
    memcpy(buf_to_station + sizeof(position), &speed, sizeof(speed));

    GeneralMessage
        msg_to_station(MsgType::CONTROL_UPDATE_SPEED_TO_STATION,
                       CLOSEST_SCHEDULED_TIME(cur_time, period, offset),
                       sizeof(buf_to_station),
                       buf_to_station,
                       &secret_key, &public_key);
    for (int dest : station_sche.couriers)
    {
        // NODE_LOG_UNCOND("sending task 6 1");
        SendLteMessage({STATION_RID, dest}, msg_to_station);
    }
}

/**
 * @note Called by the workers. The worker need to do the following:
 * 1. Verify the signature of the sensor.
 * 2. If courier, send the message to the station.
 * 3. Generate the PoC and send to measurers.
 */
void TrainApp::ProcessControlUpdateLocal(const GeneralMessage &msg, const NodeInfo &src)
{
    int station_task_id = TasksEnum::STATION_RECV_LOCATION_SPEED +
                          MAX_TRAINS * node_info.region_id;
    const auto &train_sche = schedules.at(TaskInRegion(
        TasksEnum::TRAIN_UPDATE_LOCATION,
        node_info.region_id));
    const auto &station_sche = schedules.at(
        TaskInRegion(station_task_id, STATION_RID));

    if (train_sche.couriers.find(node_info.node_id) != train_sche.couriers.end())
    {
        /* send to station */
        uint8_t buf_to_station[sizeof(speed) + sizeof(position)];
        memcpy(buf_to_station, msg.get_constant_content_buf(), sizeof(buf_to_station));

        GeneralMessage
            msg_to_station(MsgType::CONTROL_UPDATE_SPEED_TO_STATION,
                           msg.get_seq_num(),
                           sizeof(buf_to_station),
                           buf_to_station,
                           &secret_key, &public_key);
        for (int dest : station_sche.couriers)
        {
            SendLteMessage({STATION_RID, dest}, msg_to_station);
            // NODE_LOG_UNCOND("sending task 6 2 " << dest);
        }
    }

    /* Prepare the PoC and send it to the measurers */
    PoCMessage poc(TRAIN_UPDATE_LOCATION, sizeof(position) + sizeof(speed),
                   msg.get_constant_content_buf());
    GeneralMessage poc_msg(POC, msg.get_seq_num(), 0, nullptr,
                           &secret_key, &public_key);
    poc_msg.set_size(poc.serialize(poc_msg.get_content_buf()));
    for (int dest : schedules.at(TaskInRegion(GOVERNING, node_info.region_id))
                        .workers)
    {
        // NODE_LOG_UNCOND("sending poc");
        SendLanMessage({node_info.region_id, dest}, poc_msg);
    }
}

void TrainApp::ProcessMA(const GeneralMessage &msg, const NodeInfo &src)
{
    MoveAuthMsg mam(msg.get_constant_content_buf());
    NS_ASSERT(mam.which_train == node_info.region_id);
    int64_t cur_time = Simulator::Now().GetMicroSeconds();
    /* You cannot receive a message that is sent in the future */
    if (msg.get_seq_num() > cur_time)
        NS_ASSERT(0);

    NODE_LOG_LOGIC("receive MA from " << src.to_string());

    int task_id = TasksEnum::TRAIN_RECV_MA;
#if REPUTATION_SYSTEM
    scores.at(task_id).round_recv(src, node_info);
#endif
    TaskInRegion train_tir(task_id, node_info.region_id);
    TaskInRegion station_tir(TasksEnum::STATION_COMPUTE_MA +
                                 MAX_TRAINS * node_info.region_id,
                             STATION_RID);

    ma_location = mam.front_location;
    ma_speed = mam.front_speed;
    memcpy(ma_sig, msg.get_constant_sig_buf(), MULTISIG_SIG_LENGTH);

    MoveAuthFwd maf(src.node_id, mam.front_location, mam.front_speed,
                    msg.get_constant_sig_buf());
    GeneralMessage msg_maf(MsgType::CONTROL_MA_FWD, msg.get_seq_num(),
                           0, nullptr, &secret_key, &public_key);
    msg_maf.set_size(maf.serialize(msg_maf.get_content_buf()));

    for (int dest : schedules.at(train_tir).workers)
    {
        if (dest != node_info.node_id)
            SendLanMessage({node_info.region_id, dest}, msg_maf);
    }

    /* Schedule the timeout for PoC check */
    int64_t timeout = msg.get_seq_num() +
                      tasks.at(task_id).period * MILLI_TO_MICRO - cur_time;
    Simulator::Schedule(MicroSeconds(timeout), &GeneralApp::CheckForPoC,
                        this, station_tir, msg.get_seq_num());
    scheduled_check[station_tir] = msg.get_seq_num();
}

void TrainApp::ProcessMAFwd(const GeneralMessage &msg, const NodeInfo &src)
{
    NS_LOG_LOGIC("Process MA fwd ");

    int64_t cur_time = Simulator::Now().GetMicroSeconds();
    int task_id = TasksEnum::TRAIN_RECV_MA;
    MoveAuthFwd maf(msg.get_constant_content_buf());
    /* You cannot receive a message that is sent in the future */
    if (msg.get_seq_num() > cur_time)
        NS_ASSERT(0);
#if REPUTATION_SYSTEM
    scores.at(TasksEnum::TRAIN_RECV_MA).round_recv({STATION_RID, maf.issued_node}, src);
#endif
    TaskInRegion train_tir(task_id, node_info.region_id);
    TaskInRegion station_tir(TasksEnum::STATION_COMPUTE_MA +
                                 MAX_TRAINS * node_info.region_id,
                             STATION_RID);
    // if (scheduled_check.find(station_tir) != scheduled_check.end() &&
    //     scheduled_check.at(station_tir) >= msg.get_seq_num())
    // {
    //     /* already processed. */
    //     return;
    // }
    ma_location = maf.front_location;
    ma_speed = maf.front_speed;
    // NODE_LOG_UNCOND("Process fwd " << ma_location << " " << ma_speed);
    memcpy(ma_sig, maf.signature, MULTISIG_SIG_LENGTH);
}

void TrainApp::SendControlInput()
{
    Simulator::Schedule(MilliSeconds(tasks.at(TRAIN_CALC_SPEED).period),
                        &TrainApp::SendControlInput,
                        this);
    int period = MILLI_TO_MICRO * tasks.at(TRAIN_CALC_SPEED).period;
    int offset = MILLI_TO_MICRO * tasks.at(TRAIN_CALC_SPEED).offset;
    int64_t cur_time = Simulator::Now().GetMicroSeconds();

    // NODE_LOG_UNCOND("Send input " <<ma_location << " " << ma_speed);

    MoveAuthFwd maf(-1, ma_location, ma_speed, ma_sig);
    /* Reuse the maf class, but a different type */
    GeneralMessage msg(MsgType::CONTROL_MA_AS_INPUT,
                       CLOSEST_SCHEDULED_TIME(cur_time, period, offset),
                       0,
                       nullptr,
                       &secret_key, &public_key);
    msg.set_size(maf.serialize(msg.get_content_buf()));

    for (int dest : schedules.at(TaskInRegion(TRAIN_CALC_SPEED, node_info.region_id))
                        .workers)
    {
        if (dest == node_info.node_id)
            continue;
        SendLanMessage({node_info.region_id, dest}, msg);
    }

    double target_speed = CalcTargetSpeed(ma_speed, ma_location);
    GeneralMessage msg_to_act(MsgType::CONTROL_SPEED_TO_ACTUATOR,
                              CLOSEST_SCHEDULED_TIME(cur_time, period, offset),
                              sizeof(target_speed),
                              (uint8_t *)&target_speed,
                              &secret_key, &public_key);
    SendLanMessage({node_info.region_id, ACTUATOR_NID}, msg_to_act);
    // NODE_LOG_UNCOND( " Sending  " << target_speed);
}

void TrainApp::ProcessControlInput(const GeneralMessage &msg, const NodeInfo &src)
{

    MoveAuthFwd maf(msg.get_constant_content_buf());
    ma_location = maf.front_location;
    ma_speed = maf.front_speed;
    double target_speed = CalcTargetSpeed(maf.front_speed, maf.front_location);
    GeneralMessage msg_to_act(MsgType::CONTROL_SPEED_TO_ACTUATOR,
                              msg.get_seq_num(),
                              sizeof(target_speed),
                              (uint8_t *)&target_speed,
                              &secret_key, &public_key);
    SendLanMessage({node_info.region_id, ACTUATOR_NID}, msg_to_act);
    // NODE_LOG_UNCOND(" Sending  " << target_speed);
}

void TrainApp::ProcessTargetSpeed(const GeneralMessage &msg, const NodeInfo &src)
{

    if (msg.get_seq_num() > target_speed_seq)
    {
        target_speed = *((double *)msg.get_constant_content_buf());
        target_speed_seq = msg.get_seq_num();
        // NODE_LOG_UNCOND("\nAct receives target speed " << target_speed << " from " << src.to_string());
    }
    else
    {
        double proposed = *((double *)msg.get_constant_content_buf());
        if (proposed != target_speed)
        {
            NODE_LOG_ERROR("\n " << proposed << " " << target_speed);
            NS_ASSERT(0);
        }
    }
}

double TrainApp::CalcTargetSpeed(double front_speed, double front_loc)
{

#if FIG10_SCENARIO
    /* What the following train (region 1) knows about the leading train.
     *
     * The leading train stops transmitting when it loses connectivity at
     * t = 10 s, so from then on `front_loc` is whatever this train last heard.
     * FIG10_LAST_KNOWN is the position the leading train had reached by then:
     * it started at 10 km and ran at MAX_SPEED for 10 s.
     */
    const double FIG10_LAST_KNOWN = 10000.0 + 10.0 * MAX_SPEED;

    if (node_info.region_id == 1)
    {
#if FIG10_SCENARIO == FIG10_STABLE
        /* Stable network: the movement authority keeps arriving, so the train
         * tracks the leading train's real position and slows in time.  No
         * override - `front_loc` is whatever the protocol delivered. */
#elif FIG10_SCENARIO == FIG10_MANUAL
        /* Manual signalling: nothing reaches the following train until the
         * control centre gets word to the driver at t = 120 s, and the driver
         * takes FIG10_DRIVER_REACTION_S to apply the brake.
         *
         * The reaction delay is recovered from the recorded trace rather than
         * assumed.  That run holds 90 m/s through t = 130 s and then
         * decelerates at MAX_EMERGENT_ACC; taking the onset from its speed at
         * t = 150 s (66.0 m/s) and predicting t = 178 s gives 14637.6 m at
         * 32.40 m/s, against 14638 m at 32.40 m/s recorded.  Braking at
         * t = 120 s instead stops the train 1.6 km short of the collision the
         * incident actually produced. */
        const double FIG10_DRIVER_REACTION_S = 10.0;
        if (Simulator::Now().GetSeconds() < 120 + FIG10_DRIVER_REACTION_S)
            front_loc = -1;
        else
            front_loc = FIG10_LAST_KNOWN;
#elif FIG10_SCENARIO == FIG10_HEIMDALLR
        /* HEIMDALLR: the heartbeat expected at t = 10 s does not arrive, so the
         * control centre transitions to safe mode one timeout later and
         * truncates the movement authority to the leading train's last known
         * safe position.  With the paper's D_t/o = 200 ms that is t = 10.2 s,
         * as stated in Sec. VII-A. */
        if (Simulator::Now().GetMicroSeconds() >= 10 * 1000000 + PING_TIMEOUT)
            front_loc = FIG10_LAST_KNOWN;
#endif
    }
#endif

    // NODE_LOG_UNCOND(front_speed << " " << front_loc << " " << get_position() << "  my speed " << get_speed());

    if (front_loc < 0)
        return MAX_SPEED;
    else
    {
        double braking_distance = get_speed() * get_speed() / 2 / MAX_NORMAL_ACC;
        // NODE_LOG_UNCOND("buffer " << front_loc - braking_distance - get_position());
        if (front_loc - braking_distance - get_position() < 500)
            return emergency_stop;
        else if (front_loc - braking_distance - get_position() < 1000)
            return 0;
        else
            return MAX_SPEED;
    }
}

void TrainApp::Actuate()
{
    Simulator::Schedule(MilliSeconds(ACTUATE_FREQ), &TrainApp::Actuate, this);
    auto speed = mobility_model->GetVelocity().x;

    // double braking_distance = speed * speed / 2 / MAX_NORMAL_ACC;
    double a;

    /* Train 2 is in sight mode */
#if FIG10_SCENARIO
    /* The leading train loses connectivity at t = 10 s and enters forced
     * braking; at t = 120 s its driver takes over and moves it on-sight at
     * 20 km/h = 5.56 m/s.  Same in all three variants - what differs is what
     * the *following* train knows about it (see CalcTargetSpeed). */
    if (node_info.region_id == 2 && Simulator::Now().GetSeconds() >= 10)
        target_speed = emergency_stop;
    if (node_info.region_id == 2 && Simulator::Now().GetSeconds() >= 120)
        target_speed = 5.56;
#endif

    if (target_speed == emergency_stop)
    {
        a = -MAX_EMERGENT_ACC;
    }
    else if (speed > target_speed)
    {
        a = -MAX_NORMAL_ACC;
    }
    else if (speed < target_speed)
        a = MAX_NORMAL_ACC;
    else
        a = 0;

    if (speed <= 0 && target_speed <= 0)
    {
        a = 0;
        speed = 0;
    }

    /* Manual command */
    // if (node_info.region_id == 1)
    // {
    //     if (Simulator::Now().GetSeconds() >= 130)
    //     {
    //         a = -MAX_EMERGENT_ACC;
    //     }
    //     else
    //     {
    //         a = 0;
    //     }
    // }

    // NODE_LOG_UNCOND(node_info.region_id << " " << target_speed << " " << acc);

    mobility_model->SetVelocityAndAcceleration({speed, 0, 0},
                                               {a, 0, 0});
    acc = a;

#if ENABLE_TRACK_LOG
    track_log_ofs << Simulator::Now().GetMilliSeconds() << ","
                  << mobility_model->GetPosition().x << "," << speed << std::endl;
#endif
}