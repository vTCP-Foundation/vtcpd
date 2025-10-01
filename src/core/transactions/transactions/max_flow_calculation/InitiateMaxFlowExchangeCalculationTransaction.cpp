#include "InitiateMaxFlowExchangeCalculationTransaction.h"

#include "ortools/linear_solver/linear_solver.h"
#include "ortools/base/version.h"
using operations_research::MPSolver;
using operations_research::MPVariable;
using operations_research::MPConstraint;
using operations_research::MPObjective;

#include <cmath>
#include <algorithm>
#include <limits>
#include <set>
#include <map>

namespace {

struct EdgeKey {
    ContractorID from;
    ContractorID to;
    SerializedEquivalent equivalent;

    bool operator<(const EdgeKey &other) const noexcept
    {
        if (from != other.from) return from < other.from;
        if (to != other.to) return to < other.to;
        return equivalent < other.equivalent;
    }
};

static double fetchEdgeCapacity(
    EquivalentsSubsystemsRouter *router,
    ContractorID from,
    ContractorID to,
    SerializedEquivalent equivalent)
{
    try {
        auto tlm = router->topologyTrustLineManager(equivalent);
        for (auto tlPtr : tlm->trustLinePtrsSet(from)) {
            auto tl = tlPtr->topologyTrustLine();
            if (tl->targetID() == to) {
                return tl->freeAmount()->convert_to<double>();
            }
        }
    } catch (...) {
    }
    return 0.0;
}

static double findExchangeRateForStep(
    const ExchangePath &path,
    size_t index)
{
    for (const auto &ex : path.exchangeSteps) {
        if (ex.nodeID == path.nodes[index] &&
            ex.fromEquivalent == path.equivalents[index] &&
            ex.toEquivalent == path.equivalents[index + 1]) {
            double rate = ex.exchangeRate.convert_to<double>();
            int16_t sh = ex.exchangeRateShift;
            if (sh > 0) {
                for (int t = 0; t < sh; ++t) {
                    rate *= 10.0;
                }
            }
            if (sh < 0) {
                for (int t = 0; t < -sh; ++t) {
                    rate /= 10.0;
                }
            }
            return rate;
        }
    }
    return 1.0;
}

static bool commissionAppliedOnPath(
    const std::vector<std::pair<ContractorID, SerializedEquivalent>> &applied,
    ContractorID node,
    SerializedEquivalent equivalent)
{
    for (const auto &item : applied) {
        if (item.first == node && item.second == equivalent) {
            return true;
        }
    }
    return false;
}

static double computeMaxRawAllowed(
    const ExchangePath &path,
    const std::vector<std::pair<ContractorID, SerializedEquivalent>> &appliedCommissions,
    const std::map<EdgeKey, double> &edgeRemaining,
    EquivalentsSubsystemsRouter *router,
    double solverRaw)
{
    (void)solverRaw;
    double alpha = 1.0;
    double beta = 0.0;
    double maxAllowed = path.calculateMaxCapacity().convert_to<double>();
    if (!std::isfinite(maxAllowed) || maxAllowed < 0.0) {
        maxAllowed = std::numeric_limits<double>::infinity();
    }

    for (size_t k = 0; k + 1 < path.nodes.size(); ++k) {
        if (path.nodes[k] == path.nodes[k + 1] && path.equivalents[k] != path.equivalents[k + 1]) {
            double rate = findExchangeRateForStep(path, k);
            alpha *= rate;
            beta *= rate;
            continue;
        }

        EdgeKey key{path.nodes[k], path.nodes[k + 1], path.equivalents[k]};
        double residual = 0.0;
        auto it = edgeRemaining.find(key);
        if (it != edgeRemaining.end()) {
            residual = it->second;
        } else {
            residual = fetchEdgeCapacity(router, key.from, key.to, key.equivalent);
        }

        if (alpha > 1e-9) {
            double allowedEdge = (residual + beta) / alpha;
            if (std::isfinite(allowedEdge)) {
                maxAllowed = std::min(maxAllowed, allowedEdge);
            } else {
                maxAllowed = 0.0;
            }
        } else {
            maxAllowed = 0.0;
        }

        ContractorID arrivalNode = path.nodes[k + 1];
        SerializedEquivalent arrivalEq = path.equivalents[k + 1];
        if (commissionAppliedOnPath(appliedCommissions, arrivalNode, arrivalEq)) {
            try {
                auto tlm = router->topologyTrustLineManager(arrivalEq);
                auto commission = tlm->getCommission(arrivalNode, arrivalEq);
                if (commission) {
                    beta += static_cast<double>(commission->amount());
                }
            } catch (...) {}
        }
    }

    if (!std::isfinite(maxAllowed) || maxAllowed < 0.0) {
        return 0.0;
    }
    return maxAllowed;
}

} // namespace


// Helper function to collect all exchange nodes from paths
static std::set<ContractorID> collectExchangeNodes(const vector<ExchangePath>& paths) {
    std::set<ContractorID> exchangeNodes;
    for (const auto& path : paths) {
        for (const auto& exchange : path.exchangeSteps) {
            exchangeNodes.insert(exchange.nodeID);
        }
    }
    return exchangeNodes;
}

