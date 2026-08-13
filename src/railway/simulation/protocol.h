#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include "general_app.h"

#include "ns3/core-module.h"

namespace ns3
{
class Protocol
{
  public:
    /**
     * @brief The upstream primary calls this function to send input to
     * the upstream backups
     * @param app The upstream parent
     * @param seq_num The sequence number
     * @param task The task to execute
     * @param msgtype The type of input sharing message
     * @param input The input to send
     * @param size Size of input
     */
    void upstream_prim_init(GeneralApp* app,
                            uint64_t seq_num,
                            int task,
                            int msgtype,
                            const uint8_t* input,
                            uint32_t size);

    /**
     * @brief The upstream primary calls this function to send output
     * of the task to the downstream primary
     * @param app The upstream parent
     * @param uptask The task to execute
     * @param downtask The downstream task that needs the output. The
     * upstream primary needs this info to find the destination to send.
     * @param msgtype The type of the output message
     * @param output Ther output to send
     * @param size Size of output
     */
    void upstream_prim_send(GeneralApp* app,
                            uint64_t seq_num,
                            int uptask,
                            int downtask,
                            int msgtype,
                            const uint8_t* output,
                            uint32_t size);

    /**
     * @brief The downstream primary calls this to process output from
     * the upstream primary
     * @param app The downstream primary
     * @param up_task The task that the upstream nodes are working on
     * @param down_task The task that the nodes need the output of `uptask`
     * for. The downstream parent needs this info to find backups.
     * @param msg The message received
     * @param back_type The type of authenticator messages that will be sent
     * back to the upstream backups
     */
    void downstream_prim_recv(GeneralApp* app,
                              int up_task,
                              int down_task,
                              const GeneralMessage& msg,
                              int back_type);
};
} // namespace ns3
#endif