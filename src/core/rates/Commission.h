#ifndef VTCPD_COMMISSION_H
#define VTCPD_COMMISSION_H

#include "../common/Types.h"

#include <memory>

using namespace std;

class Commission
{
public:
    typedef shared_ptr<Commission> Shared;

public:
    explicit Commission(const uint64_t amount);

    const uint64_t amount() const;

private:
    uint64_t mAmount;
};

#endif //VTCPD_COMMISSION_H