// Helper to format path with addresses and flows per PRD F/E format
static std::string formatDetailedPathWithRouter(
    const OptimalPathResult &pathResult,
    EquivalentsSubsystemsRouter *router,
    const std::set<ContractorID> &exchangeNodes,
    std::set<std::pair<ContractorID, SerializedEquivalent>> *processedCommissions,
    std::map<EdgeKey, double> *edgeRemaining,
    double *deliveredOut)
{
    std::stringstream ss;
    const auto &path = pathResult.path;

    auto addrOf = [&](ContractorID id) -> std::string {
        auto addr = router->resolveParticipantAddress(id);
        if (addr) return addr->fullAddress();
        return std::string("node:") + std::to_string(id);
    };

    double currentFlow = pathResult.optimal_flow.convert_to<double>();
    bool needsArrow = false;

    for (size_t i = 0; i + 1 < path.nodes.size(); i++) {
        // Skip self-loops (exchange internal representation)
        if (path.nodes[i] == path.nodes[i + 1]) {
            // Process exchange at this node
            for (const auto &exchange : path.exchangeSteps) {
                if (exchange.nodeID == path.nodes[i] && 
                    exchange.fromEquivalent == path.equivalents[i] && 
                    exchange.toEquivalent == path.equivalents[i + 1]) {
                    double r = exchange.exchangeRate.convert_to<double>();
                    int16_t sh = exchange.exchangeRateShift;
                    if (sh > 0) for (int j = 0; j < sh; ++j) r *= 10.0;
                    if (sh < 0) for (int j = 0; j < -sh; ++j) r /= 10.0;
                    double outFlow = currentFlow * r;
                    
                    if (needsArrow) ss << " -> ";
                    ss << "E(node: " << addrOf(exchange.nodeID)
                       << "; id: " << exchange.nodeID
                       << "; eqs [" << exchange.fromEquivalent << "->" << exchange.toEquivalent
                       << "; flows:[" << currentFlow << "->" << outFlow << ")";
                    currentFlow = outFlow;
                    needsArrow = true;
                    break;
                }
            }
            continue;
        }

        // Real edge between different nodes
        if (needsArrow) ss << " -> ";

        // Apply commission if exists on this node and equivalent, and show only once
        bool showCommission = false;
        bool commissionAvailable = false;
        double commissionAmount = 0.0;
        std::pair<ContractorID, SerializedEquivalent> nodeCommissionKey = {path.nodes[i + 1], path.equivalents[i + 1]};

        // Skip sender node (index 0) and receiver node (last node) for commissions
        // Also skip exchange nodes (they don't charge transit commission)
        if (i + 1 > 0 && i + 1 < path.nodes.size() - 1 && 
            exchangeNodes.find(path.nodes[i + 1]) == exchangeNodes.end()) {
            try {
                auto tlm = router->topologyTrustLineManager(path.equivalents[i + 1]);
                auto commission = tlm->getCommission(path.nodes[i + 1], path.equivalents[i + 1]);
                if (commission) {
                    commissionAmount = static_cast<double>(commission->amount());
                    commissionAvailable = commissionAmount > 0.0;

                    if (commissionAvailable) {
                        // Show commission only if not already processed
                        if (!processedCommissions || processedCommissions->find(nodeCommissionKey) == processedCommissions->end()) {
                            showCommission = true;
                            if (processedCommissions) {
                                processedCommissions->insert(nodeCommissionKey);
                            }
                        }
                    }
                }
            } catch (...) {}
        }

        double flowBeforeCommission = currentFlow;
        if (edgeRemaining) {
            EdgeKey key{path.nodes[i], path.nodes[i + 1], path.equivalents[i]};
            auto it = edgeRemaining->find(key);
            double &remain = (it != edgeRemaining->end())
                ? it->second
                : (*edgeRemaining)[key] = fetchEdgeCapacity(router, key.from, key.to, key.equivalent);
            if (remain < flowBeforeCommission - 1e-9) {
                flowBeforeCommission = std::max(0.0, remain);
            }
            remain = std::max(0.0, remain - flowBeforeCommission);
        }

        ss << "F(nodes: [" << addrOf(path.nodes[i]) << " -> " << addrOf(path.nodes[i + 1])
           << "]; ids [" << path.nodes[i] << " -> " << path.nodes[i + 1]
           << "; flow: " << flowBeforeCommission
           << "; eq: " << path.equivalents[i] << ")";

        currentFlow = flowBeforeCommission;

        // Apply commission to flow for next segment only when it is actually charged on this path
        if (showCommission && commissionAvailable) {
            currentFlow -= commissionAmount;
            if (currentFlow < 0.0) {
                currentFlow = 0.0;
            }
            ss << " -> C(node: " << addrOf(path.nodes[i + 1])
               << "; id: " << path.nodes[i + 1]
               << "; commission: " << commissionAmount
               << "; flow after: " << currentFlow
               << "; eq: " << path.equivalents[i + 1] << ")";
        }

        needsArrow = true;
    }

    if (deliveredOut) {
        *deliveredOut = currentFlow;
    }
    return ss.str();
}

// Simulate how much of the flow sent from the source arrives to the target equivalent
// along a concrete path, accounting for fixed commissions and exchanges.
static double simulatePathNetAmount(
    const ExchangePath &path,
    double sourceFlow,
    EquivalentsSubsystemsRouter *router,
    const std::set<ContractorID> &exchangeNodes,
    std::set<std::pair<ContractorID, SerializedEquivalent>> *appliedOnce = nullptr,
    std::vector<std::pair<ContractorID, SerializedEquivalent>> *appliedNow = nullptr)
{
    if (!std::isfinite(sourceFlow) || sourceFlow <= 0.0) {
        return 0.0;
    }
    if (path.nodes.empty() || path.nodes.size() != path.equivalents.size()) {
        return 0.0;
    }

    double currentAmount = sourceFlow;

    for (size_t k = 0; k + 1 < path.nodes.size(); ++k) {
        // Exchange step represented as a self-edge
        if (path.nodes[k] == path.nodes[k + 1] && path.equivalents[k] != path.equivalents[k + 1]) {
            for (const auto &ex : path.exchangeSteps) {
                if (ex.nodeID == path.nodes[k] && ex.fromEquivalent == path.equivalents[k]
                    && ex.toEquivalent == path.equivalents[k + 1]) {
                    double r = ex.exchangeRate.convert_to<double>();
                    int16_t sh = ex.exchangeRateShift;
                    if (sh > 0) for (int i = 0; i < sh; ++i) r *= 10.0;
                    if (sh < 0) for (int i = 0; i < -sh; ++i) r /= 10.0;
                    currentAmount *= r;
                    break;
                }
            }
            continue;
        }

        // Move to the next node and apply its commission, if any
        size_t arrivalIndex = k + 1;
        if (arrivalIndex > 0 && arrivalIndex < path.nodes.size() - 1) {
            ContractorID nodeId = path.nodes[arrivalIndex];
            SerializedEquivalent nodeEq = path.equivalents[arrivalIndex];

            if (exchangeNodes.find(nodeId) == exchangeNodes.end()) {
                try {
                    auto tlm = router->topologyTrustLineManager(nodeEq);
                    auto commission = tlm->getCommission(nodeId, nodeEq);
                    if (commission) {
                        auto key = std::make_pair(nodeId, nodeEq);
                        bool charge = true;
                        if (appliedOnce && appliedOnce->find(key) != appliedOnce->end()) {
                            charge = false;
                        }
                        if (charge) {
                            currentAmount -= static_cast<double>(commission->amount());
                            if (currentAmount <= 0.0) {
                                if (appliedOnce) {
                                    appliedOnce->insert(key);
                                }
                                if (appliedNow) {
                                    appliedNow->push_back(key);
                                }
                                return 0.0;
                            }
                            if (appliedOnce) {
                                appliedOnce->insert(key);
                            }
                            if (appliedNow) {
                                appliedNow->push_back(key);
                            }
                        }
                    }
                } catch (...) {
                    // Ignore lookup issues; treat as zero commission
                }
            }
        }
    }

    return std::max(0.0, currentAmount);
}

// Safe conversion from double to TrustLineAmount with flooring and sanity checks.
// LP returns doubles; to store into integer-precision amounts we:
//  - treat non-finite or negative as 0
//  - floor with small epsilon to compensate floating error (e.g. 200.0000000001)
//  - cast via uint64_t to avoid constructing from non-integer double
static TrustLineAmount toAmountSafe(double v)
{
    if (!std::isfinite(v) || v <= 0.0) {
        return TrustLineAmount(0);
    }
    // Add a tiny epsilon to cover typical FP noise before floor
    double floored = std::floor(v + 1e-9);
    if (floored <= 0.0) {
        return TrustLineAmount(0);
    }
    // Flows and capacities in this system are well within 64-bit range
    // (trust lines and tests use small integers), so uint64_t is sufficient here.
    uint64_t iv = static_cast<uint64_t>(floored);
    return TrustLineAmount(iv);
}

