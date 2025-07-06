#ifndef VTCPD_INTERFACES_TRUSTLINEHANDLER_H
#define VTCPD_INTERFACES_TRUSTLINEHANDLER_H

#include "../../../trust_lines/TrustLine.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"

#include <vector>
#include <memory>

using namespace std;

class TrustLineHandler
{
public:
    virtual ~TrustLineHandler() = default;

    virtual void saveTrustLine(
        TrustLine::Shared trustLine,
        const SerializedEquivalent equivalent) = 0;

    virtual void updateTrustLineState(
        TrustLine::Shared trustLine,
        const SerializedEquivalent equivalent) = 0;

    virtual void updateTrustLineIsContractorGateway(
        TrustLine::Shared trustLine,
        const SerializedEquivalent equivalent) = 0;

    virtual vector<TrustLine::Shared> allTrustLinesByEquivalent(
        const SerializedEquivalent equivalent) = 0;

    virtual void deleteTrustLine(
        ContractorID contractorID,
        const SerializedEquivalent equivalent) = 0;

    virtual vector<SerializedEquivalent> equivalents() = 0;

    virtual vector<TrustLineID> allIDs() = 0;

    virtual vector<TrustLine::Shared> allTrustLinesByContractor(
        ContractorID contractorID) = 0;
};

#endif //VTCPD_INTERFACES_TRUSTLINEHANDLER_H 