#ifndef VTCPD_PATHSMANAGER_H
#define VTCPD_PATHSMANAGER_H

#include "lib/PathsCollection.h"
#include "../trust_lines/manager/TrustLinesManager.h"
#include "../topology/manager/TopologyTrustLinesManager.h"
#include "../logger/Logger.h"

// Forward declaration to avoid circular dependency
class EquivalentsSubsystemsRouter;

#include <set>

class PathsManager
{

public:
    PathsManager(
        const SerializedEquivalent equivalent,
        TrustLinesManager *trustLinesManager,
        TopologyTrustLinesManager *topologyTrustLineManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        Logger &logger);

    void buildPaths(
        BaseAddress::Shared contractorAddress,
        ContractorID contractorID);

    void addUsedAmountFromInitiator(
        BaseAddress::Shared targetAddress,
        const TrustLineAmount &amount);

    void addUsedAmount(
        BaseAddress::Shared sourceAddress,
        BaseAddress::Shared targetAddress,
        const TrustLineAmount &amount);

    void makeTrustLineFullyUsed(
        BaseAddress::Shared sourceAddress,
        BaseAddress::Shared targetAddress);

    // this method used for rebuild paths in case of insufficient founds
    void reBuildPaths(
        BaseAddress::Shared contractorAddress,
        const vector<BaseAddress::Shared> &inaccessibleNodes);

    PathsCollection::Shared pathCollection() const;

    void clearPathsCollection();

private:
    bool isPathValid(const Path &path);

    void buildPathsOnOneLevel();

    void buildPathsOnSecondLevel();

    TrustLineAmount calculateOneNode(
        ContractorID nodeID,
        const TrustLineAmount &currentFlow,
        byte_t level);

    TrustLineAmount reBuildPathsOnOneLevel();

    TrustLineAmount calculateOneNodeForRebuildingPaths(
        ContractorID nodeID,
        const TrustLineAmount &currentFlow,
        byte_t level);

    vector<BaseAddress::Shared> addressesPath();

    LoggerStream info() const;

    const string logHeader() const;

private:
    static const byte_t kMaxPathLength = 6;

private:
    TrustLinesManager *mTrustLinesManager;
    TopologyTrustLinesManager *mTopologyTrustLinesManager;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    SerializedEquivalent mEquivalent;
    Logger &mLog;
    PathsCollection::Shared mPathCollection;
    BaseAddress::Shared mContractorAddress;
    ContractorID mContractorID;

    vector<ContractorID> mPassedNodeIDs;
    byte_t mCurrentPathLength;
    set<ContractorID> mInaccessibleNodes;
};

#endif // VTCPD_PATHSMANAGER_H
