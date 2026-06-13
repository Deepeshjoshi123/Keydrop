#pragma once

#include <vector>

#include "keydrop/core/types.hpp"

namespace keydrop {

class BufferView {
public:
    BufferView() = default;
    BufferView(const byte* data, usize size);

    const byte* bytes() const;
    usize size() const;
    bool empty() const;
    byte read(usize index) const;
    BufferView slice(usize offset, usize size) const;

private:
    const byte* data_ = nullptr;
    usize size_ = 0;
};

class Buffer {
public:

    // Constructor
    Buffer() = default;

    // Write one byte
    void write(byte value);
    
    // Append raw bytes
    void append(const byte* data, usize size);

    // Append a non-owning byte view
    void append(BufferView view);

    // Append another buffer
    void append(const Buffer& buffer);

    // Read one byte
    byte read(usize index) const;

    // Clear memory
    void clear();

    // Resize buffer
    void resize(usize new_size);

    // Reserve capacity
    void reserve(usize capacity);

    // Current size
    usize size() const;

    // Empty check
    bool empty() const;

    // Immutable access
    const std::vector<byte>& data() const;

    // Raw immutable byte access
    const byte* bytes() const;

    // Raw mutable byte access
    byte* mutable_bytes();

    // Non-owning view over the whole buffer
    BufferView view() const;

    // Non-owning view over a sub-range
    BufferView slice(usize offset, usize size) const;

private:

    std::vector<byte> bytes_;

};

}