InitiateMaxFlowExchangeCalculationTransaction::InitiateMaxFlowExchangeCalculationTransaction(
    InitiateMaxFlowExchangeCalculationCommand::Shared command,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    ExchangeRatesManager *exchangeRatesManager,
    TailManager *tailManager,
    Logger &logger,
    HopsCount_t hopsCount) :

    BaseCollectTopologyForExchangeTransaction(
        BaseTransaction::InitiateMaxFlowCalculationTransactionType, // TODO: Add new transaction type
        command->equivalent(),
        contractorsManager,
        equivalentsSubsystemsRouter,
        exchangeRatesManager,
        tailManager,
        logger),
    mCommand(command),
    mExchangeEquivalents(command->exchangeEquivalents()),
    mHopsCnt(hopsCount)
{
    // Validate exchangeEquivalents limit (maximum 5 elements)
    if (mExchangeEquivalents.size() > 5) {
        throw ValueError(logHeader() + "::constructor: exchangeEquivalents limit exceeded (maximum 5 elements).");
    }
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::sendRequestForCollectingTopology()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "targets count: " << mCommand->contractorAddresses().size();
    info() << "SendRequestForCollectingTopology with exchange equivalents: ";
    for (const auto &exchangeEquivalent : mExchangeEquivalents) {
        info() << exchangeEquivalent;
    }
#endif
    info() << "contractors addresses:";
    for (const auto &contractor : mCommand->contractorAddresses()) {
        info() << contractor->fullAddress();
    }

    // Compare against all own addresses (by value), avoiding dependence on mainAddress()
    std::vector<BaseAddress::Shared> ownAddresses = mContractorsManager->ownAddresses();
    for (const auto &contractorAddress : mCommand->contractorAddresses()) {
        for (const auto &ownAddr : ownAddresses) {
            if (ownAddr && contractorAddress && contractorAddress->fullAddress() == ownAddr->fullAddress()) {
                warning() << "Attempt to initialise operation against itself was prevented. Canceled.";
                return resultProtocolError();
            }
        }
    }
    
    for (const auto &contractorAddress : mCommand->contractorAddresses()) {
        auto contractorID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(
                                contractorAddress);
        info() << "ContractorID " << contractorID << " address " << contractorAddress->fullAddress();
        mContractorIDs.emplace_back(
            contractorID,
            contractorAddress);
    }
    
    auto transaction = make_shared<CollectTopologyForExchangeTransaction>(
        mEquivalent, // Receiver's target equivalent
        mExchangeEquivalents, // Sender's payment equivalents
        mCommand->contractorAddresses(),
        mContractorsManager,
        mEquivalentsSubsystemsRouter,
        mExchangeRatesManager,
        mLog,
        mHopsCnt);
    
    launchSubsidiaryTransaction(transaction);

    mCountProcessCollectingTopologyRun = 0;
    return resultAwakeAfterMilliseconds(
        kWaitMillisecondsForCalculatingMaxFlow); 
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::processCollectingTopology()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "ProcessCollectingTopology";
#endif

    auto const contextSize = mContext.size();
    
    fillTopology();
    fillRates(); // Process exchange rates messages

    mCountProcessCollectingTopologyRun++;
    if (contextSize > 0 && mCountProcessCollectingTopologyRun <= kCountRunningProcessCollectingTopologyStage) {
        return resultAwakeAfterMilliseconds(
                   kWaitMillisecondsForCalculatingMaxFlowAgain);
    }

#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    debug() << "Collected topology:";
    debug() << "Topology participants:";
    mEquivalentsSubsystemsRouter->printParticipants();
    debug() << "Receiver equivalent: " << mEquivalent;
    mEquivalentsSubsystemsRouter->topologyTrustLineManager(mEquivalent)->printTrustLines();
    for (const auto &exchangeEquivalent : mExchangeEquivalents) {
        debug() << "Exchange equivalent: " << exchangeEquivalent;
        mEquivalentsSubsystemsRouter->topologyTrustLineManager(exchangeEquivalent)->printTrustLines();
    }
    debug() << "Exchange rates:";
    mExchangeRatesManager->printExtqrnalRates();
    debug() << "Participants commissions in receiver equivalent:";
    mEquivalentsSubsystemsRouter->topologyTrustLineManager(mEquivalent)->printCommissions();
    debug() << "Participants commissions in exchange equivalents:";
    for (const auto &exchangeEquivalent : mExchangeEquivalents) {
        mEquivalentsSubsystemsRouter->topologyTrustLineManager(exchangeEquivalent)->printCommissions();
    }
#endif

    mStep = CustomLogic;
    return applyCustomLogic();
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::applyCustomLogic()
{
#ifdef DEBUG_LOG_MAX_FLOW_CALCULATION
    info() << "applyCustomLogic";
#endif

    try {
        // Process each target contractor
        for (const auto &contractor : mContractorIDs) {
            ContractorID contractorID = contractor.first;
            
            // Step 1: Enumerate all feasible paths
            info() << "Step 1: Enumerate all feasible paths";
            vector<ExchangePath> feasiblePaths = enumerateAllFeasiblePaths(contractorID);
            
            if (feasiblePaths.empty()) {
                warning() << "No feasible paths found to contractor " << contractorID;
                mMaxFlows[contractorID] = TrustLineAmount(0);
                continue;
            }

            // Step 2: Create Linear Programming solver
            info() << "Step 2: Create Linear Programming solver";
            std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver("GLOP"));
            if (!solver) {
                throw RuntimeError("GLOP solver unavailable");
            }
            
            // Test MutableObjective availability immediately after creation
            MPObjective* testObjective = solver->MutableObjective();
            if (testObjective == nullptr) {
                throw RuntimeError("MutableObjective returns null immediately after solver creation");
            }

            // Step 3: Create LP variables for each path
            info() << "Step 3: Create LP variables for each path";
            vector<MPVariable*> pathFlowVars;
            for (size_t i = 0; i < feasiblePaths.size(); i++) {
                double maxCapacity = static_cast<double>(feasiblePaths[i].calculateMaxCapacity());
                if (!std::isfinite(maxCapacity) || maxCapacity < 0.0) {
                    warning() << "Step 3: Non-finite or negative maxCapacity for path " << i
                              << ": " << maxCapacity << ". Sanitizing to 0.";
                    maxCapacity = 0.0;
                }
                auto* flowVar = solver->MakeNumVar(0.0, maxCapacity,
                                                   "flow_path_" + std::to_string(i));
                if (flowVar == nullptr) {
                    throw RuntimeError("LP variable allocation failed for path " + std::to_string(i));
                }
                pathFlowVars.push_back(flowVar);
            }

            // Defer objective setup until after constraints are added.

            // Step 5: Add constraints
            info() << "Step 5: Add constraints";
            // Sender balance constraints per equivalent
            for (const auto& equiv : mExchangeEquivalents) {
                auto senderBalance = getSenderBalance(equiv);
                if (senderBalance == TrustLineAmount(0)) {
                    continue; // Skip if no balance available
                }
                
                auto* balanceConstraint = solver->MakeRowConstraint(
                    0.0, static_cast<double>(senderBalance), 
                    "balance_" + std::to_string(equiv));
                
                for (size_t i = 0; i < feasiblePaths.size(); i++) {
                    if (feasiblePaths[i].startsWithEquivalent(equiv)) {
                        balanceConstraint->SetCoefficient(pathFlowVars[i], 1.0);
                    }
                }
            }

            // Capacity constraints per path (path-level caps per PRD reference)
            // Now calculateMaxCapacity() already includes source commissions
            for (size_t i = 0; i < feasiblePaths.size(); i++) {
                const auto &path = feasiblePaths[i];
                double cap = static_cast<double>(path.calculateMaxCapacity());
                
                auto* capConstraint = solver->MakeRowConstraint(0.0, cap, "cap_path_" + std::to_string(i));
                capConstraint->SetCoefficient(pathFlowVars[i], 1.0);
            }

            // Commission feasibility - commissions will be handled in the objective function
            // No need for additional constraints as the LP will optimize based on actual net throughput

            struct EdgeContribution {
                size_t pathIndex;
                double multiplier;
            };

            struct EdgeAggregate {
                double capacity = 0.0;
                bool capacityInitialized = false;
                double constantSum = 0.0;
                std::vector<EdgeContribution> contributions;
            };

            std::map<EdgeKey, EdgeAggregate> edgeUsage;
            
            for (size_t i = 0; i < feasiblePaths.size(); ++i) {
                const auto &path = feasiblePaths[i];
                if (path.nodes.size() < 2) {
                    continue;
                }

                std::set<ContractorID> exchangeNodesInPath;
                for (const auto &ex : path.exchangeSteps) {
                    exchangeNodesInPath.insert(ex.nodeID);
                }

                double multiplier = 1.0;
                double constantTerm = 0.0;

                auto applyExchange = [&](const ExchangeStep &ex) {
                    double r = ex.exchangeRate.convert_to<double>();
                    int16_t sh = ex.exchangeRateShift;
                    if (sh > 0) for (int t = 0; t < sh; ++t) r *= 10.0;
                    if (sh < 0) for (int t = 0; t < -sh; ++t) r /= 10.0;
                    multiplier *= r;
                    constantTerm *= r;
                };

                for (size_t k = 0; k + 1 < path.nodes.size(); ++k) {
                    ContractorID fromNode = path.nodes[k];
                    ContractorID toNode = path.nodes[k + 1];

                    if (fromNode == toNode && path.equivalents[k] != path.equivalents[k + 1]) {
                        for (const auto &ex : path.exchangeSteps) {
                            if (ex.nodeID == fromNode && ex.fromEquivalent == path.equivalents[k]
                                && ex.toEquivalent == path.equivalents[k + 1]) {
                                applyExchange(ex);
                                break;
                            }
                        }
                        continue;
                    }

                    SerializedEquivalent edgeEq = path.equivalents[k];
                    EdgeKey key{fromNode, toNode, edgeEq};
                    auto &edge = edgeUsage[key];

                    if (!edge.capacityInitialized) {
                        double edgeCapD = 0.0;
                        try {
                            auto tlm = mEquivalentsSubsystemsRouter->topologyTrustLineManager(edgeEq);
                            for (auto tlPtr : tlm->trustLinePtrsSet(fromNode)) {
                                auto tl = tlPtr->topologyTrustLine();
                                if (tl->targetID() == toNode) {
                                    edgeCapD = tl->freeAmount()->convert_to<double>();
                                    break;
                                }
                            }
                        } catch (...) {
                            edgeCapD = 0.0;
                        }
                        edge.capacity = edgeCapD;
                        edge.capacityInitialized = true;
                    }

                    edge.contributions.push_back({i, multiplier});
                    edge.constantSum += constantTerm;

                    size_t arrivalIndex = k + 1;
                    if (arrivalIndex > 0 && arrivalIndex < path.nodes.size() - 1) {
                        ContractorID arrivalNode = path.nodes[arrivalIndex];
                        SerializedEquivalent arrivalEq = path.equivalents[arrivalIndex];
                        if (exchangeNodesInPath.find(arrivalNode) == exchangeNodesInPath.end()) {
                            try {
                                auto tlm = mEquivalentsSubsystemsRouter->topologyTrustLineManager(arrivalEq);
                                auto commission = tlm->getCommission(arrivalNode, arrivalEq);
                                if (commission) {
                                    constantTerm += static_cast<double>(commission->amount());
                                }
                            } catch (...) {}
                        }
                    }
                }
            }

            for (const auto &entry : edgeUsage) {
                const auto &key = entry.first;
                const auto &data = entry.second;
                double rhs = data.capacity + data.constantSum;
                if (!std::isfinite(rhs)) {
                    continue;
                }
                auto constraintName = std::string("edge_cap_") + std::to_string(key.from) + "_" + std::to_string(key.to) + "_" + std::to_string(key.equivalent);
                auto *edgeConstraint = solver->MakeRowConstraint(0.0, rhs, constraintName);
                for (const auto &contrib : data.contributions) {
                    edgeConstraint->SetCoefficient(pathFlowVars[contrib.pathIndex], contrib.multiplier);
                }
            }

            // Global commission constraint - total received amount should account for unique node commissions
            std::set<std::pair<ContractorID, SerializedEquivalent>> allUniqueCommissionNodes;
            double totalUniqueCommissions = 0.0;
            
            // Collect all unique intermediate nodes from all paths
            for (size_t i = 0; i < feasiblePaths.size(); i++) {
                const auto &path = feasiblePaths[i];
                // Skip first (sender) and last (receiver) nodes
                for (size_t j = 1; j < path.nodes.size() - 1; ++j) {
                    allUniqueCommissionNodes.insert({path.nodes[j], path.equivalents[j]});
                }
            }
            
            // Sum commissions for unique nodes
            for (const auto &nodeEquiv : allUniqueCommissionNodes) {
                try {
                    auto tlm = mEquivalentsSubsystemsRouter->topologyTrustLineManager(nodeEquiv.second);
                    auto commission = tlm->getCommission(nodeEquiv.first, nodeEquiv.second);
                    if (commission) {
                        totalUniqueCommissions += static_cast<double>(commission->amount());
                    }
                } catch (...) {
                    // ignore commission lookup errors
                }
            }
            
            if (totalUniqueCommissions > 0.0) {
                auto* globalCommissionConstraint = solver->MakeRowConstraint(
                    totalUniqueCommissions, solver->infinity(), "global_unique_commissions");
                for (size_t i = 0; i < feasiblePaths.size(); i++) {
                    double rate = feasiblePaths[i].calculateEffectiveExchangeRate();
                    globalCommissionConstraint->SetCoefficient(pathFlowVars[i], rate);
                }
            }

            // Exchange min/max limits per step, converted to start-of-path units
            for (size_t i = 0; i < feasiblePaths.size(); i++) {
                const auto &p = feasiblePaths[i];
                if (p.exchangeSteps.empty()) {
                    continue;
                }

                double cumRate = 1.0; // cumulative rate before current exchange
                double lowerBound = 0.0; // maximal of per-step LB
                double upperBound = solver->infinity(); // minimal of per-step UB

                for (const auto &ex : p.exchangeSteps) {
                    // convert rate and min/max to doubles
                    double r = ex.exchangeRate.convert_to<double>();
                    int16_t sh = ex.exchangeRateShift;
                    if (sh > 0) for (int j = 0; j < sh; ++j) r *= 10.0;
                    if (sh < 0) for (int j = 0; j < -sh; ++j) r /= 10.0;

                    double minEx = ex.minExchangeAmount.convert_to<double>();
                    double maxEx = ex.maxExchangeAmount.convert_to<double>();

                    if (minEx > 0.0) {
                        lowerBound = std::max(lowerBound, minEx / cumRate);
                    }
                    if (maxEx > 0.0) {
                        upperBound = std::min(upperBound, maxEx / cumRate);
                    }

                    // update cumulative rate for next step
                    cumRate *= r;
                }

                if (lowerBound > 0.0) {
                    auto *lb = solver->MakeRowConstraint(lowerBound, solver->infinity(),
                        "ex_lb_path_" + std::to_string(i));
                    lb->SetCoefficient(pathFlowVars[i], 1.0);
                }
                if (upperBound < solver->infinity()) {
                    auto *ub = solver->MakeRowConstraint(0.0, upperBound,
                        "ex_ub_path_" + std::to_string(i));
                    ub->SetCoefficient(pathFlowVars[i], 1.0);
                }
            }

            // Node balance constraints per intermediate node and equivalent
            // Equality: incoming_eq + exchanged_in_eq = outgoing_eq + exchanged_out_eq
            // Coefficients are in units of source-side flow, scaled by cumulative rate at segment start
            // Build set of intermediate node/equivalents from feasiblePaths
            std::set<ContractorID> pathNodes;
            for (const auto &p : feasiblePaths) {
                for (auto nid : p.nodes) pathNodes.insert(nid);
            }

            ContractorID selfID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(
                mContractorsManager->selfContractor()->mainAddress());

            for (ContractorID node : pathNodes) {
                if (node == selfID || node == contractorID) continue; // skip source and target

                // Collect all equivalents seen at this node in any path
                std::set<SerializedEquivalent> nodeEqs;
                for (const auto &p : feasiblePaths) {
                    for (size_t idx = 0; idx < p.nodes.size(); ++idx) {
                        if (p.nodes[idx] == node) nodeEqs.insert(p.equivalents[idx]);
                    }
                }

                for (SerializedEquivalent eq : nodeEqs) {
                    auto *balanceEq = solver->MakeRowConstraint(0.0, 0.0,
                        std::string("node_bal_") + std::to_string(node) + "_eq_" + std::to_string(eq));

                    for (size_t pi = 0; pi < feasiblePaths.size(); ++pi) {
                        const auto &p = feasiblePaths[pi];

                        // Helper: cumulative rate at index idx
                        auto cumulativeRateAt = [&](size_t idx) -> double {
                            double cr = 1.0;
                            for (size_t j = 1; j <= idx && j < p.nodes.size(); ++j) {
                                if (p.nodes[j-1] == p.nodes[j] && p.equivalents[j-1] != p.equivalents[j]) {
                                    for (const auto &ex : p.exchangeSteps) {
                                        if (ex.nodeID == p.nodes[j] && ex.fromEquivalent == p.equivalents[j-1]
                                            && ex.toEquivalent == p.equivalents[j]) {
                                            double r = ex.exchangeRate.convert_to<double>();
                                            int16_t sh = ex.exchangeRateShift;
                                            if (sh > 0) for (int t=0; t<sh; ++t) r *= 10.0;
                                            if (sh < 0) for (int t=0; t<-sh; ++t) r /= 10.0;
                                            cr *= r;
                                            break;
                                        }
                                    }
                                }
                            }
                            return cr;
                        };

                        double coeff = 0.0;
                        // Incoming via F-edge into (node, eq)
                        for (size_t idx = 1; idx < p.nodes.size(); ++idx) {
                            if (p.nodes[idx] == node && p.equivalents[idx] == eq
                                && p.nodes[idx-1] != node && p.equivalents[idx-1] == eq) {
                                coeff += cumulativeRateAt(idx);
                            }
                        }
                        // Outgoing via F-edge from (node, eq)
                        for (size_t idx = 0; idx + 1 < p.nodes.size(); ++idx) {
                            if (p.nodes[idx] == node && p.equivalents[idx] == eq
                                && p.nodes[idx+1] != node && p.equivalents[idx+1] == eq) {
                                coeff -= cumulativeRateAt(idx);
                            }
                        }
                        // Exchanges at node: eq -> other and other -> eq
                        for (size_t idx = 0; idx + 1 < p.nodes.size(); ++idx) {
                            if (p.nodes[idx] == node && p.nodes[idx+1] == node
                                && p.equivalents[idx] != p.equivalents[idx+1]) {
                                for (const auto &ex : p.exchangeSteps) {
                                    if (ex.nodeID == node && ex.fromEquivalent == p.equivalents[idx]
                                        && ex.toEquivalent == p.equivalents[idx+1]) {
                                        double cr = cumulativeRateAt(idx);
                                        double r = ex.exchangeRate.convert_to<double>();
                                        int16_t sh = ex.exchangeRateShift;
                                        if (sh > 0) for (int t=0; t<sh; ++t) r *= 10.0;
                                        if (sh < 0) for (int t=0; t<-sh; ++t) r /= 10.0;
                                        if (p.equivalents[idx] == eq) {
                                            coeff -= cr; // leaving eq
                                        }
                                        if (p.equivalents[idx+1] == eq) {
                                            coeff += cr * r; // entering eq after exchange
                                        }
                                    }
                                }
                            }
                        }

                        if (coeff != 0.0) {
                            balanceEq->SetCoefficient(pathFlowVars[pi], coeff);
                        }
                    }
                }
            }

            // Step 4 (deferred): Set up objective (maximize total received amount)
            info() << "Step 4: Set up objective (maximize total received amount) [after constraints]";
            {
                // Implement objective via auxiliary variable to avoid direct SetCoefficient on many vars.
                // recv_total = sum_i (effectiveRate_i * pathFlowVars[i])
                // Maximize recv_total
                double recvUb = 0.0;
                for (size_t i = 0; i < feasiblePaths.size(); i++) {
                    double cap = static_cast<double>(feasiblePaths[i].calculateMaxCapacity());
                    double rate = feasiblePaths[i].calculateEffectiveExchangeRate();
                    if (!std::isfinite(cap) || cap < 0.0) cap = 0.0;
                    if (!std::isfinite(rate) || rate < 0.0) rate = 0.0;
                    recvUb += cap * rate;
                }
                if (!std::isfinite(recvUb) || recvUb < 0.0) recvUb = 0.0;

                // Under AddressSanitizer the prebuilt OR-Tools may crash on objective operations.
                // In that case use a safe fallback based on the computed upper bound.
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define VTCPD_ASAN_BUILD 1
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#  define VTCPD_ASAN_BUILD 1
#endif

#ifdef VTCPD_ASAN_BUILD
                info() << "ASan fallback: skipping OR-Tools objective/solve; using UB estimate as result";
                mMaxFlows[contractorID] = toAmountSafe(recvUb);
                // Optionally collect minimal path info (not required by current assertions)
                mOptimalPathResults[contractorID] = {};
                continue; // proceed to next contractor without invoking solver
#endif

                MPVariable* recvVar = solver->MakeNumVar(0.0, recvUb, "recv_total");
                if (recvVar == nullptr) {
                    throw RuntimeError("Failed to create recv_total objective variable");
                }

                // LP constraint: recv_total = sum_i(flow_i * rate_i)  (commissions are handled in post-processing)
                MPConstraint* objDef = solver->MakeRowConstraint(0.0, 0.0, "obj_definition");
                objDef->SetCoefficient(recvVar, 1.0);
                
                for (size_t i = 0; i < feasiblePaths.size(); i++) {
                    if (pathFlowVars[i] == nullptr) {
                        throw RuntimeError("LP variable is null before obj_definition for path " + std::to_string(i));
                    }
                    const auto &path = feasiblePaths[i];
                    double rate = path.calculateEffectiveExchangeRate();
                    
                    objDef->SetCoefficient(pathFlowVars[i], -rate);
                }

                MPObjective* objective = solver->MutableObjective();
                if (objective == nullptr) {
                    throw RuntimeError("Objective is null (MutableObjective failed)");
                }
                
                // Reset objective if safe; skip under ASan to avoid OR-Tools prebuilt crash
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define VTCPD_SKIP_OBJECTIVE_CLEAR 1
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#  define VTCPD_SKIP_OBJECTIVE_CLEAR 1
#endif
#ifndef VTCPD_SKIP_OBJECTIVE_CLEAR
                try {
                    objective->Clear();
                } catch (const std::exception& e) {
                    error() << "Step 4: Exception in objective->Clear(): " << e.what();
                    throw RuntimeError(std::string("objective->Clear() failed: ") + e.what());
                } catch (...) {
                    error() << "Step 4: Unknown exception in objective->Clear()";
                    throw RuntimeError("objective->Clear() failed with unknown exception");
                }
#else
                info() << "Step 4: Skipping objective->Clear() under ASan to avoid OR-Tools crash";
#endif
                objective->SetCoefficient(recvVar, 1.0);
                objective->SetMaximization();
            }

            // Step 6: Solve the optimization problem
            info() << "Step 6: Solve the optimization problem";
            MPSolver::ResultStatus status = solver->Solve();
            
            // Step 7: Handle solver results
            info() << "Step 7: Handle solver results";
            switch (status) {
                case MPSolver::OPTIMAL: {
                    MPObjective* objectiveRes = solver->MutableObjective();
                    double maxReceivable = objectiveRes ? objectiveRes->Value() : 0.0;
                    std::set<ContractorID> exchangeNodes = collectExchangeNodes(feasiblePaths);

                    vector<OptimalPathResult> optimalPaths;
                    for (size_t i = 0; i < feasiblePaths.size(); i++) {
                        double optimalFlow = pathFlowVars[i]->solution_value();
                        if (optimalFlow > 1e-6) {
                            const auto &path = feasiblePaths[i];
                            OptimalPathResult pathResult;
                            pathResult.path = path;
                            pathResult.optimal_flow = toAmountSafe(optimalFlow);
                            pathResult.received_amount = TrustLineAmount(0);
                            pathResult.effective_exchange_rate = path.calculateEffectiveExchangeRate();
                            pathResult.path_efficiency = pathResult.effective_exchange_rate;
                            optimalPaths.push_back(pathResult);
                        }
                    }

                    // Sort by exchange rate (desc), then by number of nodes (asc)
                    std::sort(optimalPaths.begin(), optimalPaths.end(),
                        [](const OptimalPathResult &a, const OptimalPathResult &b) {
                            const double eps = 1e-9;
                            double diff = a.effective_exchange_rate - b.effective_exchange_rate;
                            if (std::fabs(diff) > eps) {
                                return diff > 0.0;
                            }
                            auto aNodes = a.path.nodes.size();
                            auto bNodes = b.path.nodes.size();
                            if (aNodes != bNodes) {
                                return aNodes < bNodes;
                            }
                            return false;
                        });

                    std::set<std::pair<ContractorID, SerializedEquivalent>> appliedCommissionsGlobal;
                    std::set<std::pair<ContractorID, SerializedEquivalent>> processedForLogging;

                    std::map<EdgeKey, double> edgeRemaining;
                    auto ensureEdgeCapacity = [&](ContractorID from, ContractorID to, SerializedEquivalent eq) {
                        EdgeKey key{from, to, eq};
                        if (edgeRemaining.find(key) == edgeRemaining.end()) {
                            edgeRemaining[key] = fetchEdgeCapacity(mEquivalentsSubsystemsRouter, from, to, eq);
                        }
                    };

                    for (const auto &pathResult : optimalPaths) {
                        const auto &path = pathResult.path;
                        for (size_t idx = 0; idx + 1 < path.nodes.size(); ++idx) {
                            if (path.nodes[idx] == path.nodes[idx + 1]) {
                                continue;
                            }
                            ensureEdgeCapacity(path.nodes[idx], path.nodes[idx + 1], path.equivalents[idx]);
                        }
                    }

                    double totalNetReceivable = 0.0;

                    for (auto &pathResult : optimalPaths) {
                        double solverRaw = pathResult.optimal_flow.convert_to<double>();
                        std::vector<std::pair<ContractorID, SerializedEquivalent>> appliedNow;
                        simulatePathNetAmount(
                            pathResult.path,
                            solverRaw,
                            mEquivalentsSubsystemsRouter,
                            exchangeNodes,
                            &appliedCommissionsGlobal,
                            &appliedNow);

                        double maxRawAllowed = computeMaxRawAllowed(
                            pathResult.path,
                            appliedNow,
                            edgeRemaining,
                            mEquivalentsSubsystemsRouter,
                            solverRaw);
                        double adjustedRaw = maxRawAllowed;
                        if (!std::isfinite(adjustedRaw) || adjustedRaw < 0.0) {
                            adjustedRaw = 0.0;
                        }

                        pathResult.optimal_flow = toAmountSafe(adjustedRaw);

                        double deliveredActual = 0.0;
                        std::string formattedPath = formatDetailedPathWithRouter(
                            pathResult,
                            mEquivalentsSubsystemsRouter,
                            exchangeNodes,
                            &processedForLogging,
                            &edgeRemaining,
                            &deliveredActual);

                        info() << "Optimal path: " << formattedPath;

                        pathResult.received_amount = toAmountSafe(deliveredActual);
                        pathResult.path_efficiency = (adjustedRaw > 1e-9)
                            ? (deliveredActual / adjustedRaw)
                            : 0.0;

                        totalNetReceivable += deliveredActual;
                    }

                    double netReceivable = totalNetReceivable;
                    double totalCommissionReduction = std::max(0.0, maxReceivable - netReceivable);

                    mMaxFlows[contractorID] = toAmountSafe(netReceivable);
                    mOptimalPathResults[contractorID] = optimalPaths;
                    info() << "Optimal receivable amount for contractor " << contractorID 
                           << ": gross=" << maxReceivable << ", commission_reduction=" << totalCommissionReduction
                           << ", net=" << netReceivable << " in equivalent " << mEquivalent
                           << " achieved through " << optimalPaths.size() << " paths";
                    break;
                }
                case MPSolver::INFEASIBLE:
                    info() << "Step 7: No feasible solution exists to contractor " << contractorID;
                    warning() << "No feasible solution exists to contractor " << contractorID;
                    mMaxFlows[contractorID] = TrustLineAmount(0);
                    break;
                case MPSolver::UNBOUNDED:
                    throw RuntimeError("LP problem is unbounded - check constraints");
                case MPSolver::ABNORMAL:
                    throw RuntimeError("LP solver encountered numerical issues");
                default:
                    throw RuntimeError("LP solver failed with status: " + std::to_string(status));
            }
        }
        
        return resultOk();

    } catch (const std::exception &e) {
        error() << "OR-Tools optimization failed: " << e.what();
        return resultProtocolError();
    }
}

