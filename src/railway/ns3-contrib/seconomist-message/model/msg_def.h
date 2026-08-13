#ifndef _MSG_DEF_H_
#define _MSG_DEF_H_

#define MAXBUF (65536)
#define USE_CRYPTO (1)

#include "ns3/buffer.h"
#include "ns3/core-module.h"
#include "ns3/tasks.h"

namespace ns3
{
    enum MsgType
    {
        INVALID = 0,

        /**
         * @brief Measurers send their signed current
         * round number and other state information.
         *
         * @note Format:  dest_region (4bytes) || m_other_size
         * (4 bytes) || m_other
         */
        PING_PREPARE,

        /**
         * @brief The upstream measurers send this to downstream
         * measurers.
         * @note Format: dest_region || m_other_size
         * (4 bytes) || m_other || num_sigs (4bytes) || multisig || multikey
         */
        PING_INTER,

        /**
         * @brief The downstream measurers propose the latency
         * value based on when they receive the message.
         * @note Format: which_region (4 bytes) || proposed_latency(8 bytes)
         * || sizeof original msg
         * || The original PING_INTER message
         */
        PING_PROPOSE,

        /**
         * @brief The nodes accept the proposed latency
         * @note Format: The region id (4 bytes) ||
         * accepted latency (8bytes)
         */
        PING_ACCEPT,

        // CALC_SPEED_INPUT,
        // CALC_SPEED_OUTPUT,
        // CALC_SPEED_HASH,

        /**
         * @brief The station couriers send the MA to trains
         * @note Format: train id (4 bytes) || Location (8 bytes) ||
         *               speed (8 bytes)
         */
        STATION_MA_TO_TRAIN,

        /**
         * @brief The station couriers share the data with their peers
         * @note Format: trainNodeInfo (8 bytes) || Location (8 bytes) ||
         *               speed (8 bytes) || signature (SIG_SIZE)
         */
        STATION_SEND_TRAIN_STATE_TO_PEERS,

        /**
         * @brief The primary sends the data to backups.
         * @note Format: Location (8 bytes) || speed (8 bytes) ||
         *               sensor's sig (SIG_SIZE)
         */
        CONTROL_UPDATE_SPEED_LOCAL,

        /**
         * @brief The train primary sends the data to station primary.
         * @note Format: Location (8 bytes) || speed (8 bytes)
         */
        CONTROL_UPDATE_SPEED_TO_STATION,

        /**
         * @brief The train couriers forward MA to peers
         * @note Format: Location (8 bytes) ||
         *               speed (8 bytes) || sig
         */
        CONTROL_MA_FWD,

        /**
         * @brief The train couriers forward MA to peers
         * @note Format: Location (8 bytes) ||
         *               speed (8 bytes) || sig
         */
        CONTROL_MA_AS_INPUT,

        /**
         * @brief The target speed sent to the actuator
         * @note Format: speed (8 bytes)
         */
        CONTROL_SPEED_TO_ACTUATOR,

        /**
         * @brief The location and speed that the sensor collects.
         * @note Format: Location (8 bytes) || speed (8 bytes)
         */
        SENSOR_LOCATION_SPEED,

        /**
         * @brief Proof of correctness
         * @note Format: TaskID (4 bytes) || PoC-Size (4 bytes) ||
         *               PoC-Contents (Poc-Size bytes)
         */
        POC,

        POM,
        LFD,
    };
}; // namespace ns3

#endif