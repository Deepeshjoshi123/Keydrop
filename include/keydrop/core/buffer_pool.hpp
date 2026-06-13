#pragma once

#include <vector>

#include "keydrop/core/buffer.hpp"

namespace keydrop {

struct BufferPoolConfig {
    usize initial_buffers = 0;
    usize default_capacity = 0;
    usize max_available = 64;
};

class BufferLease;

class BufferPool {
public:
    BufferPool() = default;
    explicit BufferPool(const BufferPoolConfig& config);

    void configure(const BufferPoolConfig& config);
    Buffer acquire();
    void release(Buffer&& buffer);
    BufferLease lease();
    void reset();

    usize available() const;
    usize total_created() const;
    const BufferPoolConfig& config() const;

private:
    BufferPoolConfig config_;
    std::vector<Buffer> available_;
    usize total_created_ = 0;
};

class BufferLease {
public:
    BufferLease(BufferPool& pool);
    ~BufferLease();

    BufferLease(const BufferLease&) = delete;
    BufferLease& operator=(const BufferLease&) = delete;

    BufferLease(BufferLease&& other) noexcept;
    BufferLease& operator=(BufferLease&& other) noexcept;

    Buffer& get();
    const Buffer& get() const;

private:
    BufferPool* pool_ = nullptr;
    Buffer buffer_;
};

}
