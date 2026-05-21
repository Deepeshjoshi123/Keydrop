#include <iostream>

#include "keydrop/core/packet_builder.hpp"

using namespace keydrop;

int main()
{
    PacketBuilder builder;

    builder.write_u8(32);

    builder.write_u16(1000);

    builder.write_u32(50000);

    const auto& packet =
        builder.buffer();

    std::cout
        << "Packet Size: "
        << packet.size()
        << "\n";

    for (
        usize i = 0;
        i < packet.size();
        ++i
    )
    {
        std::cout
            << static_cast<int>(
                packet.read(i)
            )
            << " ";
    }

    std::cout
        << "\n";

    return 0;
}