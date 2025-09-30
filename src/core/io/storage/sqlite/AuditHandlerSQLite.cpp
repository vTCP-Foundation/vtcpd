#include "AuditHandlerSQLite.h"

AuditHandlerSQLite::AuditHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   "(number INTEGER NOT NULL, "
                   "trust_line_id INTEGER NOT NULL, "
                   "our_signature BLOB NOT NULL, "
                   "contractor_signature BLOB DEFAULT NULL, "
                   "balance BLOB NOT NULL, "
                   "outgoing_amount BLOB NOT NULL, "
                   "incoming_amount BLOB NOT NULL, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE);";
    
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("AuditHandlerSQLite::creating table: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_number_trust_line_id_idx on " + mTableName + "(number, trust_line_id);";
    SQLiteStatementRAII stmtUnique(mDataBase, query.c_str());
    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("AuditHandlerSQLite::creating index for Number and TrustLineID: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "AuditHandler initialized: table=" << mTableName;
#endif
}

void AuditHandlerSQLite::saveFullAudit(
    AuditNumber number,
    TrustLineID trustLineID,
    Signature::Shared ownSignature,
    Signature::Shared contractorSignature,
    const TrustLineAmount &incomingAmount,
    const TrustLineAmount &outgoingAmount,
    const TrustLineBalance &balance)
{
    string query = "INSERT INTO " + mTableName +
                   "(number, trust_line_id, our_signature, contractor_signature, "
                   "incoming_amount, outgoing_amount, balance) VALUES (?, ?, ?, ?, ?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, number);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of Audit Number; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 2, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of ID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 3, ownSignature->data(), (int)ownSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of OwnSignature; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 4, contractorSignature->data(), (int)contractorSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of ContractorSignature; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> incomingAmountBufferBytes = trustLineAmountToBytes(incomingAmount);
    rc = sqlite3_bind_blob(stmt.get(), 5, incomingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of Incoming Amount; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> outgoingAmountBufferBytes = trustLineAmountToBytes(outgoingAmount);
    rc = sqlite3_bind_blob(stmt.get(), 6, outgoingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of Outgoing Amount; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> balanceBufferBytes = trustLineBalanceToBytes(const_cast<TrustLineBalance&>(balance));
    rc = sqlite3_bind_blob(stmt.get(), 7, balanceBufferBytes.data(), kTrustLineBalanceSerializeBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of Balance; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "prepare inserting is completed successfully";
#endif
    } else {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

void AuditHandlerSQLite::saveOwnAuditPart(
    AuditNumber number,
    TrustLineID trustLineID,
    Signature::Shared ownSignature,
    const TrustLineAmount &incomingAmount,
    const TrustLineAmount &outgoingAmount,
    const TrustLineBalance &balance)
{
    string query = "INSERT INTO " + mTableName +
                   "(number, trust_line_id, our_signature, incoming_amount, outgoing_amount, balance) "
                   "VALUES (?, ?, ?, ?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, number);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of Audit Number; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 2, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of ID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 3, ownSignature->data(), (int)ownSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of OwnSignature; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> incomingAmountBufferBytes = trustLineAmountToBytes(incomingAmount);
    rc = sqlite3_bind_blob(stmt.get(), 4, incomingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of Incoming Amount; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> outgoingAmountBufferBytes = trustLineAmountToBytes(outgoingAmount);
    rc = sqlite3_bind_blob(stmt.get(), 5, outgoingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of Outgoing Amount; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> balanceBufferBytes = trustLineBalanceToBytes(const_cast<TrustLineBalance&>(balance));
    rc = sqlite3_bind_blob(stmt.get(), 6, balanceBufferBytes.data(), kTrustLineBalanceSerializeBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of Balance; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

void AuditHandlerSQLite::saveContractorAuditPart(
    AuditNumber number,
    TrustLineID trustLineID,
    Signature::Shared contractorSignature)
{
    string query = "UPDATE " + mTableName +
                   " SET contractor_signature = ? "
                   "WHERE trust_line_id = ? AND number = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_blob(stmt.get(), 1, contractorSignature->data(), (int)contractorSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveContractorAuditPart: "
                      "Bad binding of ContractorSignature; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 2, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveContractorAuditPart: "
                      "Bad binding of ID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 3, number);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveContractorAuditPart: "
                      "Bad binding of Audit Number; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("AuditHandlerSQLite::saveContractorAuditPart: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("No data were modified");
    }
}

const AuditRecord::Shared AuditHandlerSQLite::getActualAudit(
    TrustLineID trustLineID)
{
    string query = "SELECT number, incoming_amount, outgoing_amount, balance, contractor_signature FROM " +
                   mTableName + " WHERE trust_line_id = ? ORDER BY number DESC LIMIT 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::getActualAudit: "
                      "Bad binding of Trust Line ID; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto number = (AuditNumber)sqlite3_column_int(stmt.get(), 0);
        auto incomingAmountBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 1);
        vector<byte_t> incomingAmountBufferBytes(
            incomingAmountBytes,
            incomingAmountBytes + kTrustLineAmountBytesCount);
        TrustLineAmount incomingAmount = bytesToTrustLineAmount(incomingAmountBufferBytes);

        auto outgoingAmountBytes = (byte_t*)sqlite3_column_blob(stmt, 2);
        vector<byte_t> outgoingAmountBufferBytes(
            outgoingAmountBytes,
            outgoingAmountBytes + kTrustLineAmountBytesCount);
        TrustLineAmount outgoingAmount = bytesToTrustLineAmount(outgoingAmountBufferBytes);

        auto balanceBytes = (byte_t*)sqlite3_column_blob(stmt, 3);
        vector<byte_t> balanceBufferBytes(
            balanceBytes,
            balanceBytes + kTrustLineBalanceSerializeBytesCount);
        TrustLineBalance balance = bytesToTrustLineBalance(balanceBufferBytes);

        auto contractorSignatureBytes = (byte_t*)sqlite3_column_blob(stmt, 4);
        Signature::Shared contractorSignature = nullptr;
        if (contractorSignatureBytes != nullptr) {
            contractorSignature = make_shared<Signature>(
                                      contractorSignatureBytes);
        }

        auto result = make_shared<AuditRecord>(
                          number,
                          incomingAmount,
                          outgoingAmount,
                          balance);
        result->setContractorSignature(
            contractorSignature);
        return result;
    } else {
        throw NotFoundError("AuditHandlerSQLite::getActualAudit: "
                            "There are no records with requested trust line id");
    }
}

const AuditRecord::Shared AuditHandlerSQLite::getActualAuditFull(
    TrustLineID trustLineID)
{
    string query = "SELECT number, incoming_amount, outgoing_amount, balance, "
                   "our_signature, contractor_signature FROM " +
                   mTableName + " WHERE trust_line_id = ? ORDER BY number DESC LIMIT 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::getActualAuditFull: "
                      "Bad binding of Trust Line ID; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        auto number = (AuditNumber)sqlite3_column_int(stmt, 0);
        auto incomingAmountBytes = (byte_t*)sqlite3_column_blob(stmt, 1);
        vector<byte_t> incomingAmountBufferBytes(
            incomingAmountBytes,
            incomingAmountBytes + kTrustLineAmountBytesCount);
        TrustLineAmount incomingAmount = bytesToTrustLineAmount(incomingAmountBufferBytes);

        auto outgoingAmountBytes = (byte_t*)sqlite3_column_blob(stmt, 2);
        vector<byte_t> outgoingAmountBufferBytes(
            outgoingAmountBytes,
            outgoingAmountBytes + kTrustLineAmountBytesCount);
        TrustLineAmount outgoingAmount = bytesToTrustLineAmount(outgoingAmountBufferBytes);

        auto balanceBytes = (byte_t*)sqlite3_column_blob(stmt, 3);
        vector<byte_t> balanceBufferBytes(
            balanceBytes,
            balanceBytes + kTrustLineBalanceSerializeBytesCount);
        TrustLineBalance balance = bytesToTrustLineBalance(balanceBufferBytes);

        auto ownSignature = make_shared<Signature>(
                                (byte_t*)sqlite3_column_blob(stmt, 4));

        auto contractorSignatureBytes = (byte_t*)sqlite3_column_blob(stmt, 5);
        Signature::Shared contractorSignature = nullptr;
        if (contractorSignatureBytes != nullptr) {
            contractorSignature = make_shared<Signature>(
                                      contractorSignatureBytes);
        }

        return make_shared<AuditRecord>(
                   number,
                   incomingAmount,
                   outgoingAmount,
                   balance,
                   ownSignature,
                   contractorSignature);
    } else {
        throw NotFoundError("AuditHandlerSQLite::getActualAuditFull: "
                            "There are no records with requested trust line id " + to_string(trustLineID));
    }
}

const AuditNumber AuditHandlerSQLite::getActualAuditNumber(
    TrustLineID trustLineID)
{
    string query = "SELECT number FROM " + mTableName + " WHERE trust_line_id = ? ORDER BY number DESC LIMIT 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::getActualAuditNumber: "
                      "Bad binding of Trust Line ID; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto number = (AuditNumber)sqlite3_column_int(stmt.get(), 0);
        return number;
    } else {
        throw NotFoundError("AuditHandlerSQLite::getActualAuditNumber: "
                            "There are no records with requested trust line id");
    }
}

void AuditHandlerSQLite::deleteRecords(
    TrustLineID trustLineID)
{
    string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::deleteRecords: "
                      "Bad binding of TrustLineID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "deleting is completed successfully";
#endif
    } else {
        throw IOError("AuditHandlerSQLite::deleteRecords: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

void AuditHandlerSQLite::deleteAuditByNumber(
    TrustLineID trustLineID,
    AuditNumber auditNumber)
{
    string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = ? AND number = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::deleteAuditByNumber: "
                      "Bad binding of TrustLineID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::deleteAuditByNumber: "
                      "Bad binding of auditNumber; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "deleting is completed successfully";
#endif
    } else {
        throw IOError("AuditHandlerSQLite::deleteAuditByNumber: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("No data were deleted");
    }
}

vector<AuditRecord::Shared> AuditHandlerSQLite::auditsLessEqualThanAuditNumber(
    TrustLineID trustLineID,
    AuditNumber auditNumber)
{
    string query = "SELECT number, incoming_amount, outgoing_amount, balance, "
                   "our_signature, contractor_signature FROM " +
                   mTableName + " WHERE trust_line_id = ? AND number <= ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::auditsLessEqualThanAuditNumber: "
                      "Bad binding of Trust Line ID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::auditsLessEqualThanAuditNumber: "
                      "Bad binding of AuditNumber; sqlite error: " +
                      to_string(rc));
    }

    vector<AuditRecord::Shared> result;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto number = (AuditNumber)sqlite3_column_int(stmt.get(), 0);
        auto incomingAmountBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 1);
        vector<byte_t> incomingAmountBufferBytes(
            incomingAmountBytes,
            incomingAmountBytes + kTrustLineAmountBytesCount);
        TrustLineAmount incomingAmount = bytesToTrustLineAmount(incomingAmountBufferBytes);

        auto outgoingAmountBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 2);
        vector<byte_t> outgoingAmountBufferBytes(
            outgoingAmountBytes,
            outgoingAmountBytes + kTrustLineAmountBytesCount);
        TrustLineAmount outgoingAmount = bytesToTrustLineAmount(outgoingAmountBufferBytes);

        auto balanceBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 3);
        vector<byte_t> balanceBufferBytes(
            balanceBytes,
            balanceBytes + kTrustLineBalanceSerializeBytesCount);
        TrustLineBalance balance = bytesToTrustLineBalance(balanceBufferBytes);

        auto ownSignature = make_shared<Signature>(
                                (byte_t*)sqlite3_column_blob(stmt.get(), 4));

        auto contractorSignatureBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 5);
        Signature::Shared contractorSignature = nullptr;
        if (contractorSignatureBytes != nullptr) {
            contractorSignature = make_shared<Signature>(
                                      contractorSignatureBytes);
        }

        result.push_back(
            make_shared<AuditRecord>(
                number,
                incomingAmount,
                outgoingAmount,
                balance,
                ownSignature,
                contractorSignature));
    }
    return result;
}


LoggerStream AuditHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream AuditHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string AuditHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "[AuditHandler]";
    return s.str();
}
