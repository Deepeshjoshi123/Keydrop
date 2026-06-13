#include <cassert>

#include "keydrop/core/buffer_pool.hpp"

using namespace keydrop;

int main()
{
    BufferPoolConfig config;
    config.initial_buffers = 2;
    config.default_capacity = 16;
    config.max_available = 2;

    BufferPool pool(config);
    assert(pool.available() == 2);
    assert(pool.total_created() == 2);

    Buffer first = pool.acquire();
    assert(first.empty());
    assert(first.capacity() >= 16);
    assert(pool.available() == 1);

    first.write(0xAA);
    pool.release(static_cast<Buffer&&>(first));
    assert(pool.available() == 2);

    Buffer reused = pool.acquire();
    assert(reused.empty());
    assert(reused.capacity() >= 16);
    assert(pool.available() == 1);
    pool.release(static_cast<Buffer&&>(reused));
    assert(pool.available() == 2);

    {
        BufferLease lease = pool.lease();
        lease.get().write(0xBB);
        assert(!lease.get().empty());
    }
    assert(pool.available() == 2);

    Buffer extra_a = pool.acquire();
    Buffer extra_b = pool.acquire();
    Buffer extra_c = pool.acquire();
    assert(pool.total_created() == 3);
    pool.release(static_cast<Buffer&&>(extra_a));
    pool.release(static_cast<Buffer&&>(extra_b));
    pool.release(static_cast<Buffer&&>(extra_c));
    assert(pool.available() == 2);

    pool.reset();
    assert(pool.available() == 0);
    assert(pool.total_created() == 0);

    return 0;
}
