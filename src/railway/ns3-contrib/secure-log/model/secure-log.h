#ifndef _LOG_H_
#define _LOG_H_

#include "log_entry.h"
#include <stdint.h>
#include <vector>

namespace ns3
{

    class SecureLog
    {
    public:
        SecureLog() : seq_num(0) {}
        ~SecureLog()
        {
            for (auto entry : entries)
                delete entry;
        }

        /**
         * @brief add an entry to the log
         * @return the sequence number of the new entry
         */
        size_t push_entry(int msgtype, int task, int64_t time,
                          NodeInfo sender, size_t content_size,
                          const char *contents);
        /**
         * @brief add an entry to the log with a serialized buffer
         * @return the sequence number of the new entry
         */
        size_t push_entry(const char *buf);

        size_t push_entry(LogEntry *entry);

        LogEntry *get_entry_by_seq(int64_t seq_num);

        /**
         * @brief get the entry with the info of a specific task
         * @param task the task
         * @param pos the position. 0 means the newest entry, 1 means
         * the second newest, etc.
         */
        LogEntry *get_entry_by_task(int task, int pos);

        /**
         * @brief get the entry with the info of a specific task
         * @param msgtype Message type
         * @param pos the position. 0 means the newest entry, 1 means
         * the second newest, etc.
         */
        LogEntry *get_entry_by_msg_type(int msgtype, int pos);

        /**
         * @brief get the entry with the task id and the sender id
         * @param task task id
         * @param sender sender id
         * @param pos position. 0 means the newest entry, 1 means
         * the second newest, etc.
         */
        LogEntry *get_entry_by_task_sender(int task, NodeInfo sender, int pos);

        /**
         * @brief get the entry with the task id and the sender id
         * @param msgtype the message type
         * @param sender sender id
         * @param pos position. 0 means the newest entry, 1 means
         * the second newest, etc.
         */
        LogEntry *get_entry_by_msgtype_sender(int msgtype, NodeInfo sender, int pos);

        inline int64_t get_next_seq_num() { return seq_num; }

    protected:
        int64_t seq_num;
        std::vector<LogEntry *> entries;
    };

}

#endif /* LOG_H */
