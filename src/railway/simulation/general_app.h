#ifndef _GENERAL_APP_H_
#define _GENERAL_APP_H_

#include "defs.h"
#include "reputation.h"

#include "ns3/secure-log.h"
#include "ns3/application.h"
#include "ns3/core-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/seconomist-message.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"

#include <mutex>
#include <atomic>

#define LAN_PORT (12306)
#define LTE_PORT (1234)

#define CLOSEST_SCHEDULED_TIME(cur, interval, offset) \
    (round((round(((double)(cur) - (double)offset) / (double)interval) * interval)) + offset)

#define NODE_LOG_UNCOND(msg) NS_LOG_UNCOND(node_info.to_string() << " at " << Simulator::Now().GetMilliSeconds() << "||| " << msg);
#define NODE_LOG_LOGIC(msg) NS_LOG_LOGIC(node_info.to_string() << " at " << Simulator::Now().GetMilliSeconds() << "||| " << msg);
#define NODE_LOG_ERROR(msg) NS_LOG_ERROR(node_info.to_string() << " at " << Simulator::Now().GetMilliSeconds() << "||| " << msg);
#define NODE_LOG_WARN(msg) NS_LOG_WARN(node_info.to_string() << " at " << Simulator::Now().GetMilliSeconds() << "||| " << msg);
namespace ns3
{
    enum InterState
    {
        NORMAL,
        WAIT_FOR_POC,
        SAFE_MODE,
    };
    class TrainApp;
    class StationApp;
    class GeneralApp : public Application
    {
        friend class TrainApp;
        friend class StationApp;
        friend class Protocol;

    protected:
        NodeInfo node_info; //!< The region ID and node ID
        NodeType type;      //!< Train or Station

        int seq = 0;
        MultiSigObj secret_key;

        Ptr<Socket> m_lan_socket;
        Ptr<Socket> m_lte_socket;
        Ipv4Address m_address;
        Ipv4Address m_wirelessAddr;

        std::map<NodeInfo, Ipv4Address> m_lan_id2ip_map;
        std::map<Ipv4Address, NodeInfo> m_lan_ip2id_map;

        std::map<NodeInfo, Ipv4Address> m_lte_id2ip_map;
        std::map<Ipv4Address, NodeInfo> m_lte_ip2id_map;

        std::map<NodeInfo, MultiSigObj *> m_pk_map;

        virtual void ProcessMessage(const GeneralMessage &msg, const NodeInfo &src) = 0;

        /** @brief send LAN messages. Set dest to -1.-1 for broadcast */
        void SendLanMessage(const NodeInfo &dest, const GeneralMessage &msg);
        virtual void SendLteMessage(const NodeInfo &dest, const GeneralMessage &msg);
        virtual void SendLteMessageReal(const NodeInfo &dest, const GeneralMessage &msg);

        void SendPingPrepare(int dest_region);

        virtual void StartApplication();

        int GetNextSequenceNumber();

        /* local info for ping messages */
        void ProcessPingPrepare(const GeneralMessage &msg);
        void ProcessPingInter(const GeneralMessage &msg, const NodeInfo &src);
        void ProcessPingPropose(const GeneralMessage &msg, const NodeInfo &src);
        void SendPingAccept(int remote_rid);
        void ProcessPingAccept(const GeneralMessage &msg, const NodeInfo &src);
        std::mutex counter_mtx;
        uint8_t ping_prepare_sigs[MAX_TRAINS][F_MAX + 1][MULTISIG_SIG_LENGTH]; 
        uint8_t ping_prepare_pubs[MAX_TRAINS][F_MAX + 1][MULTISIG_PUB_LENGTH]; 
        // std::array<std::atomic<int>, MAX_TRAINS> ping_prepare_num_sigs;
        int ping_prepare_num_sigs[MAX_TRAINS];

        //!< Equal to t_send in the PING_INTER messages. Each region has an entry.
        std::map<int, int64_t> ping_current_round;
        //!< The minimum latency so far
        std::map<int, int64_t> ping_min_latency;
        //!< To record if the measurer has proposed the latency for the current round.
        std::map<int, bool> ping_proposed;

        //!< Store the latency that other nodes accepted as evidence
        std::map<int, std::vector<GeneralMessage>> ping_accepted;
        //!< The evidence to justify the acceptance
        std::map<int, GeneralMessage> ping_accept_evidence;

        /* Local info for task and schedule information */
        std::map<TaskInRegion, Schedule> schedules;
        std::map<int, Task> tasks;
        std::map<KnownFault, std::string> mode_change_map;
        bool is_worker(int task);
        bool is_courier(int task);
        bool is_primary(int task);

        /**
         * @brief The state of inter-region tasks.
         * Used to check if PoC arrives in time.
         *
         * @note The key is the tuple (task, rid).
         * The value is the largest sequence number of the messages
         * validated by a PoC.
         *
         * @note This map should be updated once a PoC is received,
         * and checked at deadlines/timeouts.
         */
        std::map<TaskInRegion, int64_t> inter_states;

        /**
         * @brief
         * To avoid duplicate schedules, store the latest sequence
         * number of messages that we've already scheduled a PoC check.
         */
        std::map<TaskInRegion, int64_t> scheduled_check;

        /**
         * @brief The PoCs that the measurers need to include in the
         * future ping messages.
         * @note The key is the sequence number of the ping message that
         * should include the PoC-Message. The value is the PoC message
         * itself.
         */
        std::array<std::map<int64_t, std::vector<PoCMessage>>, MAX_TRAINS + 1> poc_to_include;
        void ProcessPoC(const GeneralMessage &msg, const NodeInfo &src);

        SecureLog local_log;

#if REPUTATION_SYSTEM
        std::map<int, CreditScores> scores;
#endif


    public:
        MultiSigObj public_key;
        GeneralApp() = delete;

        GeneralApp(int rid, int nid)
        {
            this->node_info.region_id = rid;
            this->node_info.node_id = nid;
        }

        inline NodeInfo get_node_info() const
        {
            return node_info;
        }

        void setMainAddress(Ipv4Address addr)
        {
            m_address = addr;
        }

        void setWirelessAddress(Ipv4Address addr)
        {
            m_wirelessAddr = addr;
        }

        void installKeys(MultiSigObj &pk, MultiSigObj &sk);

        void installTaskSchedules(std::map<TaskInRegion, Schedule> &schedules,
                                  std::map<int, Task> &tasks,
                                  std::map<KnownFault, std::string> &mode_change_map);

        void installKeyMap(const std::map<NodeInfo, MultiSigObj *> &map)
        {
            this->m_pk_map = map;
        }

        void setLanMapping(const std::map<NodeInfo, Ipv4Address> &id2ip,
                           const std::map<Ipv4Address, NodeInfo> &ip2id);
        void setLteMapping(const std::map<NodeInfo, Ipv4Address> &id2ip,
                           const std::map<Ipv4Address, NodeInfo> &ip2id);
        virtual void ProcessLanMsg(Ptr<Socket> socket) = 0;
        virtual void ProcessWirelessMsg(Ptr<Socket> socket) = 0;

        void CheckForPoC(const TaskInRegion &tir, int64_t seq_num);
    };
} // namespace ns3

#endif