#include "general_app.h"

using std::map;
using std::string;

#define FIND_CLOSEST_ROUND(cur) (CLOSEST_SCHEDULED_TIME(cur, PING_INTERVAL, 0))

NS_LOG_COMPONENT_DEFINE("GeneralApp");

namespace ns3
{

    bool
    GeneralApp::is_worker(int task)
    {
        const auto &schedule = schedules.at(TaskInRegion(task, node_info.region_id));
        return schedule.workers.find(node_info.node_id) != schedule.workers.end();
    }

    bool
    GeneralApp::is_courier(int task)
    {
        const auto &schedule = schedules.at(TaskInRegion(task, node_info.region_id));
        return schedule.couriers.find(node_info.node_id) != schedule.couriers.end();
    }

    bool
    GeneralApp::is_primary(int task)
    {
        const auto &schedule = schedules.at(TaskInRegion(task, node_info.region_id));
        return schedule.primary == node_info.node_id;
    }

    void
    GeneralApp::StartApplication()
    {
        /* Setup the socket */

        TypeId tid_lan = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_lan_socket = Socket::CreateSocket(GetNode(), tid_lan);
        InetSocketAddress local_app = InetSocketAddress(Ipv4Address::GetAny(), LAN_PORT);
        m_lan_socket->SetAllowBroadcast(true);
        m_lan_socket->Bind(local_app);
        m_lan_socket->SetRecvCallback(MakeCallback(&GeneralApp::ProcessLanMsg, this));
#if USE_UDP
        TypeId tid_lte = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_lte_socket = Socket::CreateSocket(GetNode(), tid_lte);
        InetSocketAddress local_wireless = InetSocketAddress(m_wirelessAddr, LTE_PORT);
        m_lte_socket->Bind(local_wireless);
        m_lte_socket->SetRecvCallback(MakeCallback(&GeneralApp::ProcessWirelessMsg, this));
#endif
#if USE_TCP
        /* See child classes */
#endif
        this->ping_accept_evidence.clear();
        this->ping_accepted.clear();
        this->ping_current_round.clear();
        this->ping_min_latency.clear();
        this->ping_proposed.clear();
#if REPUTATION_SYSTEM
        /* Initialize credit scores */
        for (auto &task : tasks)
        {
            /* if the node is not working on this task, skip */
            if (schedules.find({task.first, node_info.region_id}) == schedules.end())
                continue;
            if (schedules.at({task.first, node_info.region_id})
                    .workers.find(node_info.node_id) ==
                schedules.at({task.first, node_info.region_id}).workers.end())
                continue;

            if (task.second.inter_region != -1 &&
                task.second.incoming == INCOMING)
            {
                scores.emplace(std::make_pair(
                    task.first,
                    CreditScores(this->node_info,
                                 task.first,
                                 task.second.inter_region,
                                 this->node_info.region_id,
                                 &this->schedules.at({task.second.predecessor,
                                                      task.second.inter_region}),
                                 &this->schedules.at({task.first,
                                                      this->node_info.region_id}),
                                 PING_INTERVAL)));
            }
        }
#endif
    }

    void
    GeneralApp::setLanMapping(const map<NodeInfo, Ipv4Address> &id2ip,
                              const map<Ipv4Address, NodeInfo> &ip2id)
    {
        this->m_lan_id2ip_map = id2ip;
        this->m_lan_ip2id_map = ip2id;
    }

    void
    GeneralApp::setLteMapping(const map<NodeInfo, Ipv4Address> &id2ip,
                              const map<Ipv4Address, NodeInfo> &ip2id)
    {
        this->m_lte_id2ip_map = id2ip;
        this->m_lte_ip2id_map = ip2id;
    }

    void
    GeneralApp::SendLanMessage(const NodeInfo &dest, const GeneralMessage &msg)
    {
        // NS_ASSERT(dest.region_id == this->node_info.region_id);
        if (dest.region_id != this->node_info.region_id && dest.region_id != -1)
            return;
        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(msg);
        NS_LOG_LOGIC(Simulator::Now().GetSeconds() << " sending LAN message");
        if (dest.region_id == -1 && dest.node_id == -1)
        {
            NS_LOG_LOGIC(Simulator::Now().GetSeconds() << " broadcast LAN message");
            m_lan_socket->SendTo(packet, 0, InetSocketAddress(Ipv4Address("123.1.255.255"), LAN_PORT));
        }
        else
            m_lan_socket->SendTo(packet,
                                 0,
                                 InetSocketAddress(this->m_lan_id2ip_map.at(dest), LAN_PORT));
    }

