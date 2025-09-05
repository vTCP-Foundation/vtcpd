#ifndef VTCPD_COMMISSIONSMANAGER_H
#define VTCPD_COMMISSIONSMANAGER_H

#include "../Commission.h"
#include "../../logger/Logger.h"
#include "../../common/Types.h"
#include "../../trust_lines/TrustLine.h"

#include <map>
#include <optional>
#include <memory>

using namespace std;

class CommissionsManager
{
public:
    CommissionsManager(
        Logger &logger,
        const vector<pair<SerializedEquivalent, uint64_t>> &commissions);

public:
    Commission::Shared getCommission(const SerializedEquivalent equivalent) const;
    
    TrustLineAmount applyCommission(const TrustLineAmount &in, const SerializedEquivalent equivalent) const;

    void printCommissions() const;

private:
    LoggerStream debug() const;
    const string logHeader() const;

public:
    static const uint32_t kCommissionsTTLSeconds = 300;

private:
    Logger &mLogger;
    map<SerializedEquivalent, Commission::Shared> mCommissions;
};

#endif //VTCPD_COMMISSIONSMANAGER_H