#include "HistoryStorage.h"

HistoryStorage::HistoryStorage(
    sqlite3 *dbConnection,
    const string &mainTableName,
    const string &additionalTableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mMainTableName(mainTableName),
    mAdditionalTableName(additionalTableName),
    mLog(logger)
{
    // Validate input parameters.
    if (dbConnection == nullptr) {
        throw ValueError("HistoryStorage::constructor: Database connection cannot be null.");
    }

    if (mainTableName.empty()) {
        throw ValueError("HistoryStorage::constructor: Main table name cannot be empty.");
    }

    if (additionalTableName.empty()) {
        throw ValueError("HistoryStorage::constructor: Additional table name cannot be empty.");
    }

    // Create main table
    string query = "CREATE TABLE IF NOT EXISTS " + mMainTableName +
                   "(operation_uuid BLOB NOT NULL, "
                   "operation_timestamp INTEGER NOT NULL, "
                   "record_type INTEGER NOT NULL, "
                   "record_body BLOB NOT NULL, "
                   "record_body_bytes_count INT NOT NULL, "
                   "equivalent INTEGER NOT NULL, "
                   "command_uuid BLOB);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::constructor: Failed to create main table '" + mMainTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create indexes for main table
    query = "CREATE INDEX IF NOT EXISTS " + mMainTableName
            + "_operation_uuid_idx on " + mMainTableName + "(operation_uuid);";
    SQLiteStatementRAII uuidIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(uuidIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::constructor: Failed to create operation_uuid index on main table '" + mMainTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    query = "CREATE INDEX IF NOT EXISTS " + mMainTableName
            + "_operation_timestamp_idx on " + mMainTableName + "(operation_timestamp);";
    SQLiteStatementRAII timestampIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(timestampIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::constructor: Failed to create operation_timestamp index on main table '" + mMainTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    query = "CREATE INDEX IF NOT EXISTS " + mMainTableName
            + "_record_type_idx on " + mMainTableName + "(record_type);";
    SQLiteStatementRAII recordTypeIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(recordTypeIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::constructor: Failed to create record_type index on main table '" + mMainTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    query = "CREATE INDEX IF NOT EXISTS " + mMainTableName
            + "_command_uuid_idx on " + mMainTableName + "(command_uuid);";
    SQLiteStatementRAII commandUuidIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(commandUuidIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::constructor: Failed to create command_uuid index on main table '" + mMainTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    query = "CREATE INDEX IF NOT EXISTS " + mMainTableName
            + "_equivalent_idx on " + mMainTableName + "(equivalent);";
    SQLiteStatementRAII equivalentIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(equivalentIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::constructor: Failed to create equivalent index on main table '" + mMainTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create additional table
    query = "CREATE TABLE IF NOT EXISTS " + mAdditionalTableName +
            "(operation_uuid BLOB NOT NULL, "
            "operation_timestamp INTEGER NOT NULL, "
            "record_type INTEGER NOT NULL, "
            "record_body BLOB NOT NULL, "
            "record_body_bytes_count INT NOT NULL, "
            "equivalent INTEGER NOT NULL);";
    SQLiteStatementRAII additionalStmt(mDataBase, query.c_str());
    rc = sqlite3_step(additionalStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::constructor: Failed to create additional table '" + mAdditionalTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create indexes for additional table
    query = "CREATE INDEX IF NOT EXISTS " + mAdditionalTableName
            + "_operation_uuid_idx on " + mAdditionalTableName + "(operation_uuid);";
    SQLiteStatementRAII additionalUuidIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(additionalUuidIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::constructor: Failed to create operation_uuid index on additional table '" + mAdditionalTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    query = "CREATE INDEX IF NOT EXISTS " + mAdditionalTableName
            + "_operation_timestamp_idx on " + mAdditionalTableName + "(operation_timestamp);";
    SQLiteStatementRAII additionalTimestampIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(additionalTimestampIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::constructor: Failed to create operation_timestamp index on additional table '" + mAdditionalTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    query = "CREATE INDEX IF NOT EXISTS " + mAdditionalTableName
            + "_record_type_idx on " + mAdditionalTableName + "(record_type);";
    SQLiteStatementRAII additionalRecordTypeIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(additionalRecordTypeIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::constructor: Failed to create record_type index on additional table '" + mAdditionalTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    query = "CREATE INDEX IF NOT EXISTS " + mAdditionalTableName
            + "_equivalent_idx on " + mAdditionalTableName + "(equivalent);";
    SQLiteStatementRAII additionalEquivalentIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(additionalEquivalentIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::constructor: Failed to create equivalent index on additional table '" + mAdditionalTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "HistoryStorage initialized: mainTable=" << mMainTableName
           << ", additionalTable=" << mAdditionalTableName;
#endif
}

void HistoryStorage::saveTrustLineRecord(
    TrustLineRecord::Shared record,
    const SerializedEquivalent equivalent)
{
    if (!record) {
        throw ValueError("HistoryStorage::saveTrustLineRecord: Record cannot be null.");
    }

    string query = "INSERT INTO " + mMainTableName
                   + "(operation_uuid, operation_timestamp, equivalent, "
      "record_type, record_body, record_body_bytes_count) "
      "VALUES(?, ?, ?, ?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, record->operationUUID().data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::saveTrustLineRecord: Failed to bind operation_uuid. "
                      "Equivalent=" + to_string(equivalent) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(record->timestamp());
    rc = sqlite3_bind_int64(stmt.get(), 2, timestamp);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::saveTrustLineRecord: Failed to bind operation_timestamp. "
                      "Equivalent=" + to_string(equivalent) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 3, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::saveTrustLineRecord: Failed to bind equivalent. "
                      "Equivalent=" + to_string(equivalent) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 4, record->recordType());
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::saveTrustLineRecord: Failed to bind record_type. "
                      "Equivalent=" + to_string(equivalent) + ", RecordType=" + to_string(record->recordType()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    auto serializedTrustLineRecordAndSize = record->serializedHistoryRecordBody();
    rc = sqlite3_bind_blob(stmt.get(), 5, serializedTrustLineRecordAndSize.first.get(),
                           (int) serializedTrustLineRecordAndSize.second, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::saveTrustLineRecord: Failed to bind record_body. "
                      "Equivalent=" + to_string(equivalent) + ", BodySize=" + to_string(serializedTrustLineRecordAndSize.second) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 6, (int) serializedTrustLineRecordAndSize.second);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::saveTrustLineRecord: Failed to bind record_body_bytes_count. "
                      "Equivalent=" + to_string(equivalent) + ", BodySize=" + to_string(serializedTrustLineRecordAndSize.second) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::saveTrustLineRecord: Failed to execute INSERT. "
                      "Equivalent=" + to_string(equivalent) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Trust line record saved: Equivalent=" << equivalent
           << ", BodySize=" << serializedTrustLineRecordAndSize.second;
#endif
}

void HistoryStorage::savePaymentRecord(
    PaymentRecord::Shared record)
{
    if (!record) {
        throw ValueError("HistoryStorage::savePaymentRecord: Record cannot be null.");
    }

    switch (record->paymentOperationType()) {
    case PaymentRecord::OutgoingPaymentType:
        savePaymentMainOutgoingRecord(record);
        break;
    case PaymentRecord::IncomingPaymentType:
        savePaymentMainIncomingRecord(record);
        break;
    default:
        throw ValueError("HistoryStorage::savePaymentRecord: Invalid payment operation type=" +
                         to_string(record->paymentOperationType()) + ".");
    }
}

void HistoryStorage::savePaymentMainOutgoingRecord(
    PaymentRecord::Shared record)
{
    if (!record) {
        throw ValueError("HistoryStorage::savePaymentMainOutgoingRecord: Record cannot be null.");
    }

    string query = "INSERT INTO " + mMainTableName
                   + "(operation_uuid, operation_timestamp, equivalent, record_type, "
      "record_body, record_body_bytes_count, command_uuid) "
      "VALUES(?, ?, ?, ?, ?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, record->operationUUID().data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainOutgoingRecord: Failed to bind operation_uuid. "
                      "Equivalent=" + to_string(record->equivalent()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(record->timestamp());
    rc = sqlite3_bind_int64(stmt.get(), 2, timestamp);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainOutgoingRecord: Failed to bind operation_timestamp. "
                      "Equivalent=" + to_string(record->equivalent()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 3, record->equivalent());
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainOutgoingRecord: Failed to bind equivalent. "
                      "Equivalent=" + to_string(record->equivalent()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 4, record->recordType());
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainOutgoingRecord: Failed to bind record_type. "
                      "Equivalent=" + to_string(record->equivalent()) + ", RecordType=" + to_string(record->recordType()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    auto serializedPaymentRecordAndSize = record->serializedHistoryRecordBody();
    rc = sqlite3_bind_blob(stmt.get(), 5, serializedPaymentRecordAndSize.first.get(),
                           (int) serializedPaymentRecordAndSize.second, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainOutgoingRecord: Failed to bind record_body. "
                      "Equivalent=" + to_string(record->equivalent()) + ", BodySize=" + to_string(serializedPaymentRecordAndSize.second) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 6, (int) serializedPaymentRecordAndSize.second);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainOutgoingRecord: Failed to bind record_body_bytes_count. "
                      "Equivalent=" + to_string(record->equivalent()) + ", BodySize=" + to_string(serializedPaymentRecordAndSize.second) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 7, record->commandUUID().data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainOutgoingRecord: Failed to bind command_uuid. "
                      "Equivalent=" + to_string(record->equivalent()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::savePaymentMainOutgoingRecord: Failed to execute INSERT. "
                      "Equivalent=" + to_string(record->equivalent()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Outgoing payment record saved: Equivalent=" << record->equivalent()
           << ", BodySize=" << serializedPaymentRecordAndSize.second;
#endif
}

void HistoryStorage::savePaymentMainIncomingRecord(
    PaymentRecord::Shared record)
{
    if (!record) {
        throw ValueError("HistoryStorage::savePaymentMainIncomingRecord: Record cannot be null.");
    }

    string query = "INSERT INTO " + mMainTableName
                   + "(operation_uuid, operation_timestamp, equivalent, "
      "record_type, record_body, record_body_bytes_count) "
      "VALUES(?, ?, ?, ?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, record->operationUUID().data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainIncomingRecord: Failed to bind operation_uuid. "
                      "Equivalent=" + to_string(record->equivalent()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(record->timestamp());
    rc = sqlite3_bind_int64(stmt.get(), 2, timestamp);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainIncomingRecord: Failed to bind operation_timestamp. "
                      "Equivalent=" + to_string(record->equivalent()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 3, record->equivalent());
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainIncomingRecord: Failed to bind equivalent. "
                      "Equivalent=" + to_string(record->equivalent()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 4, record->recordType());
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainIncomingRecord: Failed to bind record_type. "
                      "Equivalent=" + to_string(record->equivalent()) + ", RecordType=" + to_string(record->recordType()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    auto serializedPaymentRecordAndSize = record->serializedHistoryRecordBody();
    rc = sqlite3_bind_blob(stmt.get(), 5, serializedPaymentRecordAndSize.first.get(),
                           (int) serializedPaymentRecordAndSize.second, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainIncomingRecord: Failed to bind record_body. "
                      "Equivalent=" + to_string(record->equivalent()) + ", BodySize=" + to_string(serializedPaymentRecordAndSize.second) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 6, (int) serializedPaymentRecordAndSize.second);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentMainIncomingRecord: Failed to bind record_body_bytes_count. "
                      "Equivalent=" + to_string(record->equivalent()) + ", BodySize=" + to_string(serializedPaymentRecordAndSize.second) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::savePaymentMainIncomingRecord: Failed to execute INSERT. "
                      "Equivalent=" + to_string(record->equivalent()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Incoming payment record saved: Equivalent=" << record->equivalent()
           << ", BodySize=" << serializedPaymentRecordAndSize.second;
#endif
}

void HistoryStorage::savePaymentAdditionalRecord(
    PaymentAdditionalRecord::Shared record,
    const SerializedEquivalent equivalent)
{
    if (!record) {
        throw ValueError("HistoryStorage::savePaymentAdditionalRecord: Record cannot be null.");
    }

    string query = "INSERT INTO " + mAdditionalTableName
                   + "(operation_uuid, operation_timestamp, equivalent, record_type, record_body, record_body_bytes_count) "
      "VALUES(?, ?, ?, ?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, record->operationUUID().data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentAdditionalRecord: Failed to bind operation_uuid. "
                      "Equivalent=" + to_string(equivalent) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(record->timestamp());
    rc = sqlite3_bind_int64(stmt.get(), 2, timestamp);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentAdditionalRecord: Failed to bind operation_timestamp. "
                      "Equivalent=" + to_string(equivalent) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 3, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentAdditionalRecord: Failed to bind equivalent. "
                      "Equivalent=" + to_string(equivalent) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 4, record->recordType());
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentAdditionalRecord: Failed to bind record_type. "
                      "Equivalent=" + to_string(equivalent) + ", RecordType=" + to_string(record->recordType()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    auto serializedPaymentRecordAndSize = record->serializedHistoryRecordBody();
    rc = sqlite3_bind_blob(stmt.get(), 5, serializedPaymentRecordAndSize.first.get(),
                           (int) serializedPaymentRecordAndSize.second, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentAdditionalRecord: Failed to bind record_body. "
                      "Equivalent=" + to_string(equivalent) + ", BodySize=" + to_string(serializedPaymentRecordAndSize.second) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 6, (int) serializedPaymentRecordAndSize.second);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::savePaymentAdditionalRecord: Failed to bind record_body_bytes_count. "
                      "Equivalent=" + to_string(equivalent) + ", BodySize=" + to_string(serializedPaymentRecordAndSize.second) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("HistoryStorage::savePaymentAdditionalRecord: Failed to execute INSERT. "
                      "Equivalent=" + to_string(equivalent) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Additional payment record saved: Equivalent=" << equivalent
           << ", BodySize=" << serializedPaymentRecordAndSize.second;
#endif
}

vector<TrustLineRecord::Shared> HistoryStorage::allTrustLineRecords(
    const SerializedEquivalent equivalent,
    size_t recordsCount,
    size_t fromRecord,
    DateTime timeFrom,
    bool isTimeFromPresent,
    DateTime timeTo,
    bool isTimeToPresent)
{
    vector<TrustLineRecord::Shared> result;
    string query = "SELECT operation_uuid, operation_timestamp, record_body, record_body_bytes_count FROM "
                   + mMainTableName + " WHERE equivalent = ? AND record_type = ? ";
    if (isTimeFromPresent) {
        query += " AND operation_timestamp >= ? ";
    }
    if (isTimeToPresent) {
        query += " AND operation_timestamp <= ? ";
    }
    query += " ORDER BY operation_timestamp DESC LIMIT ? OFFSET ?;";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int idxParam = 1;
    int rc = sqlite3_bind_int(stmt.get(), idxParam++, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allTrustLineRecords: Failed to bind equivalent. "
                      "Equivalent=" + to_string(equivalent) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), idxParam++, Record::TrustLineRecordType);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allTrustLineRecords: Failed to bind record_type. "
                      "Equivalent=" + to_string(equivalent) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    if (isTimeFromPresent) {
        GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(timeFrom);
        rc = sqlite3_bind_int64(stmt.get(), idxParam++, timestamp);
        if (rc != SQLITE_OK) {
            throw IOError("HistoryStorage::allTrustLineRecords: Failed to bind time_from. "
                          "Equivalent=" + to_string(equivalent) + ", Timestamp=" + to_string(timestamp) +
                          ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
        }
    }

    if (isTimeToPresent) {
        GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(timeTo);
        rc = sqlite3_bind_int64(stmt.get(), idxParam++, timestamp);
        if (rc != SQLITE_OK) {
            throw IOError("HistoryStorage::allTrustLineRecords: Failed to bind time_to. "
                          "Equivalent=" + to_string(equivalent) + ", Timestamp=" + to_string(timestamp) +
                          ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
        }
    }

    rc = sqlite3_bind_int(stmt.get(), idxParam++, (int)recordsCount);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allTrustLineRecords: Failed to bind records_count. "
                      "Equivalent=" + to_string(equivalent) + ", RecordsCount=" + to_string(recordsCount) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), idxParam, (int)fromRecord);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allTrustLineRecords: Failed to bind from_record. "
                      "Equivalent=" + to_string(equivalent) + ", FromRecord=" + to_string(fromRecord) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        result.push_back(deserializeTrustLineRecord(stmt.get()));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Trust line records retrieved: Equivalent=" << equivalent
           << ", Count=" << result.size()
           << ", RequestedCount=" << recordsCount;
#endif

    return result;
}

vector<PaymentRecord::Shared> HistoryStorage::allPaymentRecords(
    const SerializedEquivalent equivalent,
    size_t recordsCount,
    size_t fromRecord,
    DateTime timeFrom,
    bool isTimeFromPresent,
    DateTime timeTo,
    bool isTimeToPresent)
{
    vector<PaymentRecord::Shared> result;
    string query = "SELECT operation_uuid, operation_timestamp, record_body, record_body_bytes_count FROM "
                   + mMainTableName + " WHERE equivalent = ? AND record_type = ? ";
    if (isTimeFromPresent) {
        query += " AND operation_timestamp >= ? ";
    }
    if (isTimeToPresent) {
        query += " AND operation_timestamp <= ? ";
    }
    query += " ORDER BY operation_timestamp DESC LIMIT ? OFFSET ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allPaymentRecords: "
                      "Bad query; sqlite error: " + to_string(rc));
    }
    int idxParam = 1;
    rc = sqlite3_bind_int(stmt, idxParam++, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allPaymentRecords: "
                      "Bad binding of Equivalent; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, idxParam++, Record::PaymentRecordType);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allPaymentRecords: "
                      "Bad binding of RecordType; sqlite error: " + to_string(rc));
    }
    if (isTimeFromPresent) {
        GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(timeFrom);
        rc = sqlite3_bind_int64(stmt, idxParam++, timestamp);
        if (rc != SQLITE_OK) {
            throw IOError("HistoryStorage::allPaymentRecords: "
                          "Bad binding of TimeFrom; sqlite error: " + to_string(rc));
        }
    }
    if (isTimeToPresent) {
        GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(timeTo);
        rc = sqlite3_bind_int64(stmt, idxParam++, timestamp);
        if (rc != SQLITE_OK) {
            throw IOError("HistoryStorage::allPaymentRecords: "
                          "Bad binding of TimeTo; sqlite error: " + to_string(rc));
        }
    }
    rc = sqlite3_bind_int(stmt, idxParam++, (int)recordsCount);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allPaymentRecords: "
                      "Bad binding of recordsCount; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, idxParam, (int)fromRecord);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allPaymentRecords: "
                      "Bad binding of fromRecord; sqlite error: " + to_string(rc));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW ) {
        result.push_back(
            deserializePaymentRecord(
                equivalent,
                stmt));
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return result;
}

vector<PaymentRecord::Shared> HistoryStorage::paymentRecordsAllEquivalents(
    size_t recordsCount,
    size_t fromRecord,
    DateTime timeFrom,
    bool isTimeFromPresent,
    DateTime timeTo,
    bool isTimeToPresent)
{
    vector<PaymentRecord::Shared> result;
    string query = "SELECT operation_uuid, operation_timestamp, record_body, record_body_bytes_count, equivalent FROM "
                   + mMainTableName + " WHERE record_type = ? ";
    if (isTimeFromPresent) {
        query += " AND operation_timestamp >= ? ";
    }
    if (isTimeToPresent) {
        query += " AND operation_timestamp <= ? ";
    }
    query += " ORDER BY operation_timestamp DESC LIMIT ? OFFSET ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::paymentRecordsAllEquivalents: "
                      "Bad query; sqlite error: " + to_string(rc));
    }
    int idxParam = 1;
    rc = sqlite3_bind_int(stmt, idxParam++, Record::PaymentRecordType);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::paymentRecordsAllEquivalents: "
                      "Bad binding of RecordType; sqlite error: " + to_string(rc));
    }
    if (isTimeFromPresent) {
        GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(timeFrom);
        rc = sqlite3_bind_int64(stmt, idxParam++, timestamp);
        if (rc != SQLITE_OK) {
            throw IOError("HistoryStorage::paymentRecordsAllEquivalents: "
                          "Bad binding of TimeFrom; sqlite error: " + to_string(rc));
        }
    }
    if (isTimeToPresent) {
        GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(timeTo);
        rc = sqlite3_bind_int64(stmt, idxParam++, timestamp);
        if (rc != SQLITE_OK) {
            throw IOError("HistoryStorage::paymentRecordsAllEquivalents: "
                          "Bad binding of TimeTo; sqlite error: " + to_string(rc));
        }
    }
    rc = sqlite3_bind_int(stmt, idxParam++, (int)recordsCount);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::paymentRecordsAllEquivalents: "
                      "Bad binding of recordsCount; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, idxParam, (int)fromRecord);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::paymentRecordsAllEquivalents: "
                      "Bad binding of fromRecord; sqlite error: " + to_string(rc));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW ) {
        auto equivalent = (SerializedEquivalent)sqlite3_column_int(stmt, 4);
        result.push_back(
            deserializePaymentRecord(
                equivalent,
                stmt));
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return result;
}

size_t HistoryStorage::countRecordsByType(
    Record::RecordType recordType,
    const SerializedEquivalent equivalent)
{
    string query = "SELECT count(*) FROM "
                   + mMainTableName + " WHERE equivalent = ? AND record_type = ? ";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::countRecordsByType: Failed to bind equivalent. "
                      "Equivalent=" + to_string(equivalent) + ", RecordType=" + to_string(recordType) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, recordType);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::countRecordsByType: Failed to bind record_type. "
                      "Equivalent=" + to_string(equivalent) + ", RecordType=" + to_string(recordType) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_ROW) {
        throw IOError("HistoryStorage::countRecordsByType: Failed to execute count query. "
                      "Equivalent=" + to_string(equivalent) + ", RecordType=" + to_string(recordType) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    auto result = (size_t)sqlite3_column_int(stmt.get(), 0);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Records count retrieved: Equivalent=" << equivalent
           << ", RecordType=" << recordType
           << ", Count=" << result;
#endif

    return result;
}

size_t HistoryStorage::countRecordsByTypeAllEquivalents(
    Record::RecordType recordType)
{
    string query = "SELECT count(*) FROM "
                   + mMainTableName + " WHERE record_type = ? ";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, recordType);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::countRecordsByTypeAllEquivalents: Failed to bind record_type. "
                      "RecordType=" + to_string(recordType) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_ROW) {
        throw IOError("HistoryStorage::countRecordsByTypeAllEquivalents: Failed to execute count query. "
                      "RecordType=" + to_string(recordType) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    auto result = (size_t)sqlite3_column_int(stmt.get(), 0);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Records count retrieved (all equivalents): RecordType=" << recordType
           << ", Count=" << result;
#endif

    return result;
}

vector<PaymentRecord::Shared> HistoryStorage::allPaymentRecords(
    const SerializedEquivalent equivalent,
    size_t recordsCount,
    size_t fromRecord,
    DateTime timeFrom,
    bool isTimeFromPresent,
    DateTime timeTo,
    bool isTimeToPresent,
    const TrustLineAmount& lowBoundaryAmount,
    bool isLowBoundaryAmountPresent,
    const TrustLineAmount& highBoundaryAmount,
    bool isHighBoundaryAmountPresent)
{
    if (!isLowBoundaryAmountPresent && !isHighBoundaryAmountPresent) {
        return allPaymentRecords(
                   equivalent,
                   recordsCount,
                   fromRecord,
                   timeFrom,
                   isTimeFromPresent,
                   timeTo,
                   isTimeToPresent);
    }
    vector<PaymentRecord::Shared> result;
    size_t paymentRecordsCount = countRecordsByType(
                                     Record::PaymentRecordType,
                                     equivalent);
    size_t currentOffset = 0;
    size_t countRecordsUnderConditions = 0;
    while (result.size() < recordsCount && currentOffset < paymentRecordsCount) {
        auto paymentRecords = allPaymentRecords(
                                  equivalent,
                                  kPortionRequestSize,
                                  currentOffset,
                                  timeFrom,
                                  isTimeFromPresent,
                                  timeTo,
                                  isTimeToPresent);
        for (auto &paymentRecord : paymentRecords) {
            bool recordUnderConditions = true;
            if (isLowBoundaryAmountPresent) {
                recordUnderConditions = recordUnderConditions &&
                                        (paymentRecord->amount() >= lowBoundaryAmount);
            }
            if (isHighBoundaryAmountPresent) {
                recordUnderConditions = recordUnderConditions &&
                                        (paymentRecord->amount() <= highBoundaryAmount);
            }
            if (recordUnderConditions) {
                countRecordsUnderConditions++;
                if (countRecordsUnderConditions > fromRecord) {
                    result.push_back(paymentRecord);
                }
            }
            if (result.size() >= recordsCount) {
                break;
            }
        }
        currentOffset += kPortionRequestSize;
    }
    return result;
}

vector<PaymentRecord::Shared> HistoryStorage::paymentRecordsAllEquivalents(
    size_t recordsCount,
    size_t fromRecord,
    DateTime timeFrom,
    bool isTimeFromPresent,
    DateTime timeTo,
    bool isTimeToPresent,
    const TrustLineAmount& lowBoundaryAmount,
    bool isLowBoundaryAmountPresent,
    const TrustLineAmount& highBoundaryAmount,
    bool isHighBoundaryAmountPresent)
{
    if (!isLowBoundaryAmountPresent && !isHighBoundaryAmountPresent) {
        return paymentRecordsAllEquivalents(
                   recordsCount,
                   fromRecord,
                   timeFrom,
                   isTimeFromPresent,
                   timeTo,
                   isTimeToPresent);
    }
    vector<PaymentRecord::Shared> result;
    size_t paymentRecordsCount = countRecordsByTypeAllEquivalents(
                                     Record::PaymentRecordType);
    size_t currentOffset = 0;
    size_t countRecordsUnderConditions = 0;
    while (result.size() < recordsCount && currentOffset < paymentRecordsCount) {
        auto paymentRecords = paymentRecordsAllEquivalents(
                                  kPortionRequestSize,
                                  currentOffset,
                                  timeFrom,
                                  isTimeFromPresent,
                                  timeTo,
                                  isTimeToPresent);
        for (auto &paymentRecord : paymentRecords) {
            bool recordUnderConditions = true;
            if (isLowBoundaryAmountPresent) {
                recordUnderConditions = recordUnderConditions &&
                                        (paymentRecord->amount() >= lowBoundaryAmount);
            }
            if (isHighBoundaryAmountPresent) {
                recordUnderConditions = recordUnderConditions &&
                                        (paymentRecord->amount() <= highBoundaryAmount);
            }
            if (recordUnderConditions) {
                countRecordsUnderConditions++;
                if (countRecordsUnderConditions > fromRecord) {
                    result.push_back(paymentRecord);
                }
            }
            if (result.size() >= recordsCount) {
                break;
            }
        }
        currentOffset += kPortionRequestSize;
    }
    return result;
}

vector<PaymentAdditionalRecord::Shared> HistoryStorage::allPaymentAdditionalRecords(
    const SerializedEquivalent equivalent,
    size_t recordsCount,
    size_t fromRecord,
    DateTime timeFrom,
    bool isTimeFromPresent,
    DateTime timeTo,
    bool isTimeToPresent,
    const TrustLineAmount& lowBoundaryAmount,
    bool isLowBoundaryAmountPresent,
    const TrustLineAmount& highBoundaryAmount,
    bool isHighBoundaryAmountPresent)
{
    if (!isLowBoundaryAmountPresent && !isHighBoundaryAmountPresent) {
        return allPaymentAdditionalRecords(
                   equivalent,
                   recordsCount,
                   fromRecord,
                   timeFrom,
                   isTimeFromPresent,
                   timeTo,
                   isTimeToPresent);
    }
    vector<PaymentAdditionalRecord::Shared> result;
    size_t paymentRecordsCount = countRecordsByType(
                                     Record::PaymentAdditionalRecordType,
                                     equivalent);
    size_t currentOffset = 0;
    size_t countRecordsUnderConditions = 0;
    while (result.size() < recordsCount && currentOffset < paymentRecordsCount) {
        auto paymentAdditionalRecords = allPaymentAdditionalRecords(
                                            equivalent,
                                            kPortionRequestSize,
                                            currentOffset,
                                            timeFrom,
                                            isTimeFromPresent,
                                            timeTo,
                                            isTimeToPresent);
        for (auto &paymentAdditionalRecord : paymentAdditionalRecords) {
            bool recordUnderConditions = true;
            if (isLowBoundaryAmountPresent) {
                recordUnderConditions = recordUnderConditions &&
                                        (paymentAdditionalRecord->amount() >= lowBoundaryAmount);
            }
            if (isHighBoundaryAmountPresent) {
                recordUnderConditions = recordUnderConditions &&
                                        (paymentAdditionalRecord->amount() <= highBoundaryAmount);
            }
            if (recordUnderConditions) {
                countRecordsUnderConditions++;
                if (countRecordsUnderConditions > fromRecord) {
                    result.push_back(paymentAdditionalRecord);
                }
            }
            if (result.size() >= recordsCount) {
                break;
            }
        }
        currentOffset += kPortionRequestSize;
    }
    return result;
}

vector<PaymentAdditionalRecord::Shared> HistoryStorage::allPaymentAdditionalRecords(
    const SerializedEquivalent equivalent,
    size_t recordsCount,
    size_t fromRecord,
    DateTime timeFrom,
    bool isTimeFromPresent,
    DateTime timeTo,
    bool isTimeToPresent)
{
    vector<PaymentAdditionalRecord::Shared> result;
    string query = "SELECT operation_uuid, operation_timestamp, record_body, record_body_bytes_count FROM "
                   + mAdditionalTableName + " WHERE equivalent = ? AND record_type = ? ";
    if (isTimeFromPresent) {
        query += " AND operation_timestamp >= ? ";
    }
    if (isTimeToPresent) {
        query += " AND operation_timestamp <= ? ";
    }
    query += " ORDER BY operation_timestamp DESC LIMIT ? OFFSET ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allAdditionalPaymentRecords: "
                      "Bad query; sqlite error: " + to_string(rc));
    }
    int idxParam = 1;
    rc = sqlite3_bind_int(stmt, idxParam++, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allAdditionalPaymentRecords: "
                      "Bad binding of Equivalent; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, idxParam++, Record::PaymentAdditionalRecordType);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allAdditionalPaymentRecords: "
                      "Bad binding of RecordType; sqlite error: " + to_string(rc));
    }
    if (isTimeFromPresent) {
        GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(timeFrom);
        rc = sqlite3_bind_int64(stmt, idxParam++, timestamp);
        if (rc != SQLITE_OK) {
            throw IOError("HistoryStorage::allAdditionalPaymentRecords: "
                          "Bad binding of TimeFrom; sqlite error: " + to_string(rc));
        }
    }
    if (isTimeToPresent) {
        GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(timeTo);
        rc = sqlite3_bind_int64(stmt, idxParam++, timestamp);
        if (rc != SQLITE_OK) {
            throw IOError("HistoryStorage::allAdditionalPaymentRecords: "
                          "Bad binding of TimeTo; sqlite error: " + to_string(rc));
        }
    }
    rc = sqlite3_bind_int(stmt, idxParam++, (int)recordsCount);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allAdditionalPaymentRecords: "
                      "Bad binding of recordsCount; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, idxParam, (int)fromRecord);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::allAdditionalPaymentRecords: "
                      "Bad binding of fromRecord; sqlite error: " + to_string(rc));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW ) {
        result.push_back(
            deserializePaymentAdditionalRecord(
                stmt));
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return result;
}

vector<Record::Shared> HistoryStorage::recordsPortionWithContractor(
    const SerializedEquivalent equivalent,
    size_t recordsCount,
    size_t fromRecord)
{
    vector<Record::Shared> result;
    string query = "SELECT operation_uuid, operation_timestamp, record_body, record_body_bytes_count, record_type"
                   " FROM " + mMainTableName + " WHERE equivalent = ? "
                   "ORDER BY operation_timestamp DESC LIMIT ? OFFSET ?;";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::recordsPortionWithContractor: "
                      "Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 1, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::recordsPortionWithContractor: "
                      "Bad binding of Equivalent; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 2, (int)recordsCount);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::recordsPortionWithContractor: "
                      "Bad binding of recordsCount; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 3, (int)fromRecord);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::recordsPortionWithContractor: "
                      "Bad binding of fromRecord; sqlite error: " + to_string(rc));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW ) {
        int recordType = sqlite3_column_int(stmt, 4);
        switch (recordType) {
        case Record::TrustLineRecordType:
            result.push_back(
                deserializeTrustLineRecord(
                    stmt));
            break;
        case Record::PaymentRecordType:
            result.push_back(
                deserializePaymentRecord(
                    equivalent,
                    stmt));
            break;
        default:
            throw ValueError("HistoryStorage::recordsPortionWithContractor: "
                             "invalid record type");
        }
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return result;
}

vector<Record::Shared> HistoryStorage::recordsWithContractor(
    vector<BaseAddress::Shared> contractorAddresses,
    const SerializedEquivalent equivalent,
    size_t recordsCount,
    size_t fromRecord)
{
    vector<Record::Shared> result;

    string query = "SELECT count(*) FROM " + mMainTableName + " WHERE equivalent = ?";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::recordsWithContractor: "
                      "Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt, 1, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::recordsWithContractor: "
                      "Bad binding of Equivalent; sqlite error: " + to_string(rc));
    }
    sqlite3_step(stmt);
    auto allRecordsCount = (size_t)sqlite3_column_int(stmt, 0);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    debug() << "all records count: " << allRecordsCount;
#endif
    size_t currentOffset = 0;
    size_t countRecordsUnderConditions = 0;
    while (result.size() < recordsCount && currentOffset < allRecordsCount) {
        auto records = recordsPortionWithContractor(
                           equivalent,
                           kPortionRequestSize,
                           currentOffset);
        for (auto &record : records) {
            if (record->contractor()->containsAddresses(contractorAddresses)) {
                countRecordsUnderConditions++;
                if (countRecordsUnderConditions > fromRecord) {
                    result.push_back(record);
                }
            }
            if (result.size() >= recordsCount) {
                break;
            }
        }
        currentOffset += kPortionRequestSize;
    }
    return result;
}

vector<PaymentRecord::Shared> HistoryStorage::paymentRecordsByCommandUUID(
    const CommandUUID &commandUUID)
{
    vector<PaymentRecord::Shared> result;
    string query = "SELECT operation_uuid, operation_timestamp, record_body, record_body_bytes_count FROM "
                   + mMainTableName + " WHERE record_type = ? AND command_uuid = ?";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::paymentRecordsByCommandUUID: "
                      "Bad query; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_bind_int(stmt, 1, Record::PaymentRecordType);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::paymentRecordsByCommandUUID: "
                      "Bad binding of RecordType; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_bind_blob(stmt, 2, commandUUID.data, CommandUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::paymentRecordsByCommandUUID: "
                      "Bad binding of commandUUID; sqlite error: " + to_string(rc));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW ) {
        result.push_back(
            deserializePaymentRecord(
                0,
                stmt));
        return result;
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return result;
}

vector<PaymentRecord::Shared> HistoryStorage::paymentRecordsByTransactionUUID(
    const TransactionUUID &transactionUUID)
{
    vector<PaymentRecord::Shared> result;
    string query = "SELECT operation_uuid, operation_timestamp, record_body, record_body_bytes_count FROM "
                   + mMainTableName + " WHERE record_type = ? AND operation_uuid = ?";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::paymentRecordsByTransactionUUID: "
                      "Bad query; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_bind_int(stmt, 1, Record::PaymentRecordType);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::paymentRecordsByTransactionUUID: "
                      "Bad binding of RecordType; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_bind_blob(stmt, 2, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::paymentRecordsByTransactionUUID: "
                      "Bad binding of TransactionUUID; sqlite error: " + to_string(rc));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW ) {
        result.push_back(
            deserializePaymentRecord(
                0,
                stmt));
        return result;
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return result;
}

