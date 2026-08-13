#ifndef _PING_PROPOSE_H_
#define _PING_PROPOSE_H_

#include "msg_def.h"

#include "ns3/MultiSigObj.h"


#include <stdint.h>
#include <string.h>

namespace ns3
{

/**
 * @brief The downstream measurers propose the latency
 * value based on when they receive the message.
 * @note Format: which_region (4 bytes) || proposed_latency(8 bytes)
 * || sizeof original msg
 * || The original PING_INTER message
 */
class PingProposeMessage
{
  public:
    NodeInfo remote;
    int64_t latency;
    uint32_t size_of_ping_inter;
    uint8_t ping_inter_buf[MAXBUF];

    /** @brief deserialize from buffer */
    PingProposeMessage(uint8_t* buf)
    {
        uint64_t offset = 0;
        memcpy(&remote, buf + offset, sizeof(remote));
        offset += sizeof(remote);
        memcpy(&latency, buf + offset, sizeof(latency));
        offset += sizeof(latency);
        memcpy(&size_of_ping_inter, buf + offset, sizeof(size_of_ping_inter));
        offset += sizeof(size_of_ping_inter);
        memcpy(ping_inter_buf, buf + offset, size_of_ping_inter);
    }

    /** @brief construct by values */
    PingProposeMessage(NodeInfo remote,
                       int64_t latency,
                       uint32_t size_of_ping_inter,
                       uint8_t* ping_inter_buf)
        : remote(remote),
          latency(latency),
          size_of_ping_inter(size_of_ping_inter)
    {
        memcpy(this->ping_inter_buf, ping_inter_buf, size_of_ping_inter);
    }

    /** @brief default constructor */
    PingProposeMessage()
        : remote({0,0}),
          latency(0),
          size_of_ping_inter(0)
    {
        memset(ping_inter_buf, 0, MAXBUF);
    }

    uint32_t serialize(uint8_t* dest) const
    {
        uint32_t offset = 0;
        memcpy(dest + offset, &remote, sizeof(remote));
        offset += sizeof(remote);
        memcpy(dest + offset, &latency, sizeof(latency));
        offset += sizeof(latency);
        memcpy(dest + offset, &size_of_ping_inter, sizeof(size_of_ping_inter));
        offset += sizeof(size_of_ping_inter);
        memcpy(dest + offset, ping_inter_buf, size_of_ping_inter);
        offset += size_of_ping_inter;
        return offset;
    }
};
}; // namespace ns3

#endif