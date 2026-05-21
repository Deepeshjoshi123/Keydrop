#pragma once

#include <vector>

#include "keydrop/core/types.hpp"

namespace keydrop {

class Buffer {
public:

    // Constructor
    Buffer() = default;

    // Write one byte
    void write(byte value);

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

private:

    std::vector<byte> bytes_;

};

}