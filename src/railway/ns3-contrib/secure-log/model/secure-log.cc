#include "secure-log.h"

namespace ns3
{

    size_t SecureLog::push_entry(int msgtype, int task, int64_t time,
                           NodeInfo sender, size_t content_size,
                           const char *contents)
    {
        entries.emplace_back(new LogEntry(seq_num, time, msgtype,
                                          task, sender,
                                          content_size, contents));
        seq_num++;
        return seq_num - 1;
    }

    size_t SecureLog::push_entry(const char *buf)
    {
        entries.emplace_back(new LogEntry(seq_num, buf));
        seq_num++;
        return seq_num - 1;
    }

    size_t SecureLog::push_entry(LogEntry *entry)
    {
        assert(entry->log_seq_num == this->seq_num);
        seq_num++;
        entries.emplace_back(entry);
        return entry->log_seq_num;
    }

    LogEntry *SecureLog::get_entry_by_seq(int64_t seq_num)
    {
        for (auto &entry : entries)
        {
            if (entry->log_seq_num == seq_num)
                return entry;
        }
        return nullptr;
    }

    LogEntry *SecureLog::get_entry_by_task(int task, int pos)
    {
        int offset = 0;
        for (size_t i = entries.size() - 1; i != (size_t)-1; i--)
        {
            if (entries.at(i)->task == task)
            {
                if (pos == offset)
                    return entries[i];
                else
                    offset++;
            }
        }
        return nullptr;
    }

    LogEntry *SecureLog::get_entry_by_msg_type(int msgtype, int pos)
    {
        int offset = 0;
        for (size_t i = entries.size() - 1; i != (size_t)-1; i--)
        {
            if (entries.at(i)->msgtype == msgtype)
            {
                if (pos == offset)
                    return entries[i];
                else
                    offset++;
            }
        }
        return nullptr;
    }

    LogEntry *SecureLog::get_entry_by_task_sender(int task, NodeInfo sender, int pos)
    {
        int offset = 0;
        for (size_t i = entries.size() - 1; i != (size_t)-1; i--)
        {
            if (entries.at(i)->task == task && entries.at(i)->sender == sender)
            {
                if (pos == offset)
                    return entries[i];
                else
                    offset++;
            }
        }
        return nullptr;
    }

    LogEntry *SecureLog::get_entry_by_msgtype_sender(int msgtype, NodeInfo sender, int pos)
    {
        int offset = 0;
        for (size_t i = entries.size() - 1; i != (size_t)-1; i--)
        {
            if (entries.at(i)->msgtype == msgtype && entries.at(i)->sender == sender)
            {
                if (pos == offset)
                    return entries[i];
                else
                    offset++;
            }
        }
        return nullptr;
    }

}
