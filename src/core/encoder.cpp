#include "keydrop/core/encoder.hpp"

#include <cstring>

#include "keydrop/core/endian.hpp"

namespace keydrop {

void Encoder::write_u8(u8 value)
{
    builder_.write<u8>(value);
}

void Encoder::write_u16(u16 value)
{
    const u16 encoded = to_little_endian(value);
    builder_.write<u16>(encoded);
}

void Encoder::write_u32(u32 value)
{
    const u32 encoded = to_little_endian(value);
    builder_.write<u32>(encoded);
}

void Encoder::write_i8(i8 value)
{
    builder_.write<i8>(value);
}

void Encoder::write_i16(i16 value)
{
    const u16 encoded = to_little_endian(static_cast<u16>(value));
    builder_.write<u16>(encoded);
}

void Encoder::write_i32(i32 value)
{
    const u32 encoded = to_little_endian(static_cast<u32>(value));
    builder_.write<u32>(encoded);
}

void Encoder::write_f32(f32 value)
{
    u32 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32(bits);
}

void Encoder::write_f64(f64 value)
{
    u64 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const u64 encoded = to_little_endian(bits);
    builder_.write<u64>(encoded);
}

void Encoder::write_string(const std::string& value)
{
    write_u16(static_cast<u16>(value.size()));
    write_bytes(reinterpret_cast<const byte*>(value.data()), value.size());
}

void Encoder::write_bytes(const byte* data, usize size)
{
    builder_.write_bytes(data, size);
}

const Buffer& Encoder::buffer() const
{
    return builder_.buffer();
}

void Encoder::reserve(usize capacity)
{
    builder_.reserve(capacity);
}

void Encoder::clear()
{
    builder_.clear();
}

}
