#include <iostream>

#include "keydrop/core/buffer.hpp"

using namespace keydrop;

int main()
{
    Buffer buffer;

    buffer.write(32);
    buffer.write(70);

    std::cout
        << "Buffer Size: "
        << buffer.size()
        << "\n";

    std::cout
        << "Byte 0: "
        << static_cast<int>(
            buffer.read(0)
        )
        << "\n";

    std::cout
        << "Byte 1: "
        << static_cast<int>(
            buffer.read(1)
        )
        << "\n";

    return 0;
}