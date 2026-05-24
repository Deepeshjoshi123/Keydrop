#pragma once

#include "keydrop/core/packet_builder.hpp"
#include "keydrop/core/types.hpp"

namespace keydrop {

class Encoder {
public:
    Encoder() = default;

    void write_u8(u8 value);
    void write_u16(u16 value);
    void write_u32(u32 value);
    void write_f32(f32 value);

    void write_bytes(const byte* data, usize size);

    const Buffer& buffer() const;
    void clear();

private:
    PacketBuilder builder_;
};

}
