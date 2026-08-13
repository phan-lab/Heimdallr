#ifndef _STATION_H_
#define _STATION_H_

#include "cnode.h"
#include "station_app.h"

#include "ns3/core-module.h"

namespace ns3
{

    class Station : public CNode
    {
    public:
        Station(int rid, int nid): CNode()
        {
            this->node_info.region_id = rid;
            this->node_info.node_id = nid;
            this->type = STATION;
        }
    };

}; // namespace ns3
#endif