bool HistoryStorage::whetherOperationWasConducted(
    const TransactionUUID &transactionUUID)
{
    string query = "SELECT operation_uuid FROM "
                   + mMainTableName + " WHERE operation_uuid = ? LIMIT 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("HistoryStorage::whetherOperationWasConducted: Failed to bind transaction_uuid. "
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    bool result = (sqlite3_step(stmt.get()) == SQLITE_ROW);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Operation existence checked: Found=" << (result ? "true" : "false");
#endif

    return result;
}

TrustLineRecord::Shared HistoryStorage::deserializeTrustLineRecord(
    sqlite3_stmt *stmt)
{
    TransactionUUID operationUUID((uint8_t*)sqlite3_column_blob(stmt, 0));
    auto timestamp = (GEOEpochTimestamp)sqlite3_column_int64(stmt, 1);
    auto recordBodyBytesCount = (size_t)sqlite3_column_int(stmt, 3);
    BytesShared recordBody = tryMalloc(recordBodyBytesCount);
    memcpy(
        recordBody.get(),
        sqlite3_column_blob(stmt, 2),
        recordBodyBytesCount);

    return make_shared<TrustLineRecord>(
               operationUUID,
               timestamp,
               recordBody);
}

PaymentRecord::Shared HistoryStorage::deserializePaymentRecord(
    const SerializedEquivalent equivalent,
    sqlite3_stmt *stmt)
{
    TransactionUUID operationUUID((uint8_t*)sqlite3_column_blob(stmt, 0));
    auto timestamp = (GEOEpochTimestamp)sqlite3_column_int64(stmt, 1);
    auto recordBodyBytesCount = (size_t)sqlite3_column_int(stmt, 3);
    BytesShared recordBody = tryMalloc(recordBodyBytesCount);
    memcpy(
        recordBody.get(),
        sqlite3_column_blob(stmt, 2),
        recordBodyBytesCount);

    return make_shared<PaymentRecord>(
               equivalent,
               operationUUID,
               timestamp,
               recordBody);
}

PaymentAdditionalRecord::Shared HistoryStorage::deserializePaymentAdditionalRecord(
    sqlite3_stmt *stmt)
{
    TransactionUUID operationUUID((uint8_t*)sqlite3_column_blob(stmt, 0));
    auto timestamp = (GEOEpochTimestamp)sqlite3_column_int64(stmt, 1);
    auto recordBodyBytesCount = (size_t)sqlite3_column_int(stmt, 3);
    BytesShared recordBody = tryMalloc(recordBodyBytesCount);
    memcpy(
        recordBody.get(),
        sqlite3_column_blob(stmt, 2),
        recordBodyBytesCount);

    return make_shared<PaymentAdditionalRecord>(
               operationUUID,
               timestamp,
               recordBody);
}

LoggerStream HistoryStorage::info() const
{
    return mLog.info(logHeader());
}

LoggerStream HistoryStorage::debug() const
{
    return mLog.debug(logHeader());
}

LoggerStream HistoryStorage::warning() const
{
    return mLog.warning(logHeader());
}

const string HistoryStorage::logHeader() const
{
    stringstream s;
    s << "[HistoryStorage]";
    return s.str();
}
