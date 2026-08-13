#ifndef _LOCATION_SPEED_PEER_H_
#define _LOCATION_SPEED_PEER_H_

#include "msg_def.h"

#include "ns3/MultiSigObj.h"

#include <stdint.h>
#include <string.h>

namespace ns3
{
    /**
     * @brief The station couriers share the data with their peers
     * @note Format: senderInfo (8 bytes) || Location (8 bytes) ||
     *               speed (8 bytes) || signature (SIG_SIZE)
     */
    class LocationSpeedPeer
    {
    public:
        NodeInfo train_node;
        double location;
        double speed;
        uint8_t signature[MULTISIG_SIG_LENGTH];

        /** @brief deserialize from buffer */
        LocationSpeedPeer(uint8_t *buf)
        {
            int pos = 0;
            memcpy(&train_node, buf + pos, sizeof(train_node));
            pos += sizeof(train_node);
            memcpy(&location, buf + pos, sizeof(location));
            pos += sizeof(location);
            memcpy(&speed, buf + pos, sizeof(speed));
            pos += sizeof(speed);
            memcpy(signature, buf + pos, sizeof(signature));
        }

        /** @brief construct by values */
        LocationSpeedPeer(NodeInfo train_node, double location,
                          double speed, uint8_t *sig)
            : train_node(train_node),
              location(location),
              speed(speed)
        {
            memcpy(signature, sig, MULTISIG_SIG_LENGTH);
        }

        /** @brief default constructor */
        LocationSpeedPeer()
            : train_node({0,0}),
              location(0),
              speed(0)
        {
            memset(signature, 0, MULTISIG_SIG_LENGTH);
        }

        uint32_t serialize(uint8_t *buf) const
        {
            int pos = 0;
            memcpy(buf + pos, &train_node, sizeof(train_node));
            pos += sizeof(train_node);
            memcpy(buf + pos, &location, sizeof(location));
            pos += sizeof(location);
            memcpy(buf + pos, &speed, sizeof(speed));
            pos += sizeof(speed);
            memcpy(buf + pos, signature, sizeof(signature));
            pos += sizeof(signature);
            return pos;
        }
    };
};

#endif