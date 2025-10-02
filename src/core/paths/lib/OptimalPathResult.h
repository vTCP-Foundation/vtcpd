#ifndef VTCPD_OPTIMALPATHRESULT_H
#define VTCPD_OPTIMALPATHRESULT_H

#include "ExchangePath.h"

// Stores result of OR-Tools optimization for a single path
struct OptimalPathResult {
    ExchangePath path;
    TrustLineAmount optimal_flow;
    TrustLineAmount received_amount;
    double effective_exchange_rate;
    double path_efficiency;
};

#endif // VTCPD_OPTIMALPATHRESULT_H
