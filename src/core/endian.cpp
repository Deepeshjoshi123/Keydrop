#include "keydrop/core/endian.hpp"

namespace keydrop {

Endian system_endian()
{
    u16 value = 1;

    auto* ptr =
        reinterpret_cast<
            byte*
        >(
            &value
        );

    if (ptr[0] == 1)
    {
        return Endian::Little;
    }

    return Endian::Big;
}

u16 to_little_endian(
    u16 value
)
{
    if (
        system_endian()
        ==
        Endian::Little
    )
    {
        return value;
    }

    return
        (value >> 8)
        |
        (value << 8);
}

u32 to_little_endian(
    u32 value
)
{
    if (
        system_endian()
        ==
        Endian::Little
    )
    {
        return value;
    }

    return
        ((value>>24)&0xFF)
        |
        ((value>>8)&0xFF00)
        |
        ((value<<8)&0xFF0000)
        |
        ((value<<24));
}

u16 from_little_endian(
    u16 value
)
{
    return
        to_little_endian(
            value
        );
}

u32 from_little_endian(
    u32 value
)
{
    return
        to_little_endian(
            value
        );
}

}