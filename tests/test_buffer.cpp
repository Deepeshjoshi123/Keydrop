#include <cassert>
#include <stdexcept>

#include "keydrop/core/buffer.hpp"

using namespace keydrop;

int main()
{
    Buffer buffer;
    assert(buffer.empty());
    assert(buffer.bytes() == nullptr);
    assert(buffer.mutable_bytes() == nullptr);

    buffer.reserve(8);
    buffer.write(0xAA);
    buffer.write(0xBB);
    buffer.write(0xCC);
    assert(buffer.size() == 3);
    assert(buffer.read(0) == 0xAA);
    assert(buffer.bytes()[1] == 0xBB);

    BufferView view = buffer.view();
    assert(!view.empty());
    assert(view.size() == 3);
    assert(view.read(2) == 0xCC);

    BufferView slice = buffer.slice(1, 2);
    assert(slice.size() == 2);
    assert(slice.read(0) == 0xBB);
    assert(slice.read(1) == 0xCC);

    Buffer copied;
    copied.append(slice);
    assert(copied.size() == 2);
    assert(copied.read(0) == 0xBB);
    assert(copied.read(1) == 0xCC);

    Buffer copied_buffer;
    copied_buffer.append(copied);
    assert(copied_buffer.size() == 2);
    assert(copied_buffer.read(0) == 0xBB);

    buffer.resize(4);
    buffer.mutable_bytes()[3] = 0xDD;
    assert(buffer.read(3) == 0xDD);

    BufferView empty_slice = buffer.slice(2, 0);
    assert(empty_slice.empty());

    bool threw = false;
    try
    {
        (void)buffer.slice(3, 4);
    }
    catch (const std::out_of_range&)
    {
        threw = true;
    }
    assert(threw);

    threw = false;
    try
    {
        BufferView invalid;
        (void)invalid.read(0);
    }
    catch (const std::out_of_range&)
    {
        threw = true;
    }
    assert(threw);

    buffer.clear();
    assert(buffer.empty());
    assert(buffer.bytes() == nullptr);

    return 0;
}
