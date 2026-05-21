#include "keydrop/core/packet_builder.hpp"

namespace keydrop {

PacketBuilder::PacketBuilder()
    : buffer_()
{
}

void PacketBuilder::write_bytes(
    const byte* data,
    usize size
)
{
    buffer_.append(data, size);
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
