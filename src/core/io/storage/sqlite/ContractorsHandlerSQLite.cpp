#include "ContractorsHandlerSQLite.h"

ContractorsHandlerSQLite::ContractorsHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    sqlite3_stmt *stmt;
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   "(id INTEGER PRIMARY KEY, "
                   "id_on_contractor_side INTEGER, "
                   "crypto_key BLOB NOT NULL, "
                   "is_confirmed INTEGER NOT NULL DEFAULT 0);";
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::creating table: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("ContractorsHandlerSQLite::creating table: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_id_idx on " + mTableName + " (id);";
    rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::creating index for ID: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("ContractorsHandlerSQLite::creating index for ID: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
}

void ContractorsHandlerSQLite::saveContractor(
    Contractor::Shared contractor)
{
    string query = "INSERT INTO " + mTableName +
                   " (id, crypto_key) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::saveContractor: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_bind_int(stmt, 1, contractor->getID());
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::saveContractor: "
                      "Bad binding of ID; sqlite error: " +
                      to_string(rc));
    }

    auto cryptoKey = contractor->cryptoKey();
    vector<byte_t> cryptoKeyBlob;
    if (cryptoKey)
        cryptoKey->serialize(cryptoKeyBlob);
    rc = sqlite3_bind_blob(stmt, 2, &cryptoKeyBlob[0], cryptoKeyBlob.size(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::saveContractor: "
                      "Bad binding of cryptoKey; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "prepare inserting is completed successfully";
#endif
    } else {
        throw IOError("ContractorsHandlerSQLite::saveContractor: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

void ContractorsHandlerSQLite::saveContractorFull(
    Contractor::Shared contractor)
{
    string query = "INSERT INTO " + mTableName +
                   "(id, id_on_contractor_side, crypto_key, is_confirmed) "
                   "VALUES (?, ?, ?, 1);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::saveContractorFull: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_bind_int(stmt, 1, contractor->getID());
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::saveContractorFull: "
                      "Bad binding of ID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 2, contractor->ownIdOnContractorSide());
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::saveContractorFull: "
                      "Bad binding of ID on contractor side; sqlite error: " +
                      to_string(rc));
    }
    auto cryptoKey = contractor->cryptoKey();
    vector<byte_t> cryptoKeyBlob;
    if (cryptoKey)
        cryptoKey->serialize(cryptoKeyBlob);
    rc = sqlite3_bind_blob(stmt, 3, &cryptoKeyBlob[0], cryptoKeyBlob.size(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::saveContractorFull: "
                      "Bad binding of cryptoKey; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "prepare inserting is completed successfully";
#endif
    } else {
        throw IOError("ContractorsHandlerSQLite::saveContractorFull: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

void ContractorsHandlerSQLite::saveConfirmationInfo(
    Contractor::Shared contractor)
{
    string query = "UPDATE " + mTableName +
                   " SET id_on_contractor_side = ?, crypto_key = ?, is_confirmed = 1 WHERE id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::saveConfirmationInfo: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_bind_int(stmt, 1, contractor->ownIdOnContractorSide());
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::saveConfirmationInfo: "
                      "Bad binding of ID on contractor side; sqlite error: " +
                      to_string(rc));
    }
    auto cryptoKey = contractor->cryptoKey();
    vector<byte_t> cryptoKeyBlob;
    if (cryptoKey) {
        cryptoKey->serialize(cryptoKeyBlob);
    }
    rc = sqlite3_bind_blob(stmt, 2, &cryptoKeyBlob[0], cryptoKeyBlob.size(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::saveConfirmationInfo: "
                      "Bad binding of cryptoKey; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 3, contractor->getID());
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::saveConfirmationInfo: "
                      "Bad binding of ID; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "updateing is completed successfully";
#endif
    } else {
        throw IOError("ContractorsHandlerSQLite::saveConfirmationInfo: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("No data were modified");
    }
}

void ContractorsHandlerSQLite::updateCryptoKey(
    Contractor::Shared contractor)
{
    string query = "UPDATE " + mTableName + " SET crypto_key = ?, is_confirmed = 1 WHERE id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::updateCryptoKey: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }

    auto cryptoKey = contractor->cryptoKey();
    vector<byte_t> cryptoKeyBlob;
    if (cryptoKey) {
        cryptoKey->serialize(cryptoKeyBlob);
    }
    rc = sqlite3_bind_blob(stmt, 1, &cryptoKeyBlob[0], cryptoKeyBlob.size(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::updateCryptoKey: "
                      "Bad binding of cryptoKey; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 2, contractor->getID());
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::updateCryptoKey: "
                      "Bad binding of ID; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "updateing is completed successfully";
#endif
    } else {
        throw IOError("ContractorsHandlerSQLite::updateCryptoKey: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("No data were modified");
    }
}

void ContractorsHandlerSQLite::updateChannelIdOnContractorSide(
    Contractor::Shared contractor)
{
    string query = "UPDATE " + mTableName + " SET id_on_contractor_side = ? WHERE id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::updateChannelIdOnContractorSide: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }

    auto cryptoKey = contractor->cryptoKey();
    vector<byte_t> cryptoKeyBlob;
    if (cryptoKey) {
        cryptoKey->serialize(cryptoKeyBlob);
    }
    rc = sqlite3_bind_int(stmt, 1, contractor->ownIdOnContractorSide());
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::updateChannelIdOnContractorSide: "
                      "Bad binding of ChannelOnContractorSide; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 2, contractor->getID());
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::updateChannelIdOnContractorSide: "
                      "Bad binding of ID; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "updateing is completed successfully";
#endif
    } else {
        throw IOError("ContractorsHandlerSQLite::updateChannelIdOnContractorSide: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("No data were modified");
    }
}

vector<Contractor::Shared> ContractorsHandlerSQLite::allContractors()
{
    string queryCount = "SELECT count(*) FROM " + mTableName;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, queryCount.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::allContractors: "
                      "Bad count query; sqlite error: " +
                      to_string(rc));
    }
    sqlite3_step(stmt);
    auto rowCount = (uint32_t)sqlite3_column_int(stmt, 0);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    vector<Contractor::Shared> result;
    result.reserve(rowCount);

    string query = "SELECT id, id_on_contractor_side, crypto_key, is_confirmed FROM " + mTableName;
    rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::allContractors: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto id = (ContractorID)sqlite3_column_int(stmt, 0);
        auto idOnContractorSide = (ContractorID)sqlite3_column_int(stmt, 1);

        vector<uint8_t> cryptoKeyBlob(sqlite3_column_bytes(stmt, 2));
        memcpy(
            &cryptoKeyBlob[0],
            sqlite3_column_blob(stmt, 2),
            cryptoKeyBlob.size());
        MsgEncryptor::KeyTrio::Shared cryptoKey =
            make_shared<MsgEncryptor::KeyTrio>(cryptoKeyBlob);

        auto isChannelConfirmed = sqlite3_column_int(stmt, 3);

        try {
            result.push_back(
                make_shared<Contractor>(
                    id,
                    idOnContractorSide,
                    cryptoKey,
                    isChannelConfirmed == 1));
        } catch (...) {
            throw Exception("ContractorsHandlerSQLite::allContractors. "
                            "Unable to create contractor instance from DB.");
        }
    }
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return result;
}

vector<TrustLineID> ContractorsHandlerSQLite::allIDs()
{
    string queryCount = "SELECT count(*) FROM " + mTableName;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, queryCount.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::allIDs: "
                      "Bad count query; sqlite error: " +
                      to_string(rc));
    }
    sqlite3_step(stmt);
    auto rowCount = (uint32_t)sqlite3_column_int(stmt, 0);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    vector<ContractorID> result;
    result.reserve(rowCount);

    string query = "SELECT id FROM " + mTableName;
    rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::allIDs: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(
            (ContractorID)sqlite3_column_int(stmt, 0));
    }
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return result;
}

void ContractorsHandlerSQLite::removeContractor(
    ContractorID contractorID)
{
    string query = "DELETE FROM " + mTableName + " WHERE id = ?";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2( mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::removeContractor: "
                          "Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 1, contractorID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorsHandlerSQLite::removeContractor: "
                          "Bad binding of ContractorID; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "deleting is completed successfully";
#endif
    } else {
        throw IOError("ContractorsHandlerSQLite::removeContractor: "
                          "Run query; sqlite error: " + to_string(rc));
    }
    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("No data were deleted");
    }
}

LoggerStream ContractorsHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream ContractorsHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string ContractorsHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "[ContractorsHandler]";
    return s.str();
}