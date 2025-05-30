#include "src/core/Core.h"

#ifndef INTERNAL_TESTS
int main(int argc, char** argv)
{
    return Core(argv[0]).run();
}
#endif

#ifdef INTERNAL_TESTS
#include "tests.cpp"
#endif