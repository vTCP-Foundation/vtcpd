#include "AuditHandler.h"

AuditHandler::AuditHandler(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    sqlite3_stmt *stmt;
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   "(number INTEGER NOT NULL, "
                   "trust_line_id INTEGER NOT NULL, "
                   "our_key_hash BLOB NOT NULL, "
                   "our_signature BLOB NOT NULL, "
                   "contractor_key_hash BLOB DEFAULT NULL, "
                   "contractor_signature BLOB DEFAULT NULL, "
                   "balance BLOB NOT NULL, "
                   "outgoing_amount BLOB NOT NULL, "
                   "incoming_amount BLOB NOT NULL, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE);";
    int rc = sqlite3_prepare_v2( mDataBase, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::creating table: "
                          "Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("AuditHandler::creating table: "
                          "Run query; sqlite error: " + to_string(rc));
    }

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName
            + "_number_trust_line_id_idx on " + mTableName + "(number, trust_line_id);";
    rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::creating index for Number and TrustLineID: "
                          "Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("AuditHandler::creating index for Number and TrustLineID: "
                          "Run query; sqlite error: " + to_string(rc));
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
}

void AuditHandler::saveFullAudit(
    AuditNumber number,
    TrustLineID trustLineID,
    lamport::KeyHash::Shared ownKeyHash,
    lamport::Signature::Shared ownSignature,
    lamport::KeyHash::Shared contractorKeyHash,
    lamport::Signature::Shared contractorSignature,
    const TrustLineAmount &incomingAmount,
    const TrustLineAmount &outgoingAmount,
    const TrustLineBalance &balance)
{
    string query = "INSERT INTO " + mTableName +
                   "(number, trust_line_id, our_key_hash, our_signature, contractor_key_hash, "
                   "contractor_signature, incoming_amount, outgoing_amount, "
                   "balance) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2( mDataBase, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveFullAudit: "
                          "Bad query; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_bind_int(stmt, 1, number);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveFullAudit: "
                          "Bad binding of Audit Number; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 2, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveFullAudit: "
                          "Bad binding of ID; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt, 3, ownKeyHash->data(),
                          (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveFullAudit: "
                          "Bad binding of OwnKeyHash; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt, 4, ownSignature->data(), (int)ownSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveFullAudit: "
                          "Bad binding of OnwSignature; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt, 5, contractorKeyHash->data(),
                          (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveFullAudit: "
                          "Bad binding of ContractorKeyHash; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt, 6, contractorSignature->data(), (int)contractorSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveFullAudit: "
                          "Bad binding of ContractorSignature; sqlite error: " + to_string(rc));
    }
    vector<byte> incomingAmountBufferBytes = trustLineAmountToBytes(incomingAmount);
    rc = sqlite3_bind_blob(stmt, 7, incomingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveFullAudit: "
                          "Bad binding of Incoming Amount; sqlite error: " + to_string(rc));
    }
    vector<byte> outgoingAmountBufferBytes = trustLineAmountToBytes(outgoingAmount);
    rc = sqlite3_bind_blob(stmt, 8, outgoingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveFullAudit: "
                          "Bad binding of Outgoing Amount; sqlite error: " + to_string(rc));
    }
    vector<byte> balanceBufferBytes = trustLineBalanceToBytes(const_cast<TrustLineBalance&>(balance));
    rc = sqlite3_bind_blob(stmt, 9, balanceBufferBytes.data(), kTrustLineBalanceSerializeBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveFullAudit: "
                          "Bad binding of Balance; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "prepare inserting is completed successfully";
#endif
    } else {
        throw IOError("AuditHandler::saveFullAudit: "
                          "Run query; sqlite error: " + to_string(rc));
    }
}

