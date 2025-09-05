#include "TopologyTrustLinesManager.h"
#include <chrono>

// Static constant definition
const ContractorID TopologyTrustLinesManager::kCurrentNodeID;
const uint32_t TopologyTrustLinesManager::kCommissionsTTLSeconds;

TopologyTrustLinesManager::TopologyTrustLinesManager(
    const SerializedEquivalent equivalent,
    BaseAddress::Shared ownAddress,
    bool iAmGateway,
    as::io_context &ioContext,
    Logger &logger):

    mEquivalent(equivalent),
    mLog(logger),
    mPreventDeleting(false),
    mIOContext(ioContext),
    mCommissionExpiryTimer(make_unique<as::steady_timer>(ioContext))
{
    // Use kCurrentNodeID for current node
    if (iAmGateway) {
        mGateways.insert(kCurrentNodeID);
    }
}

void TopologyTrustLinesManager::addTrustLine(
    TopologyTrustLine::Shared trustLine)
{
    auto const &nodeIDAndSetFlows = msTrustLines.find(trustLine->sourceID());
    if (nodeIDAndSetFlows == msTrustLines.end()) {
        if (*(trustLine->amount()) == TrustLine::kZeroAmount()) {
            return;
        }
        auto newHashSet = new unordered_set<TopologyTrustLineWithPtr*>();
        auto newTrustLineWithPtr = new TopologyTrustLineWithPtr(
            trustLine,
            newHashSet);
        newHashSet->insert(
            newTrustLineWithPtr);

        msTrustLines.insert(
            make_pair(
                trustLine->sourceID(),
                newHashSet));
        auto now = utc_now();
        if (mtTrustLines.count(now) != 0) {
            now += pt::microseconds(5);
        }
        mtTrustLines.insert(
            make_pair(
                now,
                newTrustLineWithPtr));
    } else {
        auto hashSet = nodeIDAndSetFlows->second;
        auto trLineWithPtrIt = hashSet->begin();
        while (trLineWithPtrIt != hashSet->end()) {
            if ((*trLineWithPtrIt)->topologyTrustLine()->targetID() == trustLine->targetID()) {
                (*trLineWithPtrIt)->topologyTrustLine()->setAmount(trustLine->amount());

                // update time creation of trustline
                auto dateTimeAndTrustLine = mtTrustLines.begin();
                while (dateTimeAndTrustLine != mtTrustLines.end()) {
                    if (dateTimeAndTrustLine->second == *trLineWithPtrIt) {
                        if (*(*trLineWithPtrIt)->topologyTrustLine()->amount() != TrustLine::kZeroAmount()) {
                            mtTrustLines.erase(
                                dateTimeAndTrustLine);
                            auto now = utc_now();
                            if (mtTrustLines.count(now) != 0) {
                                now += pt::microseconds(5);
                            }
                            mtTrustLines.insert(
                                make_pair(
                                    now,
                                    *trLineWithPtrIt));
                        } else {
                            auto trLineWithPtr = *trLineWithPtrIt;
                            hashSet->erase(trLineWithPtr);
                            delete trLineWithPtr;
                            mtTrustLines.erase(dateTimeAndTrustLine);
                        }
                        break;
                    }
                    dateTimeAndTrustLine++;
                }

                break;
            }
            trLineWithPtrIt++;
        }
        if (hashSet->empty()) {
            msTrustLines.erase(nodeIDAndSetFlows->first);
            delete hashSet;
        } else if (trLineWithPtrIt == hashSet->end()) {
            if (*trustLine->amount() == TrustLine::kZeroAmount()) {
                return;
            }
            auto newTrustLineWithPtr = new TopologyTrustLineWithPtr(
                trustLine,
                hashSet);
            hashSet->insert(
                newTrustLineWithPtr);
            auto now = utc_now();
            if (mtTrustLines.count(now) != 0) {
                now += pt::microseconds(5);
            }
            mtTrustLines.insert(
                make_pair(
                    now,
                    newTrustLineWithPtr));
        }
    }
    mLastTrustLineTimeAdding = utc_now();
}

unordered_set<TopologyTrustLineWithPtr*> TopologyTrustLinesManager::trustLinePtrsSet(
    ContractorID nodeID)
{
    auto const &nodeIDAndSetFlows = msTrustLines.find(nodeID);
    if (nodeIDAndSetFlows == msTrustLines.end()) {
        TrustLineWithPtrHashSet result;
        return result;
    }
    return *nodeIDAndSetFlows->second;
}

