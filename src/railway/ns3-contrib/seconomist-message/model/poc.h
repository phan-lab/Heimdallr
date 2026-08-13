#ifndef _POC_H_
#define _POC_H_

#include "msg_def.h"

#include "ns3/MultiSigObj.h"

#include <stdint.h>
#include <string.h>

namespace ns3
{
    /**
     * @brief Proof of correctness
     * @note Format: TaskID (4 bytes) || PoC-Size (4 bytes) ||
     *               PoC-Contents (Poc-Size bytes)
     */
    class PoCMessage
    {
    public:
        int task_id;
        uint32_t poc_size;
        uint8_t poc_contents[MAXBUF];

        /** @brief deserialize from buffer */
        PoCMessage(uint8_t *buf)
        {
            uint32_t offset = 0;
            memcpy(&task_id, buf + offset, sizeof(task_id));
            offset += sizeof(task_id);
            memcpy(&poc_size, buf + offset, sizeof(poc_size));
            offset += sizeof(poc_size);
            memcpy(poc_contents, buf + offset, poc_size);
        }

        PoCMessage(int task_id, uint32_t poc_size, uint8_t *poc_contents)
            : task_id(task_id), poc_size(poc_size)
        {
            memcpy(this->poc_contents, poc_contents, poc_size);
        }

        PoCMessage() : task_id(-1), poc_size(0) {}

        uint32_t serialize(uint8_t *dest) const
        {
            uint32_t offset = 0;
            memcpy(dest + offset, &task_id, sizeof(task_id));
            offset += sizeof(task_id);
            memcpy(dest + offset, &poc_size, sizeof(poc_size));
            offset += sizeof(poc_size);
            memcpy(dest + offset, poc_contents, poc_size);
            offset += poc_size;
            return poc_size;
        }

        uint32_t getSerializedSize() const
        {
            uint32_t offset = 0;
            offset += sizeof(task_id);
            offset += sizeof(poc_size);
            offset += poc_size;
            return poc_size;
        }
    };
}

#endif