#include "keydrop/core/buffer.hpp"

#include <stdexcept>

namespace keydrop {

void Buffer::write(byte value)
{
    bytes_.push_back(value);
}

void Buffer::append(const byte* data, usize size)
{
    bytes_.insert(bytes_.end(), data, data + size);
}

byte Buffer::read(usize index) const
{
    if (index >= bytes_.size())
    {
        throw std::out_of_range(
            "Buffer index out of range"
        );
    }

    return bytes_[index];
}

void Buffer::clear()
{
    bytes_.clear();
}

void Buffer::resize(usize new_size)
{
    bytes_.resize(new_size);
}

void Buffer::reserve(usize capacity)
{
    bytes_.reserve(capacity);
}

usize Buffer::size() const
{
    return bytes_.size();
}

bool Buffer::empty() const
{
    return bytes_.empty();
}

const std::vector<byte>& Buffer::data() const
{
    return bytes_;
}
}
