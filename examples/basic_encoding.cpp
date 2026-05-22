#include <iostream>

#include "keydrop/core/endian.hpp"

using namespace keydrop;

int main()
{
    auto system =
        system_endian();

    if (
        system
        ==
        Endian::Little
    )
    {
        std::cout
            << "Little Endian\n";
    }
    else
    {
        std::cout
            << "Big Endian\n";
    }

    return 0;
}