void TopologyTrustLinesManager::resetAllUsedAmounts()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "resetAllUsedAmounts";
#endif
    for (auto &nodeIDAndTrustLine : msTrustLines) {
        for (auto &trustLine : *nodeIDAndTrustLine.second) {
            trustLine->topologyTrustLine()->setUsedAmount(0);
        }
    }
}

void TopologyTrustLinesManager::addUsedAmount(
    ContractorID sourceID,
    ContractorID targetID,
    const TrustLineAmount &amount)
{
    auto const &nodeIDAndSetFlows = msTrustLines.find(sourceID);
    if (nodeIDAndSetFlows == msTrustLines.end()) {
        return;
    }
    for (auto &trustLinePtr : *nodeIDAndSetFlows->second) {
        if (trustLinePtr->topologyTrustLine()->targetID() == targetID) {
            trustLinePtr->topologyTrustLine()->addUsedAmount(amount);
            return;
        }
    }
}

void TopologyTrustLinesManager::makeFullyUsed(
    ContractorID sourceID,
    ContractorID targetID)
{
    auto const &nodeIDAndSetFlows = msTrustLines.find(sourceID);
    if (nodeIDAndSetFlows == msTrustLines.end()) {
        return;
    }
    for (auto &trustLinePtr : *nodeIDAndSetFlows->second) {
        if (trustLinePtr->topologyTrustLine()->targetID() == targetID) {
            trustLinePtr->topologyTrustLine()->setUsedAmount(
                *trustLinePtr->topologyTrustLine()->amount().get());
            return;
        }
    }
}

bool TopologyTrustLinesManager::deleteLegacyTrustLines()
{
    bool isTrustLineWasDeleted = false;
    if (mtTrustLines.empty()) {
        if (utc_now() - mLastTrustLineTimeAdding > kClearTrustLinesDuration()) {
            for (auto nodeIDAndSetFlows : msTrustLines) {
                auto hashSetPtr = nodeIDAndSetFlows.second;
                hashSetPtr->clear();
                delete hashSetPtr;
            }
            msTrustLines.clear();
        }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
        info() << "deleteLegacyTrustLines\t" << "map size after deleting: " << msTrustLines.size();
#endif
        return isTrustLineWasDeleted;
    }
    for (auto &timeAndTrustLineWithPtr : mtTrustLines) {
        if (utc_now() - timeAndTrustLineWithPtr.first > kResetTrustLinesDuration()) {
            auto trustLineWithPtr = timeAndTrustLineWithPtr.second;
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
            info() << "deleteLegacyTrustLines\t" <<
                   trustLineWithPtr->topologyTrustLine()->sourceID() << " " <<
                   trustLineWithPtr->topologyTrustLine()->targetID() << " " <<
                   trustLineWithPtr->topologyTrustLine()->amount();
#endif
            auto hashSetPtr = trustLineWithPtr->hashSetPtr();
            hashSetPtr->erase(trustLineWithPtr);
            if (hashSetPtr->empty()) {
                ContractorID keyID = trustLineWithPtr->topologyTrustLine()->sourceID();
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
                info() << "deleteLegacyTrustLines\t" << "remove all trustLines for node: " << keyID;
#endif
                msTrustLines.erase(keyID);
                delete hashSetPtr;
            }
            delete trustLineWithPtr;
            mtTrustLines.erase(timeAndTrustLineWithPtr.first);
            isTrustLineWasDeleted = true;
        } else {
            break;
        }
    }
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "deleteLegacyTrustLinesNew\t" << "map size after deleting: " << msTrustLines.size();
#endif
    return isTrustLineWasDeleted;
}

size_t TopologyTrustLinesManager::trustLinesCounts() const
{
    size_t countTrustLines = 0;
    for (const auto &contractoIDAndTrustLines : msTrustLines) {
        countTrustLines += (contractoIDAndTrustLines.second)->size();
    }
    return countTrustLines;
}

