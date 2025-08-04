#ifndef VTCPD_TRUSTLINEHANDLERSQLITE_H
#define VTCPD_TRUSTLINEHANDLERSQLITE_H

#include "../../../trust_lines/TrustLine.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../interfaces/TrustLineHandler.h"
#include "SQLiteStatementRAII.h"

#include <sqlite3.h>
#include <vector>

using namespace std;

class TrustLineHandlerSQLite : public TrustLineHandler
{

public:
    TrustLineHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    void saveTrustLine(
        TrustLine::Shared trustLine,
        const SerializedEquivalent equivalent);

    void updateTrustLineState(
        TrustLine::Shared trustLine,
        const SerializedEquivalent equivalent);

    void updateTrustLineIsContractorGateway(
        TrustLine::Shared trustLine,
        const SerializedEquivalent equivalent);

    vector<TrustLine::Shared> allTrustLinesByEquivalent(
        const SerializedEquivalent equivalent);

    void deleteTrustLine(
        ContractorID contractorID,
        const SerializedEquivalent equivalent);

    vector<SerializedEquivalent> equivalents();

    vector<TrustLineID> allIDs();

    vector<TrustLine::Shared> allTrustLinesByContractor(
        ContractorID contractorID);

private:
    LoggerStream info() const;

    LoggerStream warning() const;

    const string logHeader() const;

private:
    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};


#endif //VTCPD_TRUSTLINEHANDLERSQLITE_H
