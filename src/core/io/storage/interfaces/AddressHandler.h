#ifndef VTCPD_INTERFACES_ADDRESSHANDLER_H
#define VTCPD_INTERFACES_ADDRESSHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../contractors/addresses/BaseAddress.h"

#include <vector>
#include <memory>

using namespace std;

class AddressHandler
{
public:
    virtual ~AddressHandler() = default;

    virtual void saveAddress(
        ContractorID contractorID,
        BaseAddress::Shared address) = 0;

    virtual vector<BaseAddress::Shared> contractorAddresses(
        ContractorID contractorID) = 0;

    virtual void removeAddresses(
        ContractorID contractorID) = 0;
};

#endif //VTCPD_INTERFACES_ADDRESSHANDLER_H 