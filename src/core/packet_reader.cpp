#include "keydrop/core/packet_reader.hpp"

#include <stdexcept>

#include "keydrop/core/endian.hpp"

namespace keydrop {

PacketReader::PacketReader(const Buffer& buffer)
    : buffer_(buffer)
{
}

u8 PacketReader::read_u8()
{
    if (remaining() < 1)
    {
        throw std::out_of_range("PacketReader read_u8 out of range");
    }

    const u8 value = buffer_.read(cursor_);
    ++cursor_;
    return value;
}

u16 PacketReader::read_u16()
{
    if (remaining() < 2)
    {
        throw std::out_of_range("PacketReader read_u16 out of range");
    }

    u16 little = 0;
    little |= static_cast<u16>(buffer_.read(cursor_));
    little |= static_cast<u16>(buffer_.read(cursor_ + 1)) << 8;
    cursor_ += 2;
    return from_little_endian(little);
}

u32 PacketReader::read_u32()
{
    if (remaining() < 4)
    {
        throw std::out_of_range("PacketReader read_u32 out of range");
    }

    u32 little = 0;
    little |= static_cast<u32>(buffer_.read(cursor_));
    little |= static_cast<u32>(buffer_.read(cursor_ + 1)) << 8;
    little |= static_cast<u32>(buffer_.read(cursor_ + 2)) << 16;
    little |= static_cast<u32>(buffer_.read(cursor_ + 3)) << 24;
    cursor_ += 4;
    return from_little_endian(little);
}

void PacketReader::reset()
{
    cursor_ = 0;
}

usize PacketReader::position() const
{
    return cursor_;
}

usize PacketReader::remaining() const
{
    return buffer_.size() - cursor_;
}

bool PacketReader::empty() const
{
    return remaining() == 0;
}

}
