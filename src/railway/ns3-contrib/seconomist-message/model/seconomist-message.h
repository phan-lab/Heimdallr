#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include "msg_def.h"
#include "move_auth.h"
#include "ping-accept.h"
#include "ping-inter.h"
#include "ping-prepare.h"
#include "ping-propose.h"
#include "location-speed-peer.h"
#include "poc.h"
#include "move_auth_fwd.h"

#include "ns3/MultiSigObj.h"
#include "ns3/header.h"
#include "ns3/buffer.h"

#include <openssl/sha.h>

namespace ns3
{
class GeneralMessage : public Header
{
  private:
    int type;
    int64_t seq_num;
    uint32_t size; //!< The content size, not the entire size
    uint8_t content[MAXBUF];
    uint8_t sig_buf[MULTISIG_SIG_LENGTH];
    uint8_t pk_buf[MULTISIG_PUB_LENGTH];
    MultiSigObj *sk, *pk;
    bool sign_hash; /** Sign the hash of the content instead of the content itself */

  public:
    GeneralMessage();
    GeneralMessage(int type,
                   int64_t seq_num,
                   uint32_t size,
                   const uint8_t* content,
                   MultiSigObj* sk,
                   MultiSigObj* pk);

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;

    inline void set_sign_hash(bool val)
    {
        sign_hash = val;
    }

    inline int get_type() const
    {
        return type;
    }

    inline int64_t get_seq_num() const
    {
        return seq_num;
    }

    inline uint8_t* get_content_buf()
    {
        return (uint8_t*)content;
    }

    inline uint8_t* get_constant_content_buf() const
    {
        return (uint8_t*)content;
    }

    inline uint8_t* get_constant_sig_buf() const
    {
        return (uint8_t*)sig_buf;
    }

    inline uint8_t* get_constant_pub_key_buf() const
    {
        return (uint8_t*)pk_buf;
    }

    inline void set_size(uint32_t size)
    {
        this->size = size;
    }

    inline uint32_t get_size() const
    {
        return this->size;
    }

    void sign();

    virtual uint32_t GetSerializedSize() const override;
    virtual void Serialize(Buffer::Iterator start) const override;
    virtual uint32_t Deserialize(Buffer::Iterator start) override;
    virtual void Print(std::ostream& os) const override;
};

}; // namespace ns3

#endif