    void
    GeneralApp::SendLteMessageReal(const NodeInfo &dest, const GeneralMessage &msg)
    {
#if USE_UDP
        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(msg);
        m_lte_socket->SendTo(packet, 0, InetSocketAddress(this->m_lte_id2ip_map.at(dest), LTE_PORT));
#endif
    }

    void
    GeneralApp::SendLteMessage(const NodeInfo &dest, const GeneralMessage &msg)
    {
#if USE_UDP
        Simulator::Schedule(MilliSeconds(node_info.node_id), &GeneralApp::SendLteMessageReal, this, dest, msg);
#endif
#if USE_TCP
        /* Use child class */
        NS_ASSERT(0);
#endif
    }

    void
    GeneralApp::installKeys(MultiSigObj &pk, MultiSigObj &sk)
    {
        MSObj_set(this->public_key, pk);
        MSObj_set(this->secret_key, sk);
    }

    void
    GeneralApp::installTaskSchedules(std::map<TaskInRegion, Schedule> &schedules,
                                     std::map<int, Task> &tasks,
                                     std::map<KnownFault, std::string> &mode_change_map)
    {
        // NODE_LOG_UNCOND("tasks installed " << tasks.size());
        this->schedules = schedules;
        this->tasks = tasks;
        this->mode_change_map = mode_change_map;
    }

    int
    GeneralApp::GetNextSequenceNumber()
    {
        return seq++;
    }

