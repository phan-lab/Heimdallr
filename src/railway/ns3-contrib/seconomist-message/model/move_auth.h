#ifndef _MOVE_AUTH_H_
#define _MOVE_AUTH_H_

#include "msg_def.h"

#include <stdint.h>
#include <string.h>
namespace ns3
{
    /**
     * @brief The station couriers send the MA to trains
     * @note Format: train id (4 bytes) || Location (8 bytes) ||
     *               speed (8 bytes)
     */
    class MoveAuthMsg
    {
    public:
        int which_train;
        double front_location;
        double front_speed;
        MoveAuthMsg(uint8_t *buf)
        {
            int pos = 0;
            memcpy(&which_train, buf + pos, sizeof(which_train));
            pos += sizeof(which_train);
            memcpy(&front_location, buf + pos, sizeof(front_location));
            pos += sizeof(front_location);
            memcpy(&front_speed, buf + pos, sizeof(front_speed));
        }

        MoveAuthMsg(int train, double location, double speed)
        {
            which_train = train;
            front_location = location;
            front_speed = speed;
        }

        uint32_t serialize(uint8_t *buf)
        {
            uint32_t pos = 0;
            memcpy(buf + pos, &which_train, sizeof(which_train));
            pos += sizeof(which_train);
            memcpy(buf + pos, &front_location, sizeof(front_location));
            pos += sizeof(front_location);
            memcpy(buf + pos, &front_speed, sizeof(front_speed));
            pos += sizeof(front_speed);
            return pos;
        }
    };
};

#endif