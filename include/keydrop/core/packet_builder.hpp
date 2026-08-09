#pragma once

#include <type_traits>

#include "keydrop/core/buffer.hpp"
#include "keydrop/core/types.hpp"

namespace keydrop {

class PacketBuilder {
public:

    PacketBuilder();

    template<typename T>
    void write(const T& value)
    {
        static_assert(
            std::is_trivially_copyable<T>::value,
            "PacketBuilder::write requires a trivially copyable type"
        );

        const auto* ptr =
            reinterpret_cast<const byte*>(&value);

        buffer_.append(
            ptr,
            sizeof(T)
        );
    }

    void write_bytes(
        const byte* data,
        usize size
    );

    const Buffer& buffer() const;

    void reserve(usize capacity);

    void clear();

private:

    Buffer buffer_;

};

}