    void
    GeneralApp::SendPingPrepare(int dest_region)
    {
        /* If not measurer, skip */
        if (!is_worker(GOVERNING))
            return;
        /* Schedule the next ping message */
        // int64_t next_time = FIND_CLOSEST_ROUND(Simulator::Now().GetMicroSeconds()) +
        //                     PING_INTERVAL - Simulator::Now().GetMicroSeconds() + PING_OFFSET_BY_ID(dest_region);
        // Simulator::Schedule(MicroSeconds(next_time + SCHEDULE_VAR),
        //                     &GeneralApp::SendPingPrepare, this, dest_region);
        Simulator::Schedule(MicroSeconds(PING_INTERVAL),
                            &GeneralApp::SendPingPrepare, this, dest_region);

        int offset_id = dest_region == 0 ? node_info.region_id : dest_region;
        int64_t seq_num =
            CLOSEST_SCHEDULED_TIME(Simulator::Now().GetMicroSeconds(), PING_INTERVAL, PING_OFFSET_BY_ID(offset_id));
        // NODE_LOG_UNCOND("seq " << seq_num);
        GeneralMessage msg((int)MsgType::PING_PREPARE,
                           seq_num,
                           0,
                           nullptr,
                           &this->secret_key,
                           &this->public_key);

        /* Check the PoC and include in the PPM */
        uint8_t poc_buf[MAXBUF];
        uint32_t poc_buf_offset = 0;

        if (poc_to_include.at(dest_region).find(seq_num) !=
            poc_to_include.at(dest_region).end())
        {
            for (const auto &poc : poc_to_include.at(dest_region).at(seq_num))
            {
                poc_buf_offset += poc.serialize(poc_buf + poc_buf_offset);
            }
        }
        PingPrepareMessage ppm(dest_region, poc_buf_offset, poc_buf);
        msg.set_size(ppm.serialize(msg.get_content_buf()));

        /* Sign and put into the prepare buffer */
        msg.sign();
        int &which_sig_entry = this->ping_prepare_num_sigs[dest_region];
        memcpy(&this->ping_prepare_sigs[dest_region][which_sig_entry][0],
               msg.get_constant_sig_buf(),
               MULTISIG_SIG_LENGTH);
        memcpy(&this->ping_prepare_pubs[dest_region][which_sig_entry][0],
               msg.get_constant_pub_key_buf(),
               MULTISIG_PUB_LENGTH);
        which_sig_entry++;

        /* broadcast the ping prepare message */
        NodeInfo bc_dest;
        bc_dest.region_id = -1, bc_dest.node_id = -1;
        this->SendLanMessage(bc_dest, msg);

        /* Sometimes others' prepare arrives early */
        if (this->ping_prepare_num_sigs[ppm.dest_region] == F_MAX + 1)
        {
            this->ping_prepare_num_sigs[ppm.dest_region] = 0;
            // memset(this->ping_prepare_sigs[dest_region], 0, (F_MAX + 1) * MULTISIG_SIG_LENGTH);
            /* ready to send out ping messages */
            if (this->type == TRAIN)
            {
                /* ping station */
                int dest_rid = STATION_RID;
                PingInterMessage pim(ppm.dest_region, ppm.m_other_size, ppm.m_other,
                                     F_MAX + 1,
                                     &this->ping_prepare_pubs[ppm.dest_region][0][0],
                                     &this->ping_prepare_sigs[ppm.dest_region][0][0]);
                GeneralMessage msg2(PING_INTER,
                                    msg.get_seq_num(),
                                    0,
                                    nullptr,
                                    &this->secret_key,
                                    &this->public_key);
                msg2.set_size(pim.serialize(msg2.get_content_buf()));

                const auto &station_sche = schedules.at(TaskInRegion(GOVERNING, dest_rid));
                for (const auto &ent : this->m_lte_id2ip_map)
                {
                    if (ent.first.region_id == dest_rid &&
                        station_sche.workers.find(ent.first.node_id) != station_sche.workers.end())
                    {
                        this->SendLteMessage(ent.first, msg2);
                        NODE_LOG_LOGIC("sending ping seq " << msg2.get_seq_num());
                    }
                }
            }
            else
            {
                /* ping all other regions */
                PingInterMessage pim(ppm.dest_region, ppm.m_other_size, ppm.m_other,
                                     F_MAX + 1,
                                     &this->ping_prepare_pubs[ppm.dest_region][0][0],
                                     &this->ping_prepare_sigs[ppm.dest_region][0][0]);
                GeneralMessage msg2(PING_INTER,
                                    msg.get_seq_num(),
                                    0,
                                    nullptr,
                                    &this->secret_key,
                                    &this->public_key);
                msg2.set_size(pim.serialize(msg2.get_content_buf()));
                // NODE_LOG_UNCOND("ppm other size " << ppm.m_other_size);
                // NODE_LOG_UNCOND("ping size " << msg2.get_size());
                for (const auto &ent : this->m_lte_id2ip_map)
                {
                    const auto &train_sche = schedules.at(TaskInRegion(GOVERNING, ent.first.region_id));
                    if (ent.first.region_id == ppm.dest_region &&
                        train_sche.workers.find(ent.first.node_id) != train_sche.workers.end())
                    {
                        this->SendLteMessage(ent.first, msg2);
                        NODE_LOG_LOGIC("sending ping seq " << msg2.get_seq_num());
                    }
                }
            }
        }
    }

