#ifndef _MOVE_AUTH_FWD_H_
#define _MOVE_AUTH_FWD_H_

#include "msg_def.h"
#include "ns3/MultiSigObj.h"

#include <stdint.h>
#include <string.h>
namespace ns3
{
    /**
     * @brief The train couriers forward MA to peers
     * @note Format: issued_node (4 bytes) || Location (8 bytes) ||
     *               speed (8 bytes) || sig
     */
    class MoveAuthFwd
    {
    public:
        int issued_node;
        double front_location;
        double front_speed;
        uint8_t signature[MULTISIG_SIG_LENGTH];
        MoveAuthFwd(uint8_t *buf)
        {
            uint32_t pos = 0;
            std::memcpy(&issued_node, buf + pos, sizeof(issued_node));
            pos += sizeof(issued_node);
            std::memcpy(&front_location, buf + pos, sizeof(front_location));
            pos += sizeof(front_location);
            std::memcpy(&front_speed, buf + pos, sizeof(front_speed));
            pos += sizeof(front_speed);
            std::memcpy(signature, buf + pos, sizeof(signature));
        }

        MoveAuthFwd(int node, double location, double speed, uint8_t *sig)
        {
            issued_node = node;
            front_location = location;
            front_speed = speed;
            memcpy(signature, sig, MULTISIG_SIG_LENGTH);
        }

        uint32_t serialize(uint8_t *buf)
        {
            uint32_t pos = 0;
            std::memcpy(buf + pos, &issued_node, sizeof(issued_node));
            pos += sizeof(issued_node);
            std::memcpy(buf + pos, &front_location, sizeof(front_location));
            pos += sizeof(front_location);
            std::memcpy(buf + pos, &front_speed, sizeof(front_speed));
            pos += sizeof(front_speed);
            std::memcpy(buf + pos, signature, sizeof(signature));
            pos += sizeof(signature);
            return pos;
        }
    };
};

#endif