#include "keydrop/transport/transport_scheduler.hpp"

namespace keydrop {

void TransportScheduler::set_max_pending(usize max_pending)
{
    max_pending_ = max_pending;
}

usize TransportScheduler::max_pending() const
{
    return max_pending_;
}

bool TransportScheduler::enqueue(const Buffer& packet)
{
    if (max_pending_ != 0 && queue_.size() >= max_pending_)
    {
        return false; // backpressure: caller must flush or drop
    }

    queue_.push_back(packet);
    return true;
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