void TopologyTrustLinesManager::printTrustLines() const
{
    size_t trustLinesCnt = 0;
    debug() << "trustLineMap size: " << msTrustLines.size();
    for (const auto &nodeIDAndTrustLines : msTrustLines) {
        debug() << "key: " << nodeIDAndTrustLines.first;
        for (auto &itTrustLine : *nodeIDAndTrustLines.second) {
            TopologyTrustLine::Shared trustLine = itTrustLine->topologyTrustLine();
            debug() << "value: " << trustLine->targetID() << " " << *trustLine->amount().get()
                   << " free amount: " << *trustLine->freeAmount();
        }
        trustLinesCnt += nodeIDAndTrustLines.second->size();
    }
    debug() << "trust lines count: " << trustLinesCnt;

    debug() << "now is " << utc_now();
    debug() << "timesMap size: " << mtTrustLines.size();
    for (const auto &timeAndTrustLine : mtTrustLines) {
        debug() << "key: " << timeAndTrustLine.first;
        auto trustLine = timeAndTrustLine.second->topologyTrustLine();
        debug() << "value: " << trustLine->targetID() << " " << *trustLine->amount().get()
               << " free amount: " << *trustLine->freeAmount();
    }
}

DateTime TopologyTrustLinesManager::closestTimeEvent() const
{
    DateTime result = utc_now() + kResetTrustLinesDuration();
    // if there are cached trust lines, then take closest trust line removing time as result closest time event
    // else take trust line life time as result closest time event
    if (!mtTrustLines.empty()) {
        auto timeAndTrustLine = mtTrustLines.cbegin();
        if (timeAndTrustLine->first + kResetTrustLinesDuration() < result) {
            result = timeAndTrustLine->first + kResetTrustLinesDuration();
        }
    }
    return result;
}

void TopologyTrustLinesManager::addGateway(
    ContractorID gateway)
{
    mGateways.insert(gateway);
}

const set<ContractorID> TopologyTrustLinesManager::gateways() const
{
    return mGateways;
}

void TopologyTrustLinesManager::makeFullyUsedTLsFromGatewaysToAllNodesExceptOne(
    ContractorID exceptedNode)
{
    for (const auto &gateway : mGateways) {
        auto const &nodeIDAndSetFlows = msTrustLines.find(gateway);
        if (nodeIDAndSetFlows == msTrustLines.end()) {
            continue;
        }
        for (auto &trustLinePtr : *nodeIDAndSetFlows->second) {
            const auto maxFlowTLTarget = trustLinePtr->topologyTrustLine()->targetID();
            if (mGateways.count(maxFlowTLTarget) != 0) {
                continue;
            }
            if (maxFlowTLTarget != exceptedNode) {
                trustLinePtr->topologyTrustLine()->setUsedAmount(
                    *trustLinePtr->topologyTrustLine()->amount().get());
            }
        }
    }
}

const TrustLineAmount& TopologyTrustLinesManager::flowAmount(
    ContractorID source,
    ContractorID destination)
{
    auto const &nodeIDAndSetFlows = msTrustLines.find(source);
    if (nodeIDAndSetFlows == msTrustLines.end()) {
        return TrustLine::kZeroAmount();
    }
    for (auto &trustLinePtr : *nodeIDAndSetFlows->second) {
        if (trustLinePtr->topologyTrustLine()->targetID() == destination) {
            return *trustLinePtr->topologyTrustLine()->amount();
        }
    }
    return TrustLine::kZeroAmount();
}


void TopologyTrustLinesManager::setPreventDeleting(
    bool preventDeleting)
{
    mPreventDeleting = preventDeleting;
}

bool TopologyTrustLinesManager::preventDeleting() const
{
    return mPreventDeleting;
}

LoggerStream TopologyTrustLinesManager::info() const
{
    return mLog.info(logHeader());
}

LoggerStream TopologyTrustLinesManager::debug() const
{
    return mLog.debug(logHeader());
}

const string TopologyTrustLinesManager::logHeader() const
{
    stringstream s;
    s << "TopologyTrustLinesManager: " << mEquivalent << " ";
    return s.str();
}

void TopologyTrustLinesManager::storeCommission(
    const ContractorID contractorID,
    const SerializedEquivalent equivalent,
    Commission::Shared commission)
{
    if (!commission || commission->amount() == 0) {
        return;  // Don't store zero commissions
    }
    
    auto key = make_pair(contractorID, equivalent);
    auto expiryTime = utc_now() + boost::posix_time::seconds(kCommissionsTTLSeconds);
    auto value = make_pair(commission, expiryTime);
    
    auto it = mCommissionsCache.find(key);
    if (it != mCommissionsCache.end()) {
        // Update existing entry and refresh TTL
        it->second = value;
        debug() << "Updated commission for contractor " << contractorID 
               << " equivalent " << equivalent << " amount " << commission->amount();
    } else {
        // Insert new entry
        mCommissionsCache[key] = value;
        debug() << "Stored commission for contractor " << contractorID 
               << " equivalent " << equivalent << " amount " << commission->amount();
    }
    
    // Schedule expiry timer
    scheduleExpiryTimer();
}

