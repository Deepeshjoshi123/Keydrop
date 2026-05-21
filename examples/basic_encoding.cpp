#include <cassert>
#include <iostream>

#include "keydrop/core/packet_builder.hpp"

using namespace keydrop;

int main()
{
    PacketBuilder builder;

    builder.write<u8>(1);
    builder.write<u16>(500);

    assert(builder.buffer().size() == 3);

    const auto& packet = builder.buffer();

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

    const byte raw[] = { 0xAA, 0xBB };
    builder.write_bytes(raw, 2);

    return 0;
}