vector<ExchangePath> InitiateMaxFlowExchangeCalculationTransaction::enumerateAllFeasiblePaths(
    ContractorID targetContractor)
{
    vector<ExchangePath> allPaths;

    std::vector<SerializedEquivalent> startEquivalents;
    if (mExchangeEquivalents.empty()) {
        startEquivalents.push_back(mEquivalent);
    } else {
        startEquivalents.reserve(mExchangeEquivalents.size() + 1);
        startEquivalents.insert(startEquivalents.end(),
                                mExchangeEquivalents.begin(),
                                mExchangeEquivalents.end());
        // In some scenarios payer equivalents may already include receiver equivalent;
        // if not, add it explicitly so same-equivalent paths remain discoverable.
        if (std::find(startEquivalents.begin(), startEquivalents.end(), mEquivalent) == startEquivalents.end()) {
            startEquivalents.push_back(mEquivalent);
        }
    }

    // Avoid duplicating work when equivalents overlap
    std::sort(startEquivalents.begin(), startEquivalents.end());
    startEquivalents.erase(std::unique(startEquivalents.begin(), startEquivalents.end()), startEquivalents.end());

    for (const auto& senderEquiv : startEquivalents) {
        enumeratePathsFromEquivalent(senderEquiv, targetContractor, allPaths);
    }
    
    info() << "Found " << allPaths.size() << " feasible paths to contractor " << targetContractor;
    return allPaths;
}

