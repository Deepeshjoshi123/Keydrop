#include <cassert>

#include "keydrop/schema/payload_pool.hpp"

using namespace keydrop;

int main()
{
    PayloadPoolConfig config;
    config.initial_ordered_payloads = 1;
    config.initial_named_payloads = 1;
    config.default_field_capacity = 3;
    config.max_available = 2;

    PayloadPool pool(config);
    assert(pool.available_ordered() == 1);
    assert(pool.available_named() == 1);
    assert(pool.total_ordered_created() == 1);
    assert(pool.total_named_created() == 1);

    {
        OrderedPayloadLease ordered = pool.lease_ordered(2);
        ordered.get().push_back(FieldValue::from_u8(1));
        ordered.get().push_back(FieldValue::from_u16(2));
        assert(ordered.get().size() == 2);
        assert(pool.available_ordered() == 0);
    }
    assert(pool.available_ordered() == 1);

    {
        OrderedPayloadLease ordered = pool.lease_ordered(5);
        assert(ordered.get().empty());
        assert(ordered.get().capacity() >= 5);
    }

    {
        NamedPayloadLease named = pool.lease_named(2);
        named.get()["temperature"] = FieldValue::from_u16(32);
        named.get()["device_id"] = FieldValue::from_string("sensor_01");
        assert(named.get().size() == 2);
        assert(pool.available_named() == 0);
    }
    assert(pool.available_named() == 1);

    NamedPayload named_a = pool.acquire_named(1);
    NamedPayload named_b = pool.acquire_named(1);
    NamedPayload named_c = pool.acquire_named(1);
    assert(pool.total_named_created() == 3);
    pool.release(static_cast<NamedPayload&&>(named_a));
    pool.release(static_cast<NamedPayload&&>(named_b));
    pool.release(static_cast<NamedPayload&&>(named_c));
    assert(pool.available_named() == 2);

    pool.reset();
    assert(pool.available_ordered() == 0);
    assert(pool.available_named() == 0);
    assert(pool.total_ordered_created() == 0);
    assert(pool.total_named_created() == 0);

    return 0;
}