    void
    GeneralApp::ProcessPingPrepare(const GeneralMessage &msg)
    {
        if (!is_worker(GOVERNING))
            return;
        PingPrepareMessage ppm(msg.get_constant_content_buf());

        int &which_sig_entry = this->ping_prepare_num_sigs[ppm.dest_region];
        memcpy(&this->ping_prepare_sigs[ppm.dest_region][which_sig_entry][0],
               msg.get_constant_sig_buf(),
               MULTISIG_SIG_LENGTH);
        memcpy(&this->ping_prepare_pubs[ppm.dest_region][which_sig_entry][0],
               msg.get_constant_pub_key_buf(),
               MULTISIG_PUB_LENGTH);
        which_sig_entry++;

        if (this->ping_prepare_num_sigs[ppm.dest_region] == F_MAX + 1)
        {
            this->ping_prepare_num_sigs[ppm.dest_region] = 0;
            // memset(this->ping_prepare_sigs[ppm.dest_region], 0, (F_MAX + 1) * MULTISIG_SIG_LENGTH);
            /* ready to send out ping messages */
            if (this->type == TRAIN)
            {
                /* ping station */
                int dest_rid = STATION_RID;
                PingInterMessage pim(ppm.dest_region, ppm.m_other_size, ppm.m_other,
                                     F_MAX + 1,
                                     &this->ping_prepare_pubs[ppm.dest_region][0][0],
                                     &this->ping_prepare_sigs[ppm.dest_region][0][0]);
                GeneralMessage msg2(PING_INTER,
                                    msg.get_seq_num(),
                                    0,
                                    nullptr,
                                    &this->secret_key,
                                    &this->public_key);
                msg2.set_size(pim.serialize(msg2.get_content_buf()));

                const auto &station_sche = schedules.at(TaskInRegion(GOVERNING, dest_rid));
                for (const auto &ent : this->m_lte_id2ip_map)
                {
                    if (ent.first.region_id == dest_rid &&
                        station_sche.workers.find(ent.first.node_id) != station_sche.workers.end())
                    {
                        this->SendLteMessage(ent.first, msg2);
                        NODE_LOG_LOGIC("sending ping seq " << msg2.get_seq_num());
                    }
                }
            }
            else
            {
                /* ping all other regions */
                PingInterMessage pim(ppm.dest_region, ppm.m_other_size, ppm.m_other,
                                     F_MAX + 1,
                                     &this->ping_prepare_pubs[ppm.dest_region][0][0],
                                     &this->ping_prepare_sigs[ppm.dest_region][0][0]);
                GeneralMessage msg2(PING_INTER,
                                    msg.get_seq_num(),
                                    0,
                                    nullptr,
                                    &this->secret_key,
                                    &this->public_key);
                msg2.set_size(pim.serialize(msg2.get_content_buf()));
                // NODE_LOG_UNCOND("ppm other size " << ppm.m_other_size);
                // NODE_LOG_UNCOND("ping size " << msg2.get_size());
                for (const auto &ent : this->m_lte_id2ip_map)
                {
                    const auto &train_sche = schedules.at(TaskInRegion(GOVERNING, ent.first.region_id));
                    if (ent.first.region_id == ppm.dest_region &&
                        train_sche.workers.find(ent.first.node_id) != train_sche.workers.end())
                    {
                        this->SendLteMessage(ent.first, msg2);
                        NODE_LOG_LOGIC("sending ping seq " << msg2.get_seq_num());
                    }
                }
            }
        }
    }

    void
    GeneralApp::ProcessPingInter(const GeneralMessage &msg,
                                 const NodeInfo &src)
    {
        std::unique_lock<std::mutex> uniq_lck(counter_mtx);
        if (!is_worker(GOVERNING))
            return;
        PingInterMessage pim(msg.get_constant_content_buf());
        pim.verifyMultisig(msg.get_seq_num());
        int64_t t_send = msg.get_seq_num();
        int64_t t_now = Simulator::Now().GetMicroSeconds();
        int64_t diff = t_now - t_send;

        // NODE_LOG_UNCOND("diff " << diff);

        // NODE_LOG_LOGIC("process inter " << src.to_string() << " at " << t_send);

        if (msg.get_seq_num() + PING_TIMEOUT * MILLI_TO_MICRO <=
            Simulator::Now().GetMicroSeconds())
        {
            NODE_LOG_WARN(msg.get_seq_num());
            return;
        }

        int from_region = src.region_id;
        if (this->ping_current_round.find(from_region) ==
                this->ping_current_round.end() ||
            t_send > this->ping_current_round.at(from_region))
        {
            /* The first message in a round */
#if REPUTATION_SYSTEM
            // NODE_LOG_UNCOND("remote first-time");
            scores.at(src.region_id * MAX_TRAINS + GOVERNING).round_recv(src, node_info);
#endif
            this->ping_current_round[from_region] = t_send;
            this->ping_min_latency[from_region] = diff;
            PingProposeMessage proposal(src,
                                        diff,
                                        pim.getSerializedSize(),
                                        msg.get_constant_content_buf());
            GeneralMessage proposal_msg(PING_PROPOSE,
                                        t_send,
                                        0,
                                        nullptr,
                                        &this->secret_key,
                                        &this->public_key);
            proposal_msg.set_size(proposal.serialize(
                proposal_msg.get_content_buf()));

            /* broadcast the ping propose message */
            NodeInfo bc_dest;
            bc_dest.region_id = -1, bc_dest.node_id = -1;
            this->SendLanMessage(bc_dest, proposal_msg);

            /* Check the PoC, if any */
            uint32_t poc_buf_offset = 0;
            while (poc_buf_offset < pim.m_other_size)
            {
                PoCMessage poc(pim.m_other + poc_buf_offset);
                poc_buf_offset += poc.getSerializedSize();
                /* mark the poc waiting map */
                TaskInRegion tir(poc.task_id, proposal.remote.region_id);
                inter_states[tir] = msg.get_seq_num();
                NS_LOG_LOGIC(node_info.to_string() << "Marked tir" << tir.task << "." << tir.region << " at " << msg.get_seq_num());
            }
            NODE_LOG_LOGIC("process first inter " << src.to_string());
        }
#if REPUTATION_SYSTEM
        else
        {
            /* Update the reputation system and forward the messages */
            PingProposeMessage proposal(src,
                                        diff,
                                        pim.getSerializedSize(),
                                        msg.get_constant_content_buf());
            GeneralMessage proposal_msg(PING_PROPOSE,
                                        t_send,
                                        0,
                                        nullptr,
                                        &this->secret_key,
                                        &this->public_key);
            proposal_msg.set_size(proposal.serialize(
                proposal_msg.get_content_buf()));

            /* broadcast the ping propose message */
            NodeInfo bc_dest;
            bc_dest.region_id = -1, bc_dest.node_id = -1;
            this->SendLanMessage(bc_dest, proposal_msg);

            // NODE_LOG_UNCOND("remote not first-time");
            scores.at(src.region_id * MAX_TRAINS + GOVERNING).round_recv(src, node_info);
        }
#endif
    }

