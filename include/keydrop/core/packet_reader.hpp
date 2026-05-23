#pragma once

#include "keydrop/core/buffer.hpp"
#include "keydrop/core/types.hpp"

namespace keydrop {

class PacketReader {
public:
    explicit PacketReader(const Buffer& buffer);

    u8 read_u8();
    u16 read_u16();
    u32 read_u32();

    void reset();
    usize position() const;
    usize remaining() const;
    bool empty() const;

private:
    const Buffer& buffer_;
    usize cursor_ = 0;
};

}
