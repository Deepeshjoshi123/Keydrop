#include "keydrop/core/packet_reader.hpp"

#include <cstring>
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

    const u8 value = buffer_.bytes()[cursor_];
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
    const byte* data = buffer_.bytes();
    little |= static_cast<u16>(data[cursor_]);
    little |= static_cast<u16>(data[cursor_ + 1]) << 8;
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
    const byte* data = buffer_.bytes();
    little |= static_cast<u32>(data[cursor_]);
    little |= static_cast<u32>(data[cursor_ + 1]) << 8;
    little |= static_cast<u32>(data[cursor_ + 2]) << 16;
    little |= static_cast<u32>(data[cursor_ + 3]) << 24;
    cursor_ += 4;
    return from_little_endian(little);
}

u64 PacketReader::read_u64()
{
    if (remaining() < 8)
    {
        throw std::out_of_range("PacketReader read_u64 out of range");
    }

    u64 little = 0;
    const byte* data = buffer_.bytes();
    little |= static_cast<u64>(data[cursor_]);
    little |= static_cast<u64>(data[cursor_ + 1]) << 8;
    little |= static_cast<u64>(data[cursor_ + 2]) << 16;
    little |= static_cast<u64>(data[cursor_ + 3]) << 24;
    little |= static_cast<u64>(data[cursor_ + 4]) << 32;
    little |= static_cast<u64>(data[cursor_ + 5]) << 40;
    little |= static_cast<u64>(data[cursor_ + 6]) << 48;
    little |= static_cast<u64>(data[cursor_ + 7]) << 56;
    cursor_ += 8;
    return from_little_endian(little);
}

i8 PacketReader::read_i8()
{
    return static_cast<i8>(read_u8());
}

i16 PacketReader::read_i16()
{
    return static_cast<i16>(read_u16());
}

i32 PacketReader::read_i32()
{
    return static_cast<i32>(read_u32());
}

f32 PacketReader::read_f32()
{
    const u32 bits = read_u32();
    f32 value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

f64 PacketReader::read_f64()
{
    const u64 bits = read_u64();
    f64 value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string PacketReader::read_string()
{
    const u16 size = read_u16();
    return read_string_from_size(size);
}

std::string PacketReader::read_string_from_size(u16 size)
{
    if (remaining() < size)
    {
        throw std::out_of_range("PacketReader read_string out of range");
    }

    const byte* data = buffer_.bytes();
    std::string value(reinterpret_cast<const char*>(data + cursor_), size);
    cursor_ += size;
    return value;
}

std::vector<byte> PacketReader::read_bytes(usize size)
{
    if (remaining() < size)
    {
        throw std::out_of_range("PacketReader read_bytes out of range");
    }

    const byte* data = buffer_.bytes();
    std::vector<byte> out(data + cursor_, data + cursor_ + size);
    cursor_ += size;
    return out;
}

void PacketReader::skip(usize size)
{
    if (remaining() < size)
    {
        throw std::out_of_range("PacketReader skip out of range");
    }

    cursor_ += size;
}

const Buffer& PacketReader::buffer() const
{
    return buffer_;
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
