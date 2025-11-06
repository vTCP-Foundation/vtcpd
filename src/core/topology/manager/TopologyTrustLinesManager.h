#ifndef VTCPD_TOPOLOGYTRUSTLINESMANAGER_H
#define VTCPD_TOPOLOGYTRUSTLINESMANAGER_H

#include "TopologyTrustLineWithPtr.h"
#include "../../contractors/addresses/BaseAddress.h"
#include "../../common/time/TimeUtils.h"
#include "../../logger/Logger.h"
#include "../../rates/Commission.h"

#include <set>
#include <unordered_map>
#include <optional>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

namespace as = boost::asio;

class TopologyTrustLinesManager
{

public:
    typedef unordered_set<TopologyTrustLineWithPtr *> TrustLineWithPtrHashSet;

public:
    TopologyTrustLinesManager(
        const SerializedEquivalent equivalent,
        BaseAddress::Shared ownAddress,
        bool iAmGateway,
        as::io_context &ioContext,
        Logger &logger);

    void addTrustLine(
        TopologyTrustLine::Shared trustLine);

    TrustLineWithPtrHashSet trustLinePtrsSet(
        ContractorID nodeID);

    /**
     * Removes a trust line between the given source and target contractors from the cached topology.
     *
     * The method ensures full cleanup of the associated bookkeeping structures (hash sets and
     * time-indexed maps) so callers never hold dangling pointers. If the last trust line for the
     * source node is removed, the node entry is purged as well.
     */
    void removeTrustLine(
        ContractorID sourceID,
        ContractorID targetID);

    void resetAllUsedAmounts();

    bool deleteLegacyTrustLines();

    size_t trustLinesCounts() const;

    // todo : this code used only for testing and should be deleted in future
    void printTrustLines() const;

    DateTime closestTimeEvent() const;

    void setPreventDeleting(
        bool preventDeleting);

    bool preventDeleting() const;

    void addUsedAmount(
        ContractorID sourceID,
        ContractorID targetID,
        const TrustLineAmount &amount);

    void makeFullyUsed(
        ContractorID sourceID,
        ContractorID targetID);

    void addGateway(
        ContractorID gateway);

    const set<ContractorID> gateways() const;

    void makeFullyUsedTLsFromGatewaysToAllNodesExceptOne(
        ContractorID exceptedNode);

    const TrustLineAmount &flowAmount(
        ContractorID source,
        ContractorID destination);

    void storeCommission(
        const ContractorID contractorID,
        const SerializedEquivalent equivalent,
        Commission::Shared commission);

    Commission::Shared getCommission(
        const ContractorID contractorID,
        const SerializedEquivalent equivalent) const;

    void removeCommission(
        const ContractorID contractorID,
        const SerializedEquivalent equivalent);

    void printCommissions() const;

public:
    static const ContractorID kCurrentNodeID = 0;
    static const uint32_t kCommissionsTTLSeconds = 300;

private:
    static const uint8_t kResetTrustLinesHours = 0;
    static const uint8_t kResetTrustLinesMinutes = 12;
    static const uint8_t kResetTrustLinesSeconds = 0;

    static Duration &kResetTrustLinesDuration()
    {
        static auto duration = Duration(
                                   kResetTrustLinesHours,
                                   kResetTrustLinesMinutes,
                                   kResetTrustLinesSeconds);
        return duration;
    }

    static const uint8_t kClearTrustLinesHours = 0;
    static const uint8_t kClearTrustLinesMinutes = 30;
    static const uint8_t kClearTrustLinesSeconds = 0;

    static Duration &kClearTrustLinesDuration()
    {
        static auto duration = Duration(
                                   kClearTrustLinesHours,
                                   kClearTrustLinesMinutes,
                                   kClearTrustLinesSeconds);
        return duration;
    }

private:
    LoggerStream info() const;

    LoggerStream debug() const;

    const string logHeader() const;

    void cleanupExpiredCommissions();
    
    // Commission expiry management
    void scheduleExpiryTimer();
    
    void onExpiryTimer(
        const boost::system::error_code &error);
    
    DateTime earliestCommissionExpiryTime() const;
    
    void removeExpiredCommissions();

private:
    unordered_map<ContractorID, TrustLineWithPtrHashSet *> msTrustLines;
    map<DateTime, TopologyTrustLineWithPtr *> mtTrustLines;
    SerializedEquivalent mEquivalent;
    Logger &mLog;
    bool mPreventDeleting;
    set<ContractorID> mGateways;
    DateTime mLastTrustLineTimeAdding;
    
    // Commission cache: map key <ContractorID, SerializedEquivalent> -> value <Commission::Shared, expiredAt>
    map<pair<ContractorID, SerializedEquivalent>, pair<Commission::Shared, DateTime>> mCommissionsCache;
    
    // Commission expiry timer
    as::io_context &mIOContext;
    unique_ptr<as::steady_timer> mCommissionExpiryTimer;
};

#endif // VTCPD_TOPOLOGYTRUSTLINESMANAGER_H
