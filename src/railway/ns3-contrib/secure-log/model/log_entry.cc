#include "log_entry.h"

using namespace ns3;

LogEntry::LogEntry(int64_t log_seq, int64_t msg_seq, int mtype, int t,
                   NodeInfo sdr, uint32_t content_sz, const char *cont)
    : log_seq_num(log_seq), msg_seq_num(msg_seq), msgtype(mtype),
      task(t), sender(sdr),
      content_size(content_sz)
{
    contents = new char[content_size];
    memcpy(contents, cont, content_size);
}

LogEntry::LogEntry(LogEntry&& other)
{
    log_seq_num = other.log_seq_num;
    msg_seq_num = other.msg_seq_num;
    msgtype = other.msgtype;
    task = other.task;
    sender = other.sender;
    content_size = other.content_size;
    contents = other.contents;
    other.contents = nullptr;
}

LogEntry::LogEntry(int64_t seq, const char *buf)
{
    // sequence number is ignored
    log_seq_num = seq;
    size_t offset = 0;
    // memcpy(&seq_num, buf + offset, sizeof(seq_num));
    // offset += sizeof(seq_num);
    memcpy(&msg_seq_num, buf + offset, sizeof(msg_seq_num));
    offset += sizeof(msg_seq_num);
    memcpy(&msgtype, buf + offset, sizeof(msgtype));
    offset += sizeof(msgtype);
    memcpy(&task, buf + offset, sizeof(task));
    offset += sizeof(task);
    memcpy(&sender, buf + offset, sizeof(sender));
    offset += sizeof(sender);
    memcpy(&content_size, buf + offset, sizeof(content_size));
    offset += sizeof(content_size);
    contents = new char[content_size];
    memcpy(contents, buf + offset, content_size);
}

LogEntry::~LogEntry()
{
    if (contents)
        delete[] contents;
}

uint32_t LogEntry::serialize(char *buf)
{
    // sequence number is ignored
    uint32_t offset = 0;
    memcpy(buf + offset, &msg_seq_num, sizeof(msg_seq_num));
    offset += sizeof(msg_seq_num);
    memcpy(buf + offset, &msgtype, sizeof(msgtype));
    offset += sizeof(msgtype);
    memcpy(buf + offset, &task, sizeof(task));
    offset += sizeof(task);
    memcpy(buf + offset, &sender, sizeof(sender));
    offset += sizeof(sender);
    memcpy(buf + offset, &content_size, sizeof(content_size));
    offset += sizeof(content_size);
    memcpy(buf + offset, contents, content_size);
    offset += content_size;
    return offset;
}