void AuditHandler::saveOwnAuditPart(
    AuditNumber number,
    TrustLineID trustLineID,
    lamport::KeyHash::Shared ownKeyHash,
    lamport::Signature::Shared ownSignature,
    const TrustLineAmount &incomingAmount,
    const TrustLineAmount &outgoingAmount,
    const TrustLineBalance &balance)
{
    string query = "INSERT INTO " + mTableName +
                   "(number, trust_line_id, our_key_hash, our_signature, "
                   "incoming_amount, outgoing_amount, balance) "
                   "VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2( mDataBase, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveOwnAuditPart: "
                          "Bad query; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_bind_int(stmt, 1, number);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveOwnAuditPart: "
                          "Bad binding of Audit Number; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 2, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveOwnAuditPart: "
                          "Bad binding of ID; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt, 3, ownKeyHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveOwnAuditPart: "
                          "Bad binding of OwnKeyHash; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt, 4, ownSignature->data(), (int)ownSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveOwnAuditPart: "
                          "Bad binding of OnwSignature; sqlite error: " + to_string(rc));
    }
    vector<byte> incomingAmountBufferBytes = trustLineAmountToBytes(incomingAmount);
    rc = sqlite3_bind_blob(stmt, 5, incomingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveOwnAuditPart: "
                          "Bad binding of Incoming Amount; sqlite error: " + to_string(rc));
    }
    vector<byte> outgoingAmountBufferBytes = trustLineAmountToBytes(outgoingAmount);
    rc = sqlite3_bind_blob(stmt, 6, outgoingAmountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveOwnAuditPart: "
                          "Bad binding of Outgoing Amount; sqlite error: " + to_string(rc));
    }
    vector<byte> balanceBufferBytes = trustLineBalanceToBytes(const_cast<TrustLineBalance&>(balance));
    rc = sqlite3_bind_blob(stmt, 7, balanceBufferBytes.data(), kTrustLineBalanceSerializeBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveOwnAuditPart: "
                          "Bad binding of Balance; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("AuditHandler::saveOwnAuditPart: "
                          "Run query; sqlite error: " + to_string(rc));
    }
}

void AuditHandler::saveContractorAuditPart(
    AuditNumber number,
    TrustLineID trustLineID,
    lamport::KeyHash::Shared contractorKeyHash,
    lamport::Signature::Shared contractorSignature)
{
    string query = "UPDATE " + mTableName +
                   " SET contractor_key_hash = ?, contractor_signature = ? "
                   "WHERE trust_line_id = ? AND number = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2( mDataBase, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveContractorAuditPart: "
                          "Bad query; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_bind_blob(stmt, 1, contractorKeyHash->data(),
                           (int)lamport::KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveContractorAuditPart: "
                          "Bad binding of ContractorKeyHash; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt, 2, contractorSignature->data(), (int)contractorSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveContractorAuditPart: "
                          "Bad binding of ContractorSignature; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 3, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveContractorAuditPart: "
                          "Bad binding of ID; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 4, number);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::saveContractorAuditPart: "
                          "Bad binding of Audit Number; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("AuditHandler::saveContractorAuditPart: "
                          "Run query; sqlite error: " + to_string(rc));
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("No data were modified");
    }
}

const AuditRecord::Shared AuditHandler::getActualAudit(
    TrustLineID trustLineID)
{
    string query = "SELECT number, incoming_amount, outgoing_amount, balance FROM " + mTableName
                   + " WHERE trust_line_id = ? ORDER BY number DESC LIMIT 1;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::getActualAudit: "
                          "Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::getActualAudit: "
                          "Bad binding of Trust Line ID; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        auto number = (KeyNumber)sqlite3_column_int(stmt, 0);
        auto incomingAmountBytes = (byte*)sqlite3_column_blob(stmt, 1);
        vector<byte> incomingAmountBufferBytes(
            incomingAmountBytes,
            incomingAmountBytes + kTrustLineAmountBytesCount);
        TrustLineAmount incomingAmount = bytesToTrustLineAmount(incomingAmountBufferBytes);

        auto outgoingAmountBytes = (byte*)sqlite3_column_blob(stmt, 2);
        vector<byte> outgoingAmountBufferBytes(
            outgoingAmountBytes,
            outgoingAmountBytes + kTrustLineAmountBytesCount);
        TrustLineAmount outgoingAmount = bytesToTrustLineAmount(outgoingAmountBufferBytes);

        auto balanceBytes = (byte*)sqlite3_column_blob(stmt, 3);
        vector<byte> balanceBufferBytes(
            balanceBytes,
            balanceBytes + kTrustLineBalanceSerializeBytesCount);
        TrustLineBalance balance = bytesToTrustLineBalance(balanceBufferBytes);
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        return make_shared<AuditRecord>(
            number,
            incomingAmount,
            outgoingAmount,
            balance);
    } else {
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        throw NotFoundError("AuditHandler::getActualAudit: "
                                "There are no records with requested trust line id");
    }
}

