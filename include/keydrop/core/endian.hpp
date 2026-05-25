#pragma once

#include "keydrop/core/types.hpp"

namespace keydrop {

enum class Endian {
    Little,
    Big
};

Endian system_endian();

u16 to_little_endian(
    u16 value
);

u32 to_little_endian(
    u32 value
);

u64 to_little_endian(
    u64 value
);

u16 from_little_endian(
    u16 value
);

u32 from_little_endian(
    u32 value
);

u64 from_little_endian(
    u64 value
);

}
