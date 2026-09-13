#pragma once

#include "keydrop/core/types.hpp"

namespace keydrop {

enum class Endian {
    Little,
    Big
};

Endian system_endian();

// Compile-time endian detection (GCC, Clang).
// On little-endian targets the conversion is a no-op that the
// compiler eliminates entirely.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define KEYDROP_IS_LITTLE_ENDIAN 1
#else
#define KEYDROP_IS_LITTLE_ENDIAN 0
#endif

inline u16 to_little_endian(u16 value)
{
#if KEYDROP_IS_LITTLE_ENDIAN
    return value;
#else
    if (system_endian() == Endian::Little) return value;
    return static_cast<u16>((value >> 8) | (value << 8));
#endif
}

inline u32 to_little_endian(u32 value)
{
#if KEYDROP_IS_LITTLE_ENDIAN
    return value;
#else
    if (system_endian() == Endian::Little) return value;
    return ((value >> 24) & 0x000000FFu)
         | ((value >>  8) & 0x0000FF00u)
         | ((value <<  8) & 0x00FF0000u)
         | ((value << 24) & 0xFF000000u);
#endif
}

inline u64 to_little_endian(u64 value)
{
#if KEYDROP_IS_LITTLE_ENDIAN
    return value;
#else
    if (system_endian() == Endian::Little) return value;
    return ((value >> 56) & 0x00000000000000FFULL)
         | ((value >> 40) & 0x000000000000FF00ULL)
         | ((value >> 24) & 0x0000000000FF0000ULL)
         | ((value >>  8) & 0x00000000FF000000ULL)
         | ((value <<  8) & 0x000000FF00000000ULL)
         | ((value << 24) & 0x0000FF0000000000ULL)
         | ((value << 40) & 0x00FF000000000000ULL)
         | ((value << 56) & 0xFF00000000000000ULL);
#endif
}

inline u16 from_little_endian(u16 value)
{
    return to_little_endian(value);
}

inline u32 from_little_endian(u32 value)
{
    return to_little_endian(value);
}

inline u64 from_little_endian(u64 value)
{
    return to_little_endian(value);
}

} // namespace keydrop