    void
    GeneralApp::ProcessPingPropose(const GeneralMessage &msg, const NodeInfo &src)
    {
        std::unique_lock<std::mutex> uniq_lck(counter_mtx);
        PingProposeMessage ppm(msg.get_constant_content_buf());
        int64_t lat = ppm.latency;
        int64_t t_send = msg.get_seq_num();
        int from_region = ppm.remote.region_id;
        int64_t now = Simulator::Now().GetMicroSeconds();
        NODE_LOG_LOGIC("recv propose by " << src.to_string() << " remote " << ppm.remote.to_string() << " val " << lat);
        if (msg.get_seq_num() + PING_TIMEOUT * MILLI_TO_MICRO <= Simulator::Now().GetMicroSeconds())
            return;
#if REPUTATION_SYSTEM
        // NODE_LOG_UNCOND("local");
        scores.at(ppm.remote.region_id * MAX_TRAINS + GOVERNING).round_recv(ppm.remote, src);
#endif

        /* Make sure the proposal is reasonable */
        if (now - t_send - lat >
            INTRA_REGION_LATENCY_VAR + INTRA_REGION_LATENCY)
        {
            NS_LOG_ERROR("Max allowed " << INTRA_REGION_LATENCY_VAR +
                                               INTRA_REGION_LATENCY);
            NS_LOG_ERROR("Now " << now << "; Proposed latency " << lat);
            NS_LOG_ERROR("Failed at node " << node_info.to_string());
            NS_ASSERT(0);
        }
        if (this->ping_current_round.find(from_region) ==
                this->ping_current_round.end() ||
            t_send > this->ping_current_round.at(from_region))
        {
            this->ping_current_round[from_region] = t_send;
            this->ping_min_latency[from_region] = lat;
            this->ping_accept_evidence[from_region] = msg;
        }
        if (lat < this->ping_min_latency[from_region])
        {
            this->ping_min_latency[from_region] = lat;
        }

        /* Check the PoC, if any */
        PingInterMessage pim(ppm.ping_inter_buf);
        uint32_t poc_buf_offset = 0;
        while (poc_buf_offset < pim.m_other_size)
        {
            PoCMessage poc(pim.m_other + poc_buf_offset);
            poc_buf_offset += poc.getSerializedSize();
            /* mark the poc waiting map */
            TaskInRegion tir(poc.task_id, ppm.remote.region_id);
            inter_states[tir] = msg.get_seq_num();
            NS_LOG_LOGIC(node_info.to_string() << "Marked tir" << tir.task << "." << tir.region << " at " << msg.get_seq_num());
        }
    }

