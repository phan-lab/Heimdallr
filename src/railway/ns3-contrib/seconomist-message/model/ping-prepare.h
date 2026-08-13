#ifndef _PING_PREPARE_H_
#define _PING_PREPARE_H_

#include "msg_def.h"

#include <stdint.h>
#include <string.h>

namespace ns3
{
    /**
     * @brief Measurers send their signed current
     * round number and other state information.
     *
     * @note Format: dest_region ||  m_other_size
     * (4 bytes) || m_other
     */
    class PingPrepareMessage
    {
    public:
        int dest_region;
        uint32_t m_other_size;
        uint8_t m_other[MAXBUF];

        /** @brief deserialize from buffer */
        PingPrepareMessage(uint8_t *buf)
        {
            uint64_t offset = 0;
            memcpy(&dest_region, buf + offset, sizeof(dest_region));
            offset += sizeof(dest_region);
            memcpy(&m_other_size, buf + offset, sizeof(m_other_size));
            if (m_other_size == 0)
                return;
            offset += sizeof(m_other_size);
            memcpy(m_other, buf + offset, m_other_size);
        }

        /** @brief construct by values */
        PingPrepareMessage(int region, uint32_t m_other_size, uint8_t *m_other)
            : dest_region(region), m_other_size(m_other_size)
        {
            if (m_other_size > 0)
                memcpy(this->m_other, m_other, m_other_size);
        }

        /** @brief default constructor */
        PingPrepareMessage()
            : dest_region(0), m_other_size(0)
        {
            memset(m_other, 0, MAXBUF);
        }

        uint32_t serialize(uint8_t *dest) const
        {
            uint32_t offset = 0;
            memcpy(dest + offset, &dest_region, sizeof(dest_region));
            offset += sizeof(dest_region);
            memcpy(dest + offset, &m_other_size, sizeof(m_other_size));
            offset += sizeof(m_other_size);
            memcpy(dest + offset, m_other, m_other_size);
            offset += m_other_size;
            return offset;
        }
    };
}; // namespace ns3

#endif