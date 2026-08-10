#pragma once

#include <deque>
#include <string>

#include "keydrop/core/buffer.hpp"
#include "keydrop/core/types.hpp"
#include "keydrop/transport/transport.hpp"

namespace keydrop {

enum class SchedulerStatusCode {
    ok,
    empty,
    send_failed
};

struct SchedulerResult {
    SchedulerStatusCode code = SchedulerStatusCode::ok;
    std::string message;
    usize packets_sent = 0;
    usize packets_failed = 0;
    usize bytes_sent = 0;

    bool ok() const
    {
        return code == SchedulerStatusCode::ok;
    }
};

class TransportScheduler {
public:
    // Phase 6 backpressure: when max_pending is set (nonzero), enqueue
    // rejects packets beyond the limit instead of growing unboundedly.
    void set_max_pending(usize max_pending);
    usize max_pending() const;

    bool enqueue(const Buffer& packet);
    SchedulerResult flush(Transport& transport);
    void clear();

    usize pending() const;
    bool empty() const;

private:
    std::deque<Buffer> queue_;
    usize max_pending_ = 0;
};

}
