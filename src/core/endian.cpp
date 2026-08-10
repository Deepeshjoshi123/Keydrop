#include "keydrop/core/endian.hpp"

namespace keydrop {

Endian system_endian()
{
    static const Endian e = []() {
        const u16 value = 1;
        const auto* ptr = reinterpret_cast<const byte*>(&value);
        return (ptr[0] == 1) ? Endian::Little : Endian::Big;
    }();
    return e;
}

} // namespace keydrop
