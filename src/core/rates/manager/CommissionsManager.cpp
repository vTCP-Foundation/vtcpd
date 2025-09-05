#include "CommissionsManager.h"

CommissionsManager::CommissionsManager(
    Logger &logger,
    const vector<pair<SerializedEquivalent, uint64_t>> &commissions)
    : mLogger(logger)
{
    // Load commissions from pre-parsed configuration data
    for (const auto& [equivalent, amount] : commissions) {
        mCommissions[equivalent] = make_shared<Commission>(amount);
        debug() << "Loaded commission for equivalent " << equivalent 
               << ": amount=" << amount;
    }
    
    debug() << "CommissionsManager initialized with " << mCommissions.size() 
           << " commissions";
}

Commission::Shared CommissionsManager::getCommission(const SerializedEquivalent equivalent) const
{
    auto it = mCommissions.find(equivalent);
    if (it != mCommissions.end()) {
        return it->second;
    }
    return nullptr;
}

TrustLineAmount CommissionsManager::applyCommission(
    const TrustLineAmount &in,
    const SerializedEquivalent equivalent) const
{
    auto commission = getCommission(equivalent);
    if (!commission) {
        return in;
    }
    
    if (commission->amount() == 0) {
        return in;
    }
    
    TrustLineAmount commissionAmount(commission->amount());
    if (in < commissionAmount) {
        return TrustLine::kZeroAmount();
    }
    
    return in - commissionAmount;
}

void CommissionsManager::printCommissions() const
{
    if (mCommissions.empty()) {
        debug() << "No commissions";
        return;
    }
    
    debug() << "Commissions (" << mCommissions.size() << " entries):";
    
    for (const auto& entry : mCommissions) {
        SerializedEquivalent equivalent = entry.first;
        Commission::Shared commission = entry.second;
        
        debug() << ", Equivalent " << equivalent 
               << ": amount=" << commission->amount();
    }
}

LoggerStream CommissionsManager::debug() const
{
    return mLogger.debug(logHeader());
}

const string CommissionsManager::logHeader() const
{
    return "[CommissionsManager]";
}