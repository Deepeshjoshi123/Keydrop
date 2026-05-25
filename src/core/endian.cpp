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

u64 to_little_endian(
    u64 value
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
        ((value >> 56) & 0x00000000000000FFULL)
        |
        ((value >> 40) & 0x000000000000FF00ULL)
        |
        ((value >> 24) & 0x0000000000FF0000ULL)
        |
        ((value >> 8)  & 0x00000000FF000000ULL)
        |
        ((value << 8)  & 0x000000FF00000000ULL)
        |
        ((value << 24) & 0x0000FF0000000000ULL)
        |
        ((value << 40) & 0x00FF000000000000ULL)
        |
        ((value << 56) & 0xFF00000000000000ULL);
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

u64 from_little_endian(
    u64 value
)
{
    return
        to_little_endian(
            value
        );
}

}