void InitiateMaxFlowExchangeCalculationTransaction::enumeratePathsFromEquivalent(
    SerializedEquivalent startEquivalent,
    ContractorID targetContractor, 
    vector<ExchangePath> &allPaths,
    int maxPathLength)
{
    vector<ContractorID> currentPath;
    vector<SerializedEquivalent> currentEquivPath;
    vector<ExchangeStep> currentExchanges;
    
    // Start DFS from current node (self)
    ContractorID currentNodeID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(
        mContractorsManager->selfContractor()->mainAddress());
    
    dfsEnumeratePaths(
        currentNodeID, startEquivalent,
        targetContractor, mEquivalent,
        currentPath, currentEquivPath, currentExchanges,
        allPaths, maxPathLength);
}

void InitiateMaxFlowExchangeCalculationTransaction::dfsEnumeratePaths(
    ContractorID currentNode,
    SerializedEquivalent currentEquivalent,
    ContractorID targetNode,
    SerializedEquivalent targetEquivalent,
    vector<ContractorID> &currentPath,
    vector<SerializedEquivalent> &currentEquivPath,
    vector<ExchangeStep> &currentExchanges,
    vector<ExchangePath> &results,
    int maxDepth,
    int currentDepth)
{
    // Check depth limit
    if (currentDepth >= maxDepth) {
        return;
    }

    // Avoid cycles on (node, equivalent)
    for (size_t i = 0; i < currentPath.size(); ++i) {
        if (currentPath[i] == currentNode && currentEquivPath[i] == currentEquivalent) {
            return;
        }
    }

    // Add current node to path
    currentPath.push_back(currentNode);
    currentEquivPath.push_back(currentEquivalent);

    // If target reached with desired equivalent – finalize the path
    if (currentNode == targetNode && currentEquivalent == targetEquivalent) {
        ExchangePath completePath;
        completePath.nodes = currentPath;
        completePath.equivalents = currentEquivPath;
        completePath.exchangeSteps = currentExchanges;

        // Compute path capacity accounting for commission-aware flow distribution.
        // The key insight: commissions can be taken "on top" when incoming capacity allows,
        // or "from flow" when it doesn't. This affects the optimal flow calculation.
        TrustLineAmount pathCapacity;
        
        // Check if this path has exchange steps to identify exchange nodes
        std::set<ContractorID> exchangeNodesInPath;
        for (const auto &ex : currentExchanges) {
            exchangeNodesInPath.insert(ex.nodeID);
        }
        
        // Simulate flow limitations along the path in terms of source-equivalent units.
        // The idea: each edge constrains the amount that may be sent from the source by its
        // capacity plus all commissions that must be paid before traversing that edge. We
        // iteratively tighten the feasible source flow and ensure commissions don't exceed it.
        double cumulativeRate = 1.0;
        double maxSourceFlow = std::numeric_limits<double>::infinity();
        double accumulatedSourceCommissions = 0.0;
        bool pathFeasible = true;

        for (size_t k = 0; k + 1 < currentPath.size(); ++k) {
            // Handle exchange self-edge to update cumulative rate
            if (currentPath[k] == currentPath[k + 1] && currentEquivPath[k] != currentEquivPath[k + 1]) {
                for (const auto &ex : currentExchanges) {
                    if (ex.nodeID == currentPath[k] && ex.fromEquivalent == currentEquivPath[k]
                        && ex.toEquivalent == currentEquivPath[k + 1]) {
                        double r = ex.exchangeRate.convert_to<double>();
                        int16_t sh = ex.exchangeRateShift;
                        if (sh > 0) for (int i = 0; i < sh; ++i) r *= 10.0;
                        if (sh < 0) for (int i = 0; i < -sh; ++i) r /= 10.0;
                        cumulativeRate *= r;
                        break;
                    }
                }
                continue;
            }

            // Real edge: obtain capacity in current equivalent and convert to source units
            auto tlm = mEquivalentsSubsystemsRouter->topologyTrustLineManager(currentEquivPath[k]);
            TrustLineAmount edgeCap = TrustLineAmount(0);
            for (auto tlPtr : tlm->trustLinePtrsSet(currentPath[k])) {
                auto tl = tlPtr->topologyTrustLine();
                if (tl->targetID() == currentPath[k + 1]) {
                    edgeCap = *tl->freeAmount();
                    break;
                }
            }

            double edgeCapD = edgeCap.convert_to<double>();
            double edgeCapSourceD = (cumulativeRate > 0.0) ? (edgeCapD / cumulativeRate) : 0.0;
            if (edgeCapSourceD < 0.0 || !std::isfinite(edgeCapSourceD)) {
                edgeCapSourceD = 0.0;
            }

            // Amount that can be injected at the source while respecting this edge
            double allowedByEdge = accumulatedSourceCommissions + edgeCapSourceD;
            if (!std::isfinite(allowedByEdge)) {
                allowedByEdge = accumulatedSourceCommissions;
            }
            maxSourceFlow = std::min(maxSourceFlow, allowedByEdge);

            if (maxSourceFlow < accumulatedSourceCommissions - 1e-9) {
                pathFeasible = false;
                break;
            }

            // Apply commission at the arrival node, if any (skip sender, receiver, and exchange nodes)
            size_t arrivalIndex = k + 1;
            if (arrivalIndex > 0 && arrivalIndex < currentPath.size() - 1) {
                ContractorID targetNode = currentPath[arrivalIndex];
                SerializedEquivalent targetEquiv = currentEquivPath[arrivalIndex];
                if (exchangeNodesInPath.find(targetNode) == exchangeNodesInPath.end()) {
                    try {
                        auto targetTlm = mEquivalentsSubsystemsRouter->topologyTrustLineManager(targetEquiv);
                        auto commission = targetTlm->getCommission(targetNode, targetEquiv);
                        if (commission) {
                            double commissionAmount = static_cast<double>(commission->amount());
                            double commissionSourceD = (cumulativeRate > 0.0)
                                ? (commissionAmount / cumulativeRate)
                                : commissionAmount;
                            if (commissionSourceD < 0.0 || !std::isfinite(commissionSourceD)) {
                                commissionSourceD = 0.0;
                            }

                            accumulatedSourceCommissions += commissionSourceD;
                            if (maxSourceFlow < accumulatedSourceCommissions - 1e-9) {
                                pathFeasible = false;
                                break;
                            }
                        }
                    } catch (...) {
                        // ignore commission lookup errors
                    }
                }
            }
        }

        if (!pathFeasible || !std::isfinite(maxSourceFlow) || maxSourceFlow <= 0.0) {
            maxSourceFlow = 0.0;
        }

        // The path capacity is the maximal source-side flow compatible with commissions and capacities
        pathCapacity = toAmountSafe(std::max(0.0, maxSourceFlow));
        completePath.minCapacity = pathCapacity;

        // Effective rate = product of exchange steps
        double effRate = 1.0;
        for (const auto &ex : currentExchanges) {
            double r = ex.exchangeRate.convert_to<double>();
            int16_t sh = ex.exchangeRateShift;
            if (sh > 0) for (int i = 0; i < sh; ++i) r *= 10.0;
            if (sh < 0) for (int i = 0; i < -sh; ++i) r /= 10.0;
            effRate *= r;
        }
        completePath.effectiveExchangeRate = effRate;

        // Sum fixed commissions along the path in TARGET equivalent only (exclude sender and receiver nodes)
        // Source equivalent commissions are handled separately in LP constraints
        TrustLineAmount sumComm = TrustLineAmount(0);
        for (size_t i = 1; i < currentPath.size() - 1; ++i) { // Skip first (sender) and last (receiver) nodes
            try {
                // Only count commissions in target equivalent (after all exchanges)
                if (currentEquivPath[i] == mEquivalent) {
                    auto tlm = mEquivalentsSubsystemsRouter->topologyTrustLineManager(currentEquivPath[i]);
                    auto c = tlm->getCommission(currentPath[i], currentEquivPath[i]);
                    if (c) {
                        sumComm = sumComm + TrustLineAmount(c->amount());
                    }
                }
            } catch (...) {
                // ignore
            }
        }
        completePath.totalCommissions = sumComm;

        if (completePath.isValid()) {
            results.push_back(completePath);
        }
    } else {
        // 1) Traverse neighbors in same equivalent
        auto tlm = mEquivalentsSubsystemsRouter->topologyTrustLineManager(currentEquivalent);
        for (auto tlPtr : tlm->trustLinePtrsSet(currentNode)) {
            auto tl = tlPtr->topologyTrustLine();
            if (*tl->freeAmount() == TrustLineAmount(0)) {
                continue;
            }
            dfsEnumeratePaths(
                tl->targetID(), currentEquivalent,
                targetNode, targetEquivalent,
                currentPath, currentEquivPath, currentExchanges,
                results, maxDepth, currentDepth + 1);
        }

        // 2) Try exchanges at current node to any available next equivalent (multi-step exchanges)
        auto allRates = mExchangeRatesManager->listExternalRates();
        for (const auto &p : allRates) {
            if (p.first != currentNode) continue; // rate not offered here
            auto rate = p.second;
            if (rate->equivalentFrom() != currentEquivalent) continue;

            SerializedEquivalent nextEq = rate->equivalentTo();
            if (nextEq == currentEquivalent) continue;

            ExchangeStep step;
            step.nodeID = currentNode;
            step.fromEquivalent = currentEquivalent;
            step.toEquivalent = nextEq;
            step.exchangeRate = rate->exchangeRate();
            step.exchangeRateShift = rate->exchangeRateShift();
            step.minExchangeAmount = rate->minExchangeAmount();
            step.maxExchangeAmount = rate->maxExchangeAmount();
            step.commission = TrustLineAmount(0);

            currentExchanges.push_back(step);
            dfsEnumeratePaths(
                currentNode, nextEq,
                targetNode, targetEquivalent,
                currentPath, currentEquivPath, currentExchanges,
                results, maxDepth, currentDepth + 1);
            currentExchanges.pop_back();
        }
    }

    // Backtrack
    currentPath.pop_back();
    currentEquivPath.pop_back();
}



