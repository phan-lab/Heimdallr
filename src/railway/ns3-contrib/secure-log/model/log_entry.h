#ifndef _LOG_ENTRY_H_
#define _LOG_ENTRY_H_

#include <stdint.h>

#include "ns3/core-module.h"
#include "ns3/tasks.h"

namespace ns3
{
    class LogEntry
    {
    public:
        int64_t log_seq_num; /* The sequence number of the log */
        int64_t msg_seq_num; /* The sequence number of the message */
        int msgtype;
        int task; /* related task ID */
        NodeInfo sender;
        uint32_t content_size;
        char *contents;

        LogEntry() = delete;
        LogEntry(LogEntry &) = delete;
        LogEntry(LogEntry &&);
        LogEntry(int64_t log_seq, int64_t msg_seq, int mtype, int t,
                 NodeInfo sdr, uint32_t content_sz, const char *cont);

        /* deserialize from a buffer */
        LogEntry(int64_t seq, const char *buf);

        ~LogEntry();

        uint32_t serialize(char *buf);
    };
};

#endif