    void
    GeneralApp::SendPingAccept(int remote_rid)
    {
        std::unique_lock<std::mutex> uniq_lck(counter_mtx);
        /* Schedule the next accept message */
        // int64_t next_time = FIND_CLOSEST_ROUND(Simulator::Now().GetMicroSeconds()) + PING_TIMEOUT +
        // PING_INTERVAL - Simulator::Now().GetMicroSeconds();
        Simulator::Schedule(MicroSeconds(PING_INTERVAL), &GeneralApp::SendPingAccept, this, remote_rid);
        this->ping_accepted.clear();

        NodeInfo bc;
        bc.node_id = -1, bc.region_id = -1;

        int offset_id = node_info.region_id == 0 ? remote_rid : node_info.region_id;
        int seq_num_out = CLOSEST_SCHEDULED_TIME(Simulator::Now().GetMicroSeconds() - PING_TIMEOUT, PING_INTERVAL, PING_OFFSET_BY_ID(offset_id));

        if (this->ping_current_round.find(remote_rid) == this->ping_current_round.end() ||
            this->ping_current_round.at(remote_rid) != seq_num_out)
        {
            /* Receive no proposal at all */
            NS_LOG_WARN(Simulator::Now().GetMilliSeconds()
                        << " Node " << this->node_info.to_string() << " timeout between region "
                        << remote_rid);
            return;
        }
        int64_t acc_val = ping_min_latency.at(remote_rid) + INTRA_REGION_LATENCY_VAR;
        PingAcceptMessage pam(remote_rid, acc_val);
        GeneralMessage pam_msg(PING_ACCEPT,
                               this->ping_current_round.at(remote_rid),
                               0,
                               nullptr,
                               &this->secret_key,
                               &this->public_key);
        pam_msg.set_size(pam.serialize(pam_msg.get_content_buf()));

        NS_LOG_UNCOND(Simulator::Now().GetMilliSeconds()
                      << " Node " << this->node_info.to_string() << " accepts latency "
                      << acc_val / 1000 << " ms between region " << remote_rid);

        this->SendLanMessage(bc, pam_msg);
    }

    void
    GeneralApp::ProcessPingAccept(const GeneralMessage &msg, const NodeInfo &src)
    {
        std::unique_lock<std::mutex> uniq_lck(counter_mtx);
        PingAcceptMessage pam(msg.get_constant_content_buf());
        int64_t me_accepted = this->ping_min_latency.at(pam.region) + INTRA_REGION_LATENCY_VAR;
        if (me_accepted != pam.latency)
            NODE_LOG_ERROR(me_accepted << " " << pam.latency);
        NS_ASSERT(me_accepted == pam.latency);
        if (this->ping_accepted.find(pam.region) == this->ping_accepted.end())
        {
            std::vector<GeneralMessage> vec;
            vec.emplace_back(msg);
            this->ping_accepted.insert(std::make_pair(pam.region, vec));
        }
        else
        {
            this->ping_accepted[pam.region].emplace_back(msg);
        }
    }

    void GeneralApp::CheckForPoC(const TaskInRegion &tir, int64_t seq_num)
    {
        /** @note In practice, we need to replace the assertion by accusation */
        NS_LOG_LOGIC(node_info.to_string() << " Checking tir==" << tir.task << "." << tir.region << " expects " << seq_num);
        NS_ASSERT(inter_states.find(tir) != inter_states.end());
        NS_ASSERT(inter_states.at(tir) >= seq_num);
    }

    void GeneralApp::ProcessPoC(const GeneralMessage &msg,
                                const NodeInfo &src)
    {
        NS_LOG_LOGIC("Node " << node_info.to_string() << " gets PoC");
        PoCMessage poc(msg.get_constant_content_buf());
        int64_t poc_seq = msg.get_seq_num();
        // int64_t next_ping_time =
        //     PING_INTERVAL +
        //     CLOSEST_SCHEDULED_TIME(poc_seq, PING_INTERVAL, 0);
        // NS_LOG_LOGIC(Simulator::Now().GetMicroSeconds() << " " << next_ping_time);

        int which_region = poc.task_id / MAX_TRAINS;
        int offset_id = which_region == 0 ? node_info.region_id : which_region;
        int64_t next_ping_time = PING_INTERVAL +
                                 CLOSEST_SCHEDULED_TIME(poc_seq, PING_INTERVAL,
                                                        PING_OFFSET_BY_ID(offset_id));

        if (poc_to_include.at(which_region).find(next_ping_time) == poc_to_include.at(which_region).end())
        {
            std::vector<PoCMessage> vec;
            vec.emplace_back(std::move(poc));
            poc_to_include.at(which_region).insert(std::make_pair(next_ping_time, vec));
        }
        else
        {
            poc_to_include.at(which_region)[next_ping_time].emplace_back(std::move(poc));
        }
    }

} // namespace ns3