#include "keydrop/core/buffer.hpp"

#include <stdexcept>

namespace keydrop {

BufferView::BufferView(const byte* data, usize size)
    : data_(data)
    , size_(size)
{
}

const byte* BufferView::bytes() const
{
    return data_;
}

usize BufferView::size() const
{
    return size_;
}

bool BufferView::empty() const
{
    return size_ == 0;
}

byte BufferView::read(usize index) const
{
    if (index >= size_ || data_ == nullptr)
    {
        throw std::out_of_range(
            "BufferView index out of range"
        );
    }

    return data_[index];
}

BufferView BufferView::slice(usize offset, usize size) const
{
    if (offset > size_ || size > size_ - offset)
    {
        throw std::out_of_range(
            "BufferView slice out of range"
        );
    }

    if (size == 0)
    {
        return BufferView(data_, 0);
    }

    return BufferView(data_ + offset, size);
}

void Buffer::write(byte value)
{
    bytes_.push_back(value);
}

void Buffer::append(const byte* data, usize size)
{
    if (size == 0)
    {
        return;
    }

    if (data == nullptr)
    {
        throw std::out_of_range(
            "Buffer append null data"
        );
    }

    bytes_.insert(bytes_.end(), data, data + size);
}

void Buffer::append(BufferView view)
{
    append(view.bytes(), view.size());
}

void Buffer::append(const Buffer& buffer)
{
    append(buffer.view());
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

usize Buffer::capacity() const
{
    return bytes_.capacity();
}

bool Buffer::empty() const
{
    return bytes_.empty();
}

const std::vector<byte>& Buffer::data() const
{
    return bytes_;
}

const byte* Buffer::bytes() const
{
    return bytes_.empty() ? nullptr : bytes_.data();
}

byte* Buffer::mutable_bytes()
{
    return bytes_.empty() ? nullptr : bytes_.data();
}

BufferView Buffer::view() const
{
    return BufferView(bytes(), size());
}

BufferView Buffer::slice(usize offset, usize size) const
{
    return view().slice(offset, size);
}

}
