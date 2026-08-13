#include "protocol.h"

namespace ns3
{
void
Protocol::upstream_prim_init(GeneralApp* app,
                             uint64_t seq_num,
                             int task,
                             int msgtype,
                             const uint8_t* input,
                             uint32_t size)
{
    /* The upstream node sends the input to its backups */
    int rid = app->get_node_info().region_id;
    GeneralMessage msg(msgtype, seq_num, size, input, &app->secret_key, &app->public_key);
    for (const auto& nid : app->schedules.at(TaskInRegion(task, rid)).workers)
    {
        if (nid == app->get_node_info().node_id)
            continue;

        app->SendLanMessage({rid, nid}, msg);
    }
}

void
Protocol::upstream_prim_send(GeneralApp* app,
                             uint64_t seq_num,
                             int up_task,
                             int down_task,
                             int msgtype,
                             const uint8_t* output,
                             uint32_t size)
{
    const auto& down_sche =
        app->schedules.at(TaskInRegion(down_task, app->get_node_info().region_id));
    GeneralMessage msg(msgtype, seq_num, size, output, &app->secret_key, &app->public_key);
    msg.set_sign_hash(true);
    NodeInfo dest = {app->get_node_info().region_id, down_sche.primary};
    app->SendLanMessage(dest, msg);
}

void
Protocol::downstream_prim_recv(GeneralApp* app,
                               int up_task,
                               int down_task,
                               const GeneralMessage& msg,
                               int back_type)
{
    /* It should forward the results to its backups */
    const auto& up_sche = app->schedules.at(TaskInRegion(up_task, app->get_node_info().region_id));
    const auto& down_sche =
        app->schedules.at(TaskInRegion(down_task, app->get_node_info().region_id));

    uint8_t new_content[SHA256_DIGEST_LENGTH + MULTISIG_SIG_LENGTH] = {0};
    SHA256(msg.get_constant_content_buf(), msg.get_size(), new_content);
    memcpy(new_content + SHA256_DIGEST_LENGTH, msg.get_constant_sig_buf(), MULTISIG_SIG_LENGTH);

    /* Forward output hash and sigs to downstream backups */
    GeneralMessage msg_to_backup(msg.get_type(),
                                     msg.get_seq_num(),
                                     SHA256_DIGEST_LENGTH + MULTISIG_SIG_LENGTH,
                                     new_content,
                                     &app->secret_key,
                                     &app->public_key);
    for (const auto & dest: down_sche.workers)
    {
        if (dest == app->get_node_info().node_id)
            continue;
        app->SendLanMessage({app->get_node_info().region_id, dest}, msg_to_backup);
    }

    /* Forward output hash and sigs to upstream backups */
    GeneralMessage msg_to_upstream(back_type, msg.get_seq_num(),
                                     SHA256_DIGEST_LENGTH + MULTISIG_SIG_LENGTH,
                                     new_content,
                                     &app->secret_key,
                                     &app->public_key);
    for (const auto & dest: up_sche.workers)
    {
        if (dest == up_sche.primary)
            continue;
        app->SendLanMessage({app->get_node_info().region_id, dest}, msg_to_backup);
    }

}

} // namespace ns3