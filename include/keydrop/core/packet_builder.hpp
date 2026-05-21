#pragma once

#include "keydrop/core/buffer.hpp"
#include "keydrop/core/types.hpp"

namespace keydrop {

class PacketBuilder {
public:

    PacketBuilder() = default;

    void write_u8(u8 value);

    void write_u16(u16 value);

    void write_u32(u32 value);

    const Buffer& buffer() const;

    void clear();

private:

    Buffer buffer_;

};

}