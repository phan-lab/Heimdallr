#ifndef _COMMON_H_
#define _COMMON_H_



#define USE_CRYPTO 1

#define BASE_PORT_SERVER 8888
#define BASE_PORT_CLIENT 9000

#ifndef FAULTY_NODES
#define FAULTY_NODES 1
#endif
#define NEED_OFFSET 0


#include <cstdint>
#include <cassert>

enum class PTPMessageType {
    SYNC = 0,
    DELAY_REQ = 2,
    DELAY_RESP = 3
};

struct PTPMessage {
    PTPMessageType messageType;
    uint64_t timestamp;      // t1 for SYNC, t3 for DELAY_REQ, t4 for DELAY_RESP
    uint64_t server_id;
    uint64_t t2;            // Added to store the SYNC receive time
};

#endif