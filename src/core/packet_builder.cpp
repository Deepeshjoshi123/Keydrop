#include "keydrop/core/packet_builder.hpp"

namespace keydrop {

void PacketBuilder::write_u8(u8 value)
{
    buffer_.write(value);
}

void PacketBuilder::write_u16(u16 value)
{
    buffer_.write(
        static_cast<byte>(
            value & 0xFF
        )
    );

    buffer_.write(
        static_cast<byte>(
            (value >> 8) & 0xFF
        )
    );
}

void PacketBuilder::write_u32(u32 value)
{
    for (int i = 0; i < 4; ++i)
    {
        buffer_.write(
            static_cast<byte>(
                (value >> (8 * i))
                &
                0xFF
            )
        );
    }
}

const Buffer& PacketBuilder::buffer() const
{
    return buffer_;
}

void PacketBuilder::clear()
{
    buffer_.clear();
}

}