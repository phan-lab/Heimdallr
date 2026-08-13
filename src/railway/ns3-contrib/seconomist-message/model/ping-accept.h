#ifndef _PING_ACCEPT_H_
#define _PING_ACCEPT_H_

#include "msg_def.h"

#include "ns3/MultiSigObj.h"

#include <stdint.h>
#include <string.h>

namespace ns3
{

  /**
   * @brief The nodes accept the proposed latency
   * @note Format: The region id (4 bytes) ||
   * accepted latency (8bytes)
   */
  class PingAcceptMessage
  {
  public:
    int region;
    int64_t latency;

    /** @brief deserialize from buffer */
    PingAcceptMessage(uint8_t *buf)
    {
      uint64_t offset = 0;
      memcpy(&region, buf + offset, sizeof(region));
      offset += sizeof(region);
      memcpy(&latency, buf + offset, sizeof(latency));
      offset += sizeof(latency);
    }

    /** @brief construct by values */
    PingAcceptMessage(int region, int64_t latency)
        : region(region),
          latency(latency)
    {
    }

    /** @brief default constructor */
    PingAcceptMessage()
        : region(0),
          latency(0)
    {
    }

    uint32_t serialize(uint8_t *dest) const
    {
      uint32_t offset = 0;
      memcpy(dest + offset, &region, sizeof(region));
      offset += sizeof(region);
      memcpy(dest + offset, &latency, sizeof(latency));
      offset += sizeof(latency);
      return offset;
    }
  };
}; // namespace ns3

#endif