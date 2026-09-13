#include "keydrop/schema/payload_pool.hpp"

#include <utility>

namespace keydrop {

OrderedPayloadLease::OrderedPayloadLease(PayloadPool& pool, usize capacity)
    : pool_(&pool)
    , payload_(pool.acquire_ordered(capacity))
{
}

OrderedPayloadLease::~OrderedPayloadLease()
{
    if (pool_ != nullptr)
    {
        pool_->release(std::move(payload_));
    }
}

OrderedPayloadLease::OrderedPayloadLease(OrderedPayloadLease&& other) noexcept
    : pool_(other.pool_)
    , payload_(std::move(other.payload_))
{
    other.pool_ = nullptr;
}

OrderedPayloadLease& OrderedPayloadLease::operator=(OrderedPayloadLease&& other) noexcept
{
    if (this != &other)
    {
        if (pool_ != nullptr)
        {
            pool_->release(std::move(payload_));
        }

        pool_ = other.pool_;
        payload_ = std::move(other.payload_);
        other.pool_ = nullptr;
    }

    return *this;
}

OrderedPayload& OrderedPayloadLease::get()
{
    return payload_;
}

const OrderedPayload& OrderedPayloadLease::get() const
{
    return payload_;
}

NamedPayloadLease::NamedPayloadLease(PayloadPool& pool, usize capacity)
    : pool_(&pool)
    , payload_(pool.acquire_named(capacity))
{
}

NamedPayloadLease::~NamedPayloadLease()
{
    if (pool_ != nullptr)
    {
        pool_->release(std::move(payload_));
    }
}

NamedPayloadLease::NamedPayloadLease(NamedPayloadLease&& other) noexcept
    : pool_(other.pool_)
    , payload_(std::move(other.payload_))
{
    other.pool_ = nullptr;
}

NamedPayloadLease& NamedPayloadLease::operator=(NamedPayloadLease&& other) noexcept
{
    if (this != &other)
    {
        if (pool_ != nullptr)
        {
            pool_->release(std::move(payload_));
        }

        pool_ = other.pool_;
        payload_ = std::move(other.payload_);
        other.pool_ = nullptr;
    }

    return *this;
}

NamedPayload& NamedPayloadLease::get()
{
    return payload_;
}

const NamedPayload& NamedPayloadLease::get() const
{
    return payload_;
}

PayloadPool::PayloadPool(const PayloadPoolConfig& config)
{
    configure(config);
}

void PayloadPool::configure(const PayloadPoolConfig& config)
{
    config_ = config;
    ordered_available_.clear();
    named_available_.clear();
    ordered_available_.reserve(config_.initial_ordered_payloads);
    named_available_.reserve(config_.initial_named_payloads);
    total_ordered_created_ = 0;
    total_named_created_ = 0;

    for (usize i = 0; i < config_.initial_ordered_payloads; ++i)
    {
        OrderedPayload payload;
        payload.reserve(config_.default_field_capacity);
        ordered_available_.push_back(std::move(payload));
        total_ordered_created_ += 1;
    }

    for (usize i = 0; i < config_.initial_named_payloads; ++i)
    {
        NamedPayload payload;
        payload.reserve(config_.default_field_capacity);
        named_available_.push_back(std::move(payload));
        total_named_created_ += 1;
    }
}

OrderedPayload PayloadPool::acquire_ordered(usize capacity)
{
    const usize target_capacity =
        capacity > config_.default_field_capacity
        ? capacity
        : config_.default_field_capacity;

    if (!ordered_available_.empty())
    {
        OrderedPayload payload = std::move(ordered_available_.back());
        ordered_available_.pop_back();
        payload.clear();
        payload.reserve(target_capacity);
        return payload;
    }

    OrderedPayload payload;
    payload.reserve(target_capacity);
    total_ordered_created_ += 1;
    return payload;
}

NamedPayload PayloadPool::acquire_named(usize capacity)
{
    const usize target_capacity =
        capacity > config_.default_field_capacity
        ? capacity
        : config_.default_field_capacity;

    if (!named_available_.empty())
    {
        NamedPayload payload = std::move(named_available_.back());
        named_available_.pop_back();
        payload.clear();
        payload.reserve(target_capacity);
        return payload;
    }

    NamedPayload payload;
    payload.reserve(target_capacity);
    total_named_created_ += 1;
    return payload;
}

void PayloadPool::release(OrderedPayload&& payload)
{
    if (ordered_available_.size() >= config_.max_available)
    {
        return;
    }

    payload.clear();
    if (payload.capacity() < config_.default_field_capacity)
    {
        payload.reserve(config_.default_field_capacity);
    }
    ordered_available_.push_back(std::move(payload));
}

void PayloadPool::release(NamedPayload&& payload)
{
    if (named_available_.size() >= config_.max_available)
    {
        return;
    }

    payload.clear();
    if (payload.bucket_count() < config_.default_field_capacity)
    {
        payload.reserve(config_.default_field_capacity);
    }
    named_available_.push_back(std::move(payload));
}

OrderedPayloadLease PayloadPool::lease_ordered(usize capacity)
{
    return OrderedPayloadLease(*this, capacity);
}

NamedPayloadLease PayloadPool::lease_named(usize capacity)
{
    return NamedPayloadLease(*this, capacity);
}

void PayloadPool::reset()
{
    ordered_available_.clear();
    named_available_.clear();
    total_ordered_created_ = 0;
    total_named_created_ = 0;
}

usize PayloadPool::available_ordered() const
{
    return ordered_available_.size();
}

usize PayloadPool::available_named() const
{
    return named_available_.size();
}

usize PayloadPool::total_ordered_created() const
{
    return total_ordered_created_;
}

usize PayloadPool::total_named_created() const
{
    return total_named_created_;
}

const PayloadPoolConfig& PayloadPool::config() const
{
    return config_;
}

}
