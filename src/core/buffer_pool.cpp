#include "keydrop/core/buffer_pool.hpp"

#include <utility>

namespace keydrop {

BufferPool::BufferPool(const BufferPoolConfig& config)
{
    configure(config);
}

void BufferPool::configure(const BufferPoolConfig& config)
{
    config_ = config;
    available_.clear();
    available_.reserve(config_.initial_buffers);
    total_created_ = 0;

    for (usize i = 0; i < config_.initial_buffers; ++i)
    {
        Buffer buffer;
        buffer.reserve(config_.default_capacity);
        available_.push_back(buffer);
        total_created_ += 1;
    }
}

Buffer BufferPool::acquire()
{
    if (!available_.empty())
    {
        Buffer buffer = std::move(available_.back());
        available_.pop_back();
        buffer.clear();
        if (buffer.capacity() < config_.default_capacity)
        {
            buffer.reserve(config_.default_capacity);
        }
        return buffer;
    }

    Buffer buffer;
    buffer.reserve(config_.default_capacity);
    total_created_ += 1;
    return buffer;
}

void BufferPool::release(Buffer&& buffer)
{
    if (available_.size() >= config_.max_available)
    {
        return;
    }

    buffer.clear();
    if (buffer.capacity() < config_.default_capacity)
    {
        buffer.reserve(config_.default_capacity);
    }
    available_.push_back(std::move(buffer));
}

BufferLease BufferPool::lease()
{
    return BufferLease(*this);
}

void BufferPool::reset()
{
    available_.clear();
    total_created_ = 0;
}

usize BufferPool::available() const
{
    return available_.size();
}

usize BufferPool::total_created() const
{
    return total_created_;
}

const BufferPoolConfig& BufferPool::config() const
{
    return config_;
}

BufferLease::BufferLease(BufferPool& pool)
    : pool_(&pool)
    , buffer_(pool.acquire())
{
}

BufferLease::~BufferLease()
{
    if (pool_ != nullptr)
    {
        pool_->release(std::move(buffer_));
    }
}

BufferLease::BufferLease(BufferLease&& other) noexcept
    : pool_(other.pool_)
    , buffer_(std::move(other.buffer_))
{
    other.pool_ = nullptr;
}

BufferLease& BufferLease::operator=(BufferLease&& other) noexcept
{
    if (this != &other)
    {
        if (pool_ != nullptr)
        {
            pool_->release(std::move(buffer_));
        }

        pool_ = other.pool_;
        buffer_ = std::move(other.buffer_);
        other.pool_ = nullptr;
    }

    return *this;
}

Buffer& BufferLease::get()
{
    return buffer_;
}

const Buffer& BufferLease::get() const
{
    return buffer_;
}

}