Commission::Shared TopologyTrustLinesManager::getCommission(
    const ContractorID contractorID,
    const SerializedEquivalent equivalent) const
{
    auto key = make_pair(contractorID, equivalent);
    auto it = mCommissionsCache.find(key);
    
    if (it == mCommissionsCache.end()) {
        return nullptr;
    }
    
    // Check if entry has expired
    if (it->second.second < utc_now()) {
        return nullptr;
    }
    
    return it->second.first;
}

void TopologyTrustLinesManager::cleanupExpiredCommissions()
{
    auto now = utc_now();
    
    for (auto it = mCommissionsCache.begin(); it != mCommissionsCache.end();) {
        if (it->second.second < now) {
            debug() << "Removing expired commission for contractor " << it->first.first 
                   << " equivalent " << it->first.second;
            it = mCommissionsCache.erase(it);
        } else {
            ++it;
        }
    }
}

DateTime TopologyTrustLinesManager::earliestCommissionExpiryTime() const
{
    if (mCommissionsCache.empty()) {
        return utc_now() + boost::posix_time::hours(24);  // Return far future if no commissions
    }
    
    DateTime earliest = boost::posix_time::max_date_time;
    for (const auto& entry : mCommissionsCache) {
        if (entry.second.second < earliest) {
            earliest = entry.second.second;
        }
    }
    
    return earliest;
}

void TopologyTrustLinesManager::printCommissions() const
{
    if (mCommissionsCache.empty()) {
        debug() << "No commissions cached";
        return;
    }
    
    debug() << "Cached commissions (" << mCommissionsCache.size() << " entries):";
    
    for (const auto& entry : mCommissionsCache) {
        ContractorID contractorID = entry.first.first;
        SerializedEquivalent equivalent = entry.first.second;
        Commission::Shared commission = entry.second.first;
        const DateTime& expiryTime = entry.second.second;
        
        debug() << "  Contractor " << contractorID 
               << ", Equivalent " << equivalent 
               << ": amount=" << commission->amount() 
               << ", expires at " << expiryTime;
    }
}

void TopologyTrustLinesManager::removeExpiredCommissions()
{
    auto now = utc_now();
    auto it = mCommissionsCache.begin();
    
    size_t removedCount = 0;
    while (it != mCommissionsCache.end()) {
        if (it->second.second <= now) {  // expiryTime <= now
            debug() << "Removing expired commission for contractor " << it->first.first 
                   << ", equivalent " << it->first.second 
                   << ", amount " << it->second.first->amount();
            it = mCommissionsCache.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }
    
    if (removedCount > 0) {
        debug() << "Removed " << removedCount << " expired commissions, " 
               << mCommissionsCache.size() << " remain cached";
    }
}

void TopologyTrustLinesManager::scheduleExpiryTimer()
{
    if (mCommissionsCache.empty()) {
        return;
    }
    
    DateTime earliestExpiry = earliestCommissionExpiryTime();
    DateTime now = utc_now();
    
    if (earliestExpiry <= now) {
        // Some commissions have already expired, remove them immediately
        removeExpiredCommissions();
        if (mCommissionsCache.empty()) {
            return;
        }
        earliestExpiry = earliestCommissionExpiryTime();
    }
    
    auto delayMs = (earliestExpiry - now).total_milliseconds();
    if (delayMs < 0) {
        delayMs = 0;
    }
    
    debug() << "Scheduling commission expiry timer in " << delayMs << "ms";
    
    mCommissionExpiryTimer->expires_after(chrono::milliseconds(delayMs));
    mCommissionExpiryTimer->async_wait(boost::bind(
        &TopologyTrustLinesManager::onExpiryTimer,
        this,
        as::placeholders::error));
}

void TopologyTrustLinesManager::onExpiryTimer(
    const boost::system::error_code &error)
{
    if (error == as::error::operation_aborted) {
        return; // Timer was cancelled
    }
    
    if (error) {
        debug() << "Commission expiry timer error: " << error.message();
        return;
    }
    
    debug() << "Commission expiry timer fired, removing expired commissions";
    removeExpiredCommissions();
    
    // Reschedule timer if there are still commissions remaining
    if (!mCommissionsCache.empty()) {
        scheduleExpiryTimer();
    }
}
