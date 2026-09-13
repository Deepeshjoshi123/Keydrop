#pragma once

#include <vector>

#include "keydrop/schema/field_mapper.hpp"

namespace keydrop {

struct PayloadPoolConfig {
    usize initial_ordered_payloads = 0;
    usize initial_named_payloads = 0;
    usize default_field_capacity = 0;
    usize max_available = 64;
};

class PayloadPool;

class OrderedPayloadLease {
public:
    OrderedPayloadLease(PayloadPool& pool, usize capacity);
    ~OrderedPayloadLease();

    OrderedPayloadLease(const OrderedPayloadLease&) = delete;
    OrderedPayloadLease& operator=(const OrderedPayloadLease&) = delete;

    OrderedPayloadLease(OrderedPayloadLease&& other) noexcept;
    OrderedPayloadLease& operator=(OrderedPayloadLease&& other) noexcept;

    OrderedPayload& get();
    const OrderedPayload& get() const;

private:
    PayloadPool* pool_ = nullptr;
    OrderedPayload payload_;
};

class NamedPayloadLease {
public:
    NamedPayloadLease(PayloadPool& pool, usize capacity);
    ~NamedPayloadLease();

    NamedPayloadLease(const NamedPayloadLease&) = delete;
    NamedPayloadLease& operator=(const NamedPayloadLease&) = delete;

    NamedPayloadLease(NamedPayloadLease&& other) noexcept;
    NamedPayloadLease& operator=(NamedPayloadLease&& other) noexcept;

    NamedPayload& get();
    const NamedPayload& get() const;

private:
    PayloadPool* pool_ = nullptr;
    NamedPayload payload_;
};

class PayloadPool {
public:
    PayloadPool() = default;
    explicit PayloadPool(const PayloadPoolConfig& config);

    void configure(const PayloadPoolConfig& config);

    OrderedPayload acquire_ordered(usize capacity);
    NamedPayload acquire_named(usize capacity);
    void release(OrderedPayload&& payload);
    void release(NamedPayload&& payload);

    OrderedPayloadLease lease_ordered(usize capacity);
    NamedPayloadLease lease_named(usize capacity);

    void reset();
    usize available_ordered() const;
    usize available_named() const;
    usize total_ordered_created() const;
    usize total_named_created() const;
    const PayloadPoolConfig& config() const;

private:
    PayloadPoolConfig config_;
    std::vector<OrderedPayload> ordered_available_;
    std::vector<NamedPayload> named_available_;
    usize total_ordered_created_ = 0;
    usize total_named_created_ = 0;
};

}
