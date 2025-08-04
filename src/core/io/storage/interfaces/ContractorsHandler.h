#ifndef VTCPD_INTERFACES_CONTRACTORSHANDLER_H
#define VTCPD_INTERFACES_CONTRACTORSHANDLER_H

#include "../../../contractors/Contractor.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"

#include <vector>
#include <memory>

using namespace std;

class ContractorsHandler
{
public:
    virtual ~ContractorsHandler() = default;

    virtual void saveContractor(
        Contractor::Shared contractor) = 0;

    virtual void saveContractorFull(
        Contractor::Shared contractor) = 0;

    virtual void saveConfirmationInfo(
        Contractor::Shared contractor) = 0;

    virtual void updateCryptoKey(
        Contractor::Shared contractor) = 0;

    virtual void updateChannelIdOnContractorSide(
        Contractor::Shared contractor) = 0;

    virtual vector<Contractor::Shared> allContractors() = 0;

    virtual vector<ContractorID> allIDs() = 0;

    virtual void removeContractor(
        ContractorID contractorID) = 0;
};

#endif //VTCPD_INTERFACES_CONTRACTORSHANDLER_H 