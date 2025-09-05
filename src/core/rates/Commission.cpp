#include "Commission.h"

Commission::Commission(const uint64_t amount) :
    mAmount(amount)
{
}

const uint64_t Commission::amount() const
{
    return mAmount;
}