const AuditRecord::Shared AuditHandler::getActualAuditFull(
    TrustLineID trustLineID)
{
    string query = "SELECT number, incoming_amount, outgoing_amount, balance, "
                   "our_key_hash, our_signature, contractor_key_hash, contractor_signature FROM " + mTableName
                   + " WHERE trust_line_id = ? ORDER BY number DESC LIMIT 1;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::getActualAuditFull: "
                              "Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditHandler::getActualAuditFull: "
                          "Bad binding of Trust Line ID; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        auto number = (KeyNumber)sqlite3_column_int(stmt, 0);
        auto incomingAmountBytes = (byte*)sqlite3_column_blob(stmt, 1);
        vector<byte> incomingAmountBufferBytes(
            incomingAmountBytes,
            incomingAmountBytes + kTrustLineAmountBytesCount);
        TrustLineAmount incomingAmount = bytesToTrustLineAmount(incomingAmountBufferBytes);

        auto outgoingAmountBytes = (byte*)sqlite3_column_blob(stmt, 2);
        vector<byte> outgoingAmountBufferBytes(
            outgoingAmountBytes,
            outgoingAmountBytes + kTrustLineAmountBytesCount);
        TrustLineAmount outgoingAmount = bytesToTrustLineAmount(outgoingAmountBufferBytes);

        auto balanceBytes = (byte*)sqlite3_column_blob(stmt, 3);
        vector<byte> balanceBufferBytes(
            balanceBytes,
            balanceBytes + kTrustLineBalanceSerializeBytesCount);
        TrustLineBalance balance = bytesToTrustLineBalance(balanceBufferBytes);

        auto ownKeyHash = make_shared<lamport::KeyHash>(
            (byte*)sqlite3_column_blob(stmt, 4));

        auto ownSignature = make_shared<lamport::Signature>(
            (byte*)sqlite3_column_blob(stmt, 5));

        auto contractorKeyHashBytes = (byte*)sqlite3_column_blob(stmt, 6);
        lamport::KeyHash::Shared contractorKeyHash = nullptr;
        if (contractorKeyHashBytes != nullptr) {
            contractorKeyHash = make_shared<lamport::KeyHash>(
                contractorKeyHashBytes);
        }

        auto contractorSignatureBytes = (byte*)sqlite3_column_blob(stmt, 7);
        lamport::Signature::Shared contractorSignature = nullptr;
        if (contractorSignatureBytes != nullptr) {
            contractorSignature = make_shared<lamport::Signature>(
                contractorSignatureBytes);
        }

        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        return make_shared<AuditRecord>(
            number,
            incomingAmount,
            outgoingAmount,
            balance,
            ownKeyHash,
            ownSignature,
            contractorKeyHash,
            contractorSignature);
    } else {
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        throw NotFoundError("AuditHandler::getActualAuditFull: "
                                "There are no records with requested trust line id");
    }
}

LoggerStream AuditHandler::info() const
{
    return mLog.info(logHeader());
}

LoggerStream AuditHandler::warning() const
{
    return mLog.warning(logHeader());
}

const string AuditHandler::logHeader() const
{
    stringstream s;
    s << "[AuditHandler]";
    return s.str();
}