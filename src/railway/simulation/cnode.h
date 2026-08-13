#ifndef _CNODE_H_
#define _CNODE_H_

#include "defs.h"

#include "ns3/core-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-global-routing.h"


namespace ns3
{

  class CNode : public Node
  {
  public:
    NodeInfo node_info; //!< The region ID and node ID
    NodeType type;      //!< Train or Station
    CNode();

  private:
    Ptr<Socket> m_socket;
    
  };

}; // namespace ns3

#endif