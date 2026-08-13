#include "tasks.h"

namespace ns3
{

bool
operator==(const TaskWithState& l, const TaskWithState& r)
{
    return l.state == r.state && l.task == r.task;
}

bool
operator<(const TaskWithState& l, const TaskWithState& r)
{
    return l.state < r.state || (l.state == r.state && l.task < r.task);
}
} // namespace ns3