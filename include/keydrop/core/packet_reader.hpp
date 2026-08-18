#pragma once

#include <string>
#include <vector>

#include "keydrop/core/buffer.hpp"
#include "keydrop/core/types.hpp"

namespace keydrop {

class PacketReader {
public:
    explicit PacketReader(const Buffer& buffer);

    u8 read_u8();
    u16 read_u16();
    u32 read_u32();
    u64 read_u64();
    i8 read_i8();
    i16 read_i16();
    i32 read_i32();
    f32 read_f32();
    f64 read_f64();
    std::string read_string();
    std::string read_string_from_size(u16 size);
    std::vector<byte> read_bytes(usize size);

    // Advance the cursor without copying (bounds-checked).
    void skip(usize size);

    // The buffer being read. Combined with skip() and slice() this enables
    // zero-copy decoding of string/bytes fields as BufferView values.
    const Buffer& buffer() const;

    void reset();
    usize position() const;
    usize remaining() const;
    bool empty() const;

private:
    const Buffer& buffer_;
    usize cursor_ = 0;
};

}
