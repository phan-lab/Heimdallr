#ifndef _STATION_APP_H_
#define _STATION_APP_H_

#include "general_app.h"

#include <set>
#include <vector>
#include <map>
#include <array>

namespace ns3
{
    class StationApp : public GeneralApp
    {
    protected:
        virtual void StartApplication();
#if USE_TCP
        void ProcessAccept(Ptr<Socket> socket, const Address &address);
        void HandlePeerClose(Ptr<Socket> socket);
        void HandlePeerError(Ptr<Socket> socket);
        std::map<NodeInfo, Ptr<Socket>> m_connections;
        virtual void SendLteMessage(const NodeInfo &dest, const GeneralMessage &msg);
#endif
        void ProcessMessage(const GeneralMessage &msg, const NodeInfo &src);
        void ProcessControlUpdate(const GeneralMessage &msg, const NodeInfo &src);
        void ProcessControlUpdatePeers(const GeneralMessage &msg, const NodeInfo &src);
        void ComputeMA(int train_id);

        /* Local information for control applications */
        std::map<int, double> train_locations;
        std::map<int, double> train_speed;
        std::map<int, std::array<uint8_t, MULTISIG_SIG_LENGTH>> train_sigs;

    public:
        int max_train_id;
        StationApp() = delete;

        StationApp(int rid, int nid)
            : GeneralApp(rid, nid)
        {
            this->type = STATION;
        }

        virtual void ProcessLanMsg(Ptr<Socket> socket);
        virtual void ProcessWirelessMsg(Ptr<Socket> socket);
    };
} // namespace ns3

#endif