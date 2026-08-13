#ifndef _PING_INTER_H_
#define _PING_INTER_H_

#include "msg_def.h"

#include "ns3/MultiSigObj.h"
#include "ping-prepare.h"

#include <stdint.h>
#include <string.h>

namespace ns3
{
    /**
     * @brief The upstream measurers send this to downstream
     * measurers.
     * @note Format: dest_region || m_other_size
     * (4 bytes) || m_other || num_sigs (4bytes) || multisig || multikey
     */

    class PingInterMessage
    {
    public:
        int dest_region;
        uint32_t m_other_size;
        uint8_t m_other[MAXBUF];
        uint32_t num_sigs;
        // uint8_t all_sigs[MAXBUF];
        uint8_t multisig[MULTISIG_SIG_LENGTH];
        uint8_t multikey[MULTISIG_PUB_LENGTH];

        /** @brief deserialize from buffer */
        PingInterMessage(uint8_t *buf)
        {
            uint64_t offset = 0;
            memcpy(&dest_region, buf + offset, sizeof(dest_region));
            offset += sizeof(dest_region);

            memcpy(&m_other_size, buf + offset, sizeof(m_other_size));
            offset += sizeof(m_other_size);
            if (m_other_size > 0)
            {
                memcpy(m_other, buf + offset, m_other_size);
                offset += m_other_size;
            }

            memcpy(&num_sigs, buf + offset, sizeof(num_sigs));
            offset += sizeof(num_sigs);

            if (num_sigs > 0)
            {

                memcpy(multisig, buf + offset, MULTISIG_SIG_LENGTH);
                // NS_LOG_UNCOND("Deserialize");
                // print_bytes((unsigned char *)multisig, MULTISIG_SIG_LENGTH);
                offset += MULTISIG_SIG_LENGTH;
                memcpy(multikey, buf + offset, MULTISIG_PUB_LENGTH);
                // print_bytes((unsigned char *)multikey, MULTISIG_PUB_LENGTH);
                offset += MULTISIG_PUB_LENGTH;
            }
        }

        /** @brief construct by values */
        PingInterMessage(int region,
                         uint32_t m_other_size,
                         uint8_t *m_other,
                         uint32_t num_sigs,
                         uint8_t *public_keys,
                         uint8_t *sigbuf)
            : dest_region(region),
              m_other_size(m_other_size),
              num_sigs(num_sigs)
        {
            if (m_other_size > 0)
                memcpy(this->m_other, m_other, m_other_size);
            if (num_sigs > 0)
            {
                MultiSigObj combined_keys;
                MultiSigObj combined_sigs;
                MSObj_init_pub(combined_keys);
                MSObj_init_sig(combined_sigs);

                MultiSigObj sigs[num_sigs];
                MultiSigObj keys[num_sigs];
                for (uint32_t i = 0; i < num_sigs; i++)
                {
                    MSObj_init_pub(keys[i]);
                    MSObj_init_sig(sigs[i]);
                    MSObj_from_buf(
                        keys[i],
                        (char *)public_keys + i * MULTISIG_PUB_LENGTH);
                    MSObj_from_buf(
                        sigs[i], (char *)sigbuf + i * MULTISIG_SIG_LENGTH);
                    // print_bytes((unsigned char *)sigbuf + i * MULTISIG_SIG_LENGTH, MULTISIG_SIG_LENGTH);
                    // print_bytes((unsigned char *)public_keys + i * MULTISIG_PUB_LENGTH, MULTISIG_PUB_LENGTH);
                }
                MSObj_combine_keys(combined_keys, keys, num_sigs);
                MSObj_combine_sigs(combined_sigs, sigs, num_sigs);
                MSObj_to_buf(combined_sigs, (char *)multisig);
                MSObj_to_buf(combined_keys, (char *)multikey);
                // NS_LOG_UNCOND("Create " << num_sigs);
                // print_bytes((unsigned char *)multisig, MULTISIG_SIG_LENGTH);
                // print_bytes((unsigned char *)multikey, MULTISIG_PUB_LENGTH);
            }
        }

        // /** @brief default constructor */
        // PingInterMessage()
        //     : dest_region(0),
        //       m_other_size(0),
        //       num_sigs(0)
        // {
        //     memset(m_other, 0, MAXBUF);
        //     memset(all_sigs, 0, MAXBUF);
        // }

        uint32_t serialize(uint8_t *dest) const
        {
            uint32_t offset = 0;
            memcpy(dest + offset, &dest_region, sizeof(dest_region));
            offset += sizeof(dest_region);

            memcpy(dest + offset, &m_other_size, sizeof(m_other_size));
            offset += sizeof(m_other_size);
            if (m_other_size > 0)
            {
                memcpy(dest + offset, m_other, m_other_size);
                offset += m_other_size;
            }

            memcpy(dest + offset, &num_sigs, sizeof(num_sigs));
            offset += sizeof(num_sigs);

            if (num_sigs > 0)
            {
                // NS_LOG_UNCOND("Serialize");
                // print_bytes((unsigned char *)multisig, MULTISIG_SIG_LENGTH);
                memcpy(dest + offset, multisig, MULTISIG_SIG_LENGTH);
                offset += MULTISIG_SIG_LENGTH;
                // print_bytes((unsigned char *)multikey, MULTISIG_PUB_LENGTH);
                memcpy(dest + offset, multikey, MULTISIG_PUB_LENGTH);
                offset += MULTISIG_PUB_LENGTH;
            }

            return offset;
        }

        uint64_t getSerializedSize() const
        {
            return sizeof(dest_region) + sizeof(m_other_size) +
                   m_other_size + sizeof(num_sigs) +
                   MULTISIG_SIG_LENGTH + MULTISIG_PUB_LENGTH;
        }

        bool verifyMultisig(int64_t seq_num)
        {
            uint32_t content_sz = sizeof(dest_region) +
                                  sizeof(m_other_size) + m_other_size;
            char buf[MAXBUF];
            uint32_t offset = 0;
            int prepare_type = PING_PREPARE;
            memcpy(buf + offset, &prepare_type, sizeof(prepare_type));
            offset += sizeof(prepare_type);
            memcpy(buf + offset, &seq_num, sizeof(seq_num));
            offset += sizeof(seq_num);
            memcpy(buf + offset, &content_sz, sizeof(content_sz));
            offset += sizeof(content_sz);
            memcpy(buf + offset, &dest_region, sizeof(dest_region));
            offset += sizeof(dest_region);

            memcpy(buf + offset, &m_other_size, sizeof(m_other_size));
            offset += sizeof(m_other_size);
            if (m_other_size > 0)
            {
                memcpy(buf + offset, m_other, m_other_size);
                offset += m_other_size;
            }

            MultiSigObj keyobj, sigobj;
            MSObj_init_sig(sigobj);
            MSObj_init_pub(keyobj);
            MSObj_from_buf(keyobj, (char *)multikey);
            MSObj_from_buf(sigobj, (char *)multisig);
            // NS_LOG_UNCOND("veri size " << offset);
            // NS_LOG_UNCOND("veri " << prepare_type
            //                       << " " << seq_num << " " << content_sz
            //                       << " " << dest_region << " " << m_other_size << " " << num_sigs);
            // print_bytes((unsigned char *)multisig, MULTISIG_SIG_LENGTH);
            // print_bytes((unsigned char *)multikey, MULTISIG_PUB_LENGTH);
            bool ret = MSObj_verify(buf, offset, keyobj, sigobj);
            NS_ASSERT(ret);
            return ret;
        }
    };
}; // namespace ns3

#endif