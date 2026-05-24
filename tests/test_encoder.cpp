#include <cassert>
#include <cmath>
#include <cstring>

#include "keydrop/core/encoder.hpp"
#include "keydrop/core/packet_reader.hpp"

using namespace keydrop;

int main()
{
    Encoder encoder;
    encoder.write_u8(0x20);
    encoder.write_u16(1000);
    encoder.write_u32(50000);
    encoder.write_f32(25.7f);

    const Buffer& packet = encoder.buffer();
    assert(packet.size() == 11);

    assert(packet.read(0) == 0x20);
    assert(packet.read(1) == 0xE8);
    assert(packet.read(2) == 0x03);
    assert(packet.read(3) == 0x50);
    assert(packet.read(4) == 0xC3);
    assert(packet.read(5) == 0x00);
    assert(packet.read(6) == 0x00);

    PacketReader reader(packet);
    assert(reader.read_u8() == 0x20);
    assert(reader.read_u16() == 1000);
    assert(reader.read_u32() == 50000);

    const u32 f_bits = reader.read_u32();
    f32 f_value = 0.0f;
    std::memcpy(&f_value, &f_bits, sizeof(f_value));
    assert(std::fabs(f_value - 25.7f) < 0.0001f);

    return 0;
}
