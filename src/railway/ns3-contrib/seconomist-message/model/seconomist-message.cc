#include "seconomist-message.h"

namespace ns3
{

    GeneralMessage::GeneralMessage()
    {
        memset(this->content, 0, MAXBUF);
        sk = nullptr;
        pk = nullptr;
        sign_hash = false;
    }

    GeneralMessage::GeneralMessage(int type,
                                   int64_t seq_num,
                                   uint32_t size,
                                   const uint8_t *content,
                                   MultiSigObj *sk,
                                   MultiSigObj *pk)
        : type(type),
          seq_num(seq_num),
          size(size),
          sk(sk),
          pk(pk),
          sign_hash(false)
    {
        memset(this->content, 0, MAXBUF);
        if (size == 0)
            return;
        memcpy(this->content, content, size);
    }

    void
    GeneralMessage::sign()
    {
        int buf_size = sizeof(type) + sizeof(seq_num) + sizeof(size) + size;
        unsigned char buf_to_sign[buf_size] = {0};

        int offset = 0;
        memcpy(buf_to_sign + offset, &type, sizeof(type));
        offset += sizeof(type);
        memcpy(buf_to_sign + offset, &seq_num, sizeof(seq_num));
        offset += sizeof(seq_num);
        memcpy(buf_to_sign + offset, &size, sizeof(size));
        offset += sizeof(size);

        MultiSigObj sig;
        if (!sign_hash)
        {
            memcpy(buf_to_sign + offset, content, size);
            offset += size;
            MSObj_sign(sig, (char *)buf_to_sign, buf_size, *sk);
            // NS_LOG_UNCOND("sign size " << buf_size);
            // NS_LOG_UNCOND("sign " << type
            //                       << " " << seq_num << " " << size);
        }
        else
        {
            int hash_and_header_size = buf_size - size + SHA256_DIGEST_LENGTH;
            unsigned char hash[SHA256_DIGEST_LENGTH] = {0};
            SHA256(content, size, hash);
            memcpy(buf_to_sign + buf_size - size, hash, SHA256_DIGEST_LENGTH);
            MSObj_sign(sig, (char *)buf_to_sign, hash_and_header_size, *sk);
        }
        MSObj_to_buf(sig, (char *)sig_buf);
        MSObj_to_buf(*pk, (char*)pk_buf);
    }

    TypeId
    GeneralMessage::GetTypeId()
    {
        static TypeId tid =
            TypeId("GeneralMessage").SetParent<Header>().AddConstructor<GeneralMessage>();
        return tid;
    }

    TypeId
    GeneralMessage::GetInstanceTypeId() const
    {
        return GeneralMessage::GetTypeId();
    }

    uint32_t
    GeneralMessage::GetSerializedSize() const
    {
        uint32_t ret = sizeof(type) + sizeof(seq_num) + sizeof(size) + size;
#if USE_CRYPTO
        ret += MULTISIG_PUB_LENGTH + MULTISIG_SIG_LENGTH;
#endif
        return ret;
    }

    void
    GeneralMessage::Serialize(Buffer::Iterator start) const
    {
        start.WriteU32(type);
        start.WriteU64(seq_num);
        start.WriteU32(size);
        start.Write(content, size);

#if USE_CRYPTO
        MultiSigObj sig;
        int buf_size = sizeof(type) + sizeof(seq_num) + sizeof(size) + size;

        if (!sign_hash)
        {
            unsigned char buf_to_sign[buf_size] = {0};
            start.Prev(buf_size);
            start.Read(buf_to_sign, buf_size);
            MSObj_sign(sig, (char *)buf_to_sign, buf_size, *sk);
        }
        else
        {
            int hash_and_header_size = buf_size - size + SHA256_DIGEST_LENGTH;
            unsigned char buf_to_sign[hash_and_header_size] = {0};
            start.Prev(buf_size);
            start.Read(buf_to_sign, buf_size - size);
            start.Next(size);
            unsigned char hash[SHA256_DIGEST_LENGTH] = {0};
            SHA256(content, size, hash);
            memcpy(buf_to_sign + buf_size - size, hash, SHA256_DIGEST_LENGTH);
            MSObj_sign(sig, (char *)buf_to_sign, hash_and_header_size, *sk);
        }

        char pk_buf[MULTISIG_PUB_LENGTH] = {0};
        char sig_buf[MULTISIG_SIG_LENGTH] = {0};

        MSObj_to_buf(*pk, pk_buf);
        MSObj_to_buf(sig, sig_buf);

        start.Write((uint8_t *)pk_buf, MULTISIG_PUB_LENGTH);
        start.Write((uint8_t *)sig_buf, MULTISIG_SIG_LENGTH);

#endif
        // NS_LOG_UNCOND( "Ser " << start.GetSize() << " " << size);
    }

    /**
     * @return The size of the entire message
     */
    uint32_t
    GeneralMessage::Deserialize(Buffer::Iterator start)
    {
        // NS_LOG_UNCOND( "Des " << start.GetSize());
        uint32_t ret = 0;
        type = (int)start.ReadU32();
        seq_num = start.ReadU64();
        size = start.ReadU32();
        // NS_LOG_UNCOND( "Content size " << size);

        start.Read(content, size);
        ret += sizeof(type) + sizeof(seq_num) + sizeof(size) + size;
#if USE_CRYPTO
        int buf_size = sizeof(type) + sizeof(seq_num) + sizeof(size) + size;
        char buf_to_sign[buf_size] = {0};

        start.Prev(buf_size);
        start.Read((uint8_t *)buf_to_sign, buf_size);

        start.Read((uint8_t *)pk_buf, MULTISIG_PUB_LENGTH);
        start.Read((uint8_t *)sig_buf, MULTISIG_SIG_LENGTH);

        MultiSigObj sig, msg_pk;
        MSObj_from_buf(msg_pk, (char *)pk_buf);
        MSObj_from_buf(sig, (char *)sig_buf);

        int valid = MSObj_verify(buf_to_sign, buf_size, msg_pk, sig);

        if (!valid)
        {
            unsigned char hash[SHA256_DIGEST_LENGTH] = {0};
            SHA256((uint8_t *)content, size, hash);
            memcpy(buf_to_sign + buf_size - size, hash, SHA256_DIGEST_LENGTH);

            valid =
                MSObj_verify((char *)buf_to_sign,
                             buf_size - size + SHA256_DIGEST_LENGTH, msg_pk, sig);
            NS_ASSERT(valid);
        }

        ret += MULTISIG_PUB_LENGTH + MULTISIG_SIG_LENGTH;
#endif
        return ret;
    }

    void
    GeneralMessage::Print(std::ostream &os) const
    {
        os << "Type: " << type << std::endl;
        os << "Seq: " << seq_num << std::endl;
        os << "Size: " << size << std::endl;
        os << content << std::endl;
    }
} // namespace ns3