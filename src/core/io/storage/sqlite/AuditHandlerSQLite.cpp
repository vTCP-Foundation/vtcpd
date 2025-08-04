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
                   "our_key_hash BLOB NOT NULL, "
                   "our_signature BLOB NOT NULL, "
                   "contractor_key_hash BLOB DEFAULT NULL, "
                   "contractor_signature BLOB DEFAULT NULL, "
                   "own_keys_set_hash BLOB NOT NULL, "
                   "contractor_keys_set_hash BLOB NOT NULL, "
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
    lamport::KeyHash::Shared ownKeyHash,
    lamport::Signature::Shared ownSignature,
    lamport::KeyHash::Shared contractorKeyHash,
    lamport::Signature::Shared contractorSignature,
    lamport::KeyHash::Shared ownKeysSetHash,
    lamport::KeyHash::Shared contractorKeysSetHash,
    const TrustLineAmount &incomingAmount,
    const TrustLineAmount &outgoingAmount,
    const TrustLineBalance &balance)
{
    string query = "INSERT INTO " + mTableName +
                   "(number, trust_line_id, our_key_hash, our_signature, contractor_key_hash, "
                   "contractor_signature, own_keys_set_hash, contractor_keys_set_hash, "
                   "incoming_amount, outgoing_amount, balance) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
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
    rc = sqlite3_bind_blob(stmt.get(), 3, ownKeyHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of OwnKeyHash; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 4, ownSignature->data(), (int)ownSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of OnwSignature; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 5, contractorKeyHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of ContractorKeyHash; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 6, contractorSignature->data(), (int)contractorSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of ContractorSignature; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 7, ownKeysSetHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of OwnKeysSetHash; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 8, contractorKeysSetHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of ContractorKeysSetHash; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> incomingAmountBufferBytes = trustLineAmountToBytes(incomingAmount);
    rc = sqlite3_bind_blob(stmt.get(), 9, incomingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of Incoming Amount; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> outgoingAmountBufferBytes = trustLineAmountToBytes(outgoingAmount);
    rc = sqlite3_bind_blob(stmt.get(), 10, outgoingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveFullAudit: "
                      "Bad binding of Outgoing Amount; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> balanceBufferBytes = trustLineBalanceToBytes(const_cast<TrustLineBalance&>(balance));
    rc = sqlite3_bind_blob(stmt.get(), 11, balanceBufferBytes.data(), kTrustLineBalanceSerializeBytesCount, SQLITE_STATIC);
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
    lamport::KeyHash::Shared ownKeyHash,
    lamport::Signature::Shared ownSignature,
    lamport::KeyHash::Shared ownKeysSetHash,
    lamport::KeyHash::Shared contractorKeysSetHash,
    const TrustLineAmount &incomingAmount,
    const TrustLineAmount &outgoingAmount,
    const TrustLineBalance &balance)
{
    string query = "INSERT INTO " + mTableName +
                   "(number, trust_line_id, our_key_hash, our_signature, "
                   "own_keys_set_hash, contractor_keys_set_hash, incoming_amount, outgoing_amount, balance) "
                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
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
    rc = sqlite3_bind_blob(stmt.get(), 3, ownKeyHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of OwnKeyHash; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 4, ownSignature->data(), (int)ownSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of OnwSignature; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 5, ownKeysSetHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of OwnKeysSetHash; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 6, contractorKeysSetHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of ContractorKeysSetHash; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> incomingAmountBufferBytes = trustLineAmountToBytes(incomingAmount);
    rc = sqlite3_bind_blob(stmt.get(), 7, incomingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of Incoming Amount; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> outgoingAmountBufferBytes = trustLineAmountToBytes(outgoingAmount);
    rc = sqlite3_bind_blob(stmt.get(), 8, outgoingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveOwnAuditPart: "
                      "Bad binding of Outgoing Amount; sqlite error: " +
                      to_string(rc));
    }
    vector<byte_t> balanceBufferBytes = trustLineBalanceToBytes(const_cast<TrustLineBalance&>(balance));
    rc = sqlite3_bind_blob(stmt.get(), 9, balanceBufferBytes.data(), kTrustLineBalanceSerializeBytesCount, SQLITE_STATIC);
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
    lamport::KeyHash::Shared contractorKeyHash,
    lamport::Signature::Shared contractorSignature)
{
    string query = "UPDATE " + mTableName +
                   " SET contractor_key_hash = ?, contractor_signature = ? "
                   "WHERE trust_line_id = ? AND number = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_blob(stmt.get(), 1, contractorKeyHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveContractorAuditPart: "
                      "Bad binding of ContractorKeyHash; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 2, contractorSignature->data(), (int)contractorSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveContractorAuditPart: "
                      "Bad binding of ContractorSignature; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 3, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::saveContractorAuditPart: "
                      "Bad binding of ID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 4, number);
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
    string query = "SELECT number, incoming_amount, outgoing_amount, balance, contractor_signature, "
                   "own_keys_set_hash, contractor_keys_set_hash FROM " +
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
        lamport::Signature::Shared contractorSignature = nullptr;
        if (contractorSignatureBytes != nullptr) {
            contractorSignature = make_shared<lamport::Signature>(
                                      contractorSignatureBytes);
        }

        auto ownKeysSetHash = make_shared<lamport::KeyHash>(
                                  (byte_t*)sqlite3_column_blob(stmt, 5));

        auto contractorKeysSetHash = make_shared<lamport::KeyHash>(
                                         (byte_t*)sqlite3_column_blob(stmt, 6));

        auto result = make_shared<AuditRecord>(
                          number,
                          incomingAmount,
                          outgoingAmount,
                          balance);
        result->setContractorSignature(
            contractorSignature);
        result->setOwnKeysSetHash(
            ownKeysSetHash);
        result->setContractorKeysSetHash(
            contractorKeysSetHash);
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
                   "our_key_hash, our_signature, contractor_key_hash, contractor_signature, "
                   "own_keys_set_hash, contractor_keys_set_hash FROM " +
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

        auto ownKeyHash = make_shared<lamport::KeyHash>(
                              (byte_t*)sqlite3_column_blob(stmt, 4));

        auto ownSignature = make_shared<lamport::Signature>(
                                (byte_t*)sqlite3_column_blob(stmt, 5));

        auto contractorKeyHashBytes = (byte_t*)sqlite3_column_blob(stmt, 6);
        lamport::KeyHash::Shared contractorKeyHash = nullptr;
        if (contractorKeyHashBytes != nullptr) {
            contractorKeyHash = make_shared<lamport::KeyHash>(
                                    contractorKeyHashBytes);
        }

        auto contractorSignatureBytes = (byte_t*)sqlite3_column_blob(stmt, 7);
        lamport::Signature::Shared contractorSignature = nullptr;
        if (contractorSignatureBytes != nullptr) {
            contractorSignature = make_shared<lamport::Signature>(
                                      contractorSignatureBytes);
        }

        auto ownKeysSetHash = make_shared<lamport::KeyHash>(
                                  (byte_t*)sqlite3_column_blob(stmt, 8));

        auto contractorKeysSetHash = make_shared<lamport::KeyHash>(
                                         (byte_t*)sqlite3_column_blob(stmt, 9));

        return make_shared<AuditRecord>(
                   number,
                   incomingAmount,
                   outgoingAmount,
                   balance,
                   ownKeyHash,
                   ownSignature,
                   contractorKeyHash,
                   contractorSignature,
                   ownKeysSetHash,
                   contractorKeysSetHash);
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
                   "our_key_hash, our_signature, contractor_key_hash, contractor_signature, "
                   "own_keys_set_hash, contractor_keys_set_hash FROM " +
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

        auto ownKeyHash = make_shared<lamport::KeyHash>(
                              (byte_t*)sqlite3_column_blob(stmt.get(), 4));

        auto ownSignature = make_shared<lamport::Signature>(
                                (byte_t*)sqlite3_column_blob(stmt.get(), 5));

        auto contractorKeyHashBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 6);
        lamport::KeyHash::Shared contractorKeyHash = nullptr;
        if (contractorKeyHashBytes != nullptr) {
            contractorKeyHash = make_shared<lamport::KeyHash>(
                                    contractorKeyHashBytes);
        }

        auto contractorSignatureBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 7);
        lamport::Signature::Shared contractorSignature = nullptr;
        if (contractorSignatureBytes != nullptr) {
            contractorSignature = make_shared<lamport::Signature>(
                                      contractorSignatureBytes);
        }

        auto ownKeysSetHash = make_shared<lamport::KeyHash>(
                                  (byte_t*)sqlite3_column_blob(stmt.get(), 8));

        auto contractorKeysSetHash = make_shared<lamport::KeyHash>(
                                         (byte_t*)sqlite3_column_blob(stmt.get(), 9));

        result.push_back(
            make_shared<AuditRecord>(
                number,
                incomingAmount,
                outgoingAmount,
                balance,
                ownKeyHash,
                ownSignature,
                contractorKeyHash,
                contractorSignature,
                ownKeysSetHash,
                contractorKeysSetHash));
    }
    return result;
}

bool AuditHandlerSQLite::isContainsKeyHash(
    lamport::KeyHash::Shared keyHash) const
{
    string query = "SELECT number FROM " + mTableName + " WHERE our_key_hash = ? OR contractor_key_hash = ? LIMIT 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_blob(stmt.get(), 1, keyHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::isContainsKeyHash: "
                      "Bad binding of Own KeyHash; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 2, keyHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandlerSQLite::isContainsKeyHash: "
                      "Bad binding of Contractor KeyHash; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_step(stmt.get());
    auto result = rc == SQLITE_ROW;
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
