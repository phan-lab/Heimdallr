#ifndef _TRAIN_H_
#define _TRAIN_H_

#include "cnode.h"
#include "train_app.h"

#include "ns3/core-module.h"

namespace ns3
{

    class Train : public CNode
    {
    public:
        Train(int rid, int nid) : CNode()
        {
            this->node_info.region_id = rid;
            this->node_info.node_id = nid;
            this->type = TRAIN;
        }
    };

}; // namespace ns3

#endif