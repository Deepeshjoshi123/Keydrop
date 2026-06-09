#include "keydrop/transport/transport_scheduler.hpp"

namespace keydrop {

void TransportScheduler::enqueue(const Buffer& packet)
{
    queue_.push_back(packet);
}

SchedulerResult TransportScheduler::flush(Transport& transport)
{
    if (queue_.empty())
    {
        return {SchedulerStatusCode::empty, "No packets pending.", 0, 0, 0};
    }

    SchedulerResult result;
    result.code = SchedulerStatusCode::ok;
    result.message = "Scheduled packets sent.";

    while (!queue_.empty())
    {
        const Buffer& packet = queue_.front();
        const TransportResult send_result = transport.send(packet);
        if (!send_result.ok())
        {
            result.code = SchedulerStatusCode::send_failed;
            result.message = send_result.message;
            result.packets_failed = queue_.size();
            return result;
        }

        result.packets_sent += 1;
        result.bytes_sent += send_result.bytes_transferred;
        queue_.pop_front();
    }

    return result;
}

void TransportScheduler::clear()
{
    queue_.clear();
}

usize TransportScheduler::pending() const
{
    return queue_.size();
}

bool TransportScheduler::empty() const
{
    return queue_.empty();
}

}
