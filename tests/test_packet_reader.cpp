#include <cassert>
#include <stdexcept>

#include "keydrop/core/packet_builder.hpp"
#include "keydrop/core/packet_reader.hpp"

using namespace keydrop;

int main()
{
    PacketBuilder builder;
    builder.write<u8>(32);
    builder.write<u16>(1000);
    builder.write<u32>(50000);

    PacketReader reader(builder.buffer());

    assert(reader.read_u8() == 32);
    assert(reader.read_u16() == 1000);
    assert(reader.read_u32() == 50000);
    assert(reader.empty());

    bool threw = false;
    try
    {
        (void)reader.read_u8();
    }
    catch (const std::out_of_range&)
    {
        threw = true;
    }

    assert(threw);
    return 0;
}