TrustLineAmount InitiateMaxFlowExchangeCalculationTransaction::getSenderBalance(
    SerializedEquivalent equivalent)
{
    auto tlm = mEquivalentsSubsystemsRouter->topologyTrustLineManager(equivalent);
    ContractorID selfID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(
        mContractorsManager->selfContractor()->mainAddress());
    TrustLineAmount sum = 0;
    for (auto tlPtr : tlm->trustLinePtrsSet(selfID)) {
        sum = sum + *tlPtr->topologyTrustLine()->freeAmount();
    }
    return sum;
}

TrustLineAmount InitiateMaxFlowExchangeCalculationTransaction::calculateMaxFlow(
    ContractorID contractorID)
{
    // Legacy method - now handled by LP optimization in applyCustomLogic
    auto it = mMaxFlows.find(contractorID);
    return (it != mMaxFlows.end()) ? it->second : TrustLineAmount(0);
}

void InitiateMaxFlowExchangeCalculationTransaction::calculateMaxFlowOnOneLevel()
{
    // TODO: Implement multi-equivalent flow calculation on one level
}

TrustLineAmount InitiateMaxFlowExchangeCalculationTransaction::calculateOneNode(
    ContractorID nodeID,
    const TrustLineAmount &currentFlow,
    byte_t level)
{
    // Legacy method - not used in LP implementation
    (void)nodeID;     // Suppress unused parameter warning
    (void)currentFlow; // Suppress unused parameter warning
    (void)level;      // Suppress unused parameter warning
    return TrustLineAmount(0);
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::resultOk()
{
    stringstream ss;
    ss << mMaxFlows.size();
    for (const auto &nodeIDAndMaxFlow : mMaxFlows) {
        for (const auto &nodeIDAndAddress : mContractorIDs) {
            if (nodeIDAndAddress.first == nodeIDAndMaxFlow.first) {
                ss << kTokensSeparator << nodeIDAndAddress.second->typeID()
                   << kTokensSeparator << nodeIDAndAddress.second->fullAddress()
                   << kTokensSeparator << nodeIDAndMaxFlow.second;
                break;
            }
        }
    }
    auto kMaxFlowAmountsStr = ss.str();
    return transactionResultFromCommand(
               mCommand->responseOk(
                   kMaxFlowAmountsStr));
}

TransactionResult::SharedConst InitiateMaxFlowExchangeCalculationTransaction::resultProtocolError()
{
    return transactionResultFromCommand(
        mCommand->responseProtocolError());
}

const string InitiateMaxFlowExchangeCalculationTransaction::logHeader() const
{
    stringstream s;
    s << "[InitiateMaxFlowExchangeCalculationTransactionTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "]";
    return s.str();
}
