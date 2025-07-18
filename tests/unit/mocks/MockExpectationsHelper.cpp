#include "MockSQLiteAPI.h"
#include <gmock/gmock.h>

using ::testing::Return;
using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::InSequence;

void MockExpectationsHelper::expectSuccessfulTableCreation(MockSQLiteAPI& mockAPI, const std::string& tableName) {
    // Mock successful table creation
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(SQLITE_OK));
    EXPECT_CALL(mockAPI, sqlite3_step(_))
        .WillOnce(Return(SQLITE_DONE));
    EXPECT_CALL(mockAPI, sqlite3_finalize(_))
        .WillOnce(Return(SQLITE_OK));
}

void MockExpectationsHelper::expectFailedTableCreation(MockSQLiteAPI& mockAPI, const std::string& tableName, int errorCode) {
    // Mock failed table creation
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(errorCode));
    EXPECT_CALL(mockAPI, sqlite3_errmsg(_))
        .WillOnce(Return("Mock error message"));
}

void MockExpectationsHelper::expectSuccessfulIndexCreation(MockSQLiteAPI& mockAPI, const std::string& indexName) {
    // Mock successful index creation
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(SQLITE_OK));
    EXPECT_CALL(mockAPI, sqlite3_step(_))
        .WillOnce(Return(SQLITE_DONE));
    EXPECT_CALL(mockAPI, sqlite3_finalize(_))
        .WillOnce(Return(SQLITE_OK));
}

void MockExpectationsHelper::expectFailedIndexCreation(MockSQLiteAPI& mockAPI, const std::string& indexName, int errorCode) {
    // Mock failed index creation
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(errorCode));
    EXPECT_CALL(mockAPI, sqlite3_errmsg(_))
        .WillOnce(Return("Mock error message"));
}

void MockExpectationsHelper::expectSuccessfulInsert(MockSQLiteAPI& mockAPI) {
    // Mock successful insert operation
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(SQLITE_OK));
    EXPECT_CALL(mockAPI, sqlite3_step(_))
        .WillOnce(Return(SQLITE_DONE));
    EXPECT_CALL(mockAPI, sqlite3_finalize(_))
        .WillOnce(Return(SQLITE_OK));
}

void MockExpectationsHelper::expectFailedInsert(MockSQLiteAPI& mockAPI, int errorCode) {
    // Mock failed insert operation
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(SQLITE_OK));
    EXPECT_CALL(mockAPI, sqlite3_step(_))
        .WillOnce(Return(errorCode));
    EXPECT_CALL(mockAPI, sqlite3_errmsg(_))
        .WillOnce(Return("Mock error message"));
    EXPECT_CALL(mockAPI, sqlite3_finalize(_))
        .WillOnce(Return(SQLITE_OK));
}

void MockExpectationsHelper::expectSuccessfulSelect(MockSQLiteAPI& mockAPI) {
    // Mock successful select operation with data
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(SQLITE_OK));
    EXPECT_CALL(mockAPI, sqlite3_step(_))
        .WillOnce(Return(SQLITE_ROW));
    EXPECT_CALL(mockAPI, sqlite3_finalize(_))
        .WillOnce(Return(SQLITE_OK));
}

void MockExpectationsHelper::expectEmptySelect(MockSQLiteAPI& mockAPI) {
    // Mock select operation with no data
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(SQLITE_OK));
    EXPECT_CALL(mockAPI, sqlite3_step(_))
        .WillOnce(Return(SQLITE_DONE));
    EXPECT_CALL(mockAPI, sqlite3_finalize(_))
        .WillOnce(Return(SQLITE_OK));
}

void MockExpectationsHelper::expectFailedSelect(MockSQLiteAPI& mockAPI, int errorCode) {
    // Mock failed select operation
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(errorCode));
    EXPECT_CALL(mockAPI, sqlite3_errmsg(_))
        .WillOnce(Return("Mock error message"));
}

void MockExpectationsHelper::expectSuccessfulUpdate(MockSQLiteAPI& mockAPI) {
    // Mock successful update operation
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(SQLITE_OK));
    EXPECT_CALL(mockAPI, sqlite3_step(_))
        .WillOnce(Return(SQLITE_DONE));
    EXPECT_CALL(mockAPI, sqlite3_changes(_))
        .WillOnce(Return(1));
    EXPECT_CALL(mockAPI, sqlite3_finalize(_))
        .WillOnce(Return(SQLITE_OK));
}

void MockExpectationsHelper::expectFailedUpdate(MockSQLiteAPI& mockAPI, int errorCode) {
    // Mock failed update operation
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(SQLITE_OK));
    EXPECT_CALL(mockAPI, sqlite3_step(_))
        .WillOnce(Return(errorCode));
    EXPECT_CALL(mockAPI, sqlite3_errmsg(_))
        .WillOnce(Return("Mock error message"));
    EXPECT_CALL(mockAPI, sqlite3_finalize(_))
        .WillOnce(Return(SQLITE_OK));
}

void MockExpectationsHelper::expectSuccessfulDelete(MockSQLiteAPI& mockAPI) {
    // Mock successful delete operation
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(SQLITE_OK));
    EXPECT_CALL(mockAPI, sqlite3_step(_))
        .WillOnce(Return(SQLITE_DONE));
    EXPECT_CALL(mockAPI, sqlite3_changes(_))
        .WillOnce(Return(1));
    EXPECT_CALL(mockAPI, sqlite3_finalize(_))
        .WillOnce(Return(SQLITE_OK));
}

void MockExpectationsHelper::expectFailedDelete(MockSQLiteAPI& mockAPI, int errorCode) {
    // Mock failed delete operation
    EXPECT_CALL(mockAPI, sqlite3_prepare_v2(_, _, _, _, _))
        .WillOnce(Return(SQLITE_OK));
    EXPECT_CALL(mockAPI, sqlite3_step(_))
        .WillOnce(Return(errorCode));
    EXPECT_CALL(mockAPI, sqlite3_errmsg(_))
        .WillOnce(Return("Mock error message"));
    EXPECT_CALL(mockAPI, sqlite3_finalize(_))
        .WillOnce(Return(SQLITE_OK));
}

void MockExpectationsHelper::expectSuccessfulConnection(MockSQLiteAPI& mockAPI) {
    // Mock successful database connection
    EXPECT_CALL(mockAPI, sqlite3_open_v2(_, _, _, _))
        .WillOnce(Return(SQLITE_OK));
    EXPECT_CALL(mockAPI, sqlite3_config(_))
        .WillOnce(Return(SQLITE_OK));
}

void MockExpectationsHelper::expectFailedConnection(MockSQLiteAPI& mockAPI, int errorCode) {
    // Mock failed database connection
    EXPECT_CALL(mockAPI, sqlite3_open_v2(_, _, _, _))
        .WillOnce(Return(errorCode));
    EXPECT_CALL(mockAPI, sqlite3_errmsg(_))
        .WillOnce(Return("Mock connection error"));
}

void MockExpectationsHelper::expectSuccessfulBind(MockSQLiteAPI& mockAPI, int parameterCount) {
    // Mock successful parameter binding
    for (int i = 0; i < parameterCount; ++i) {
        EXPECT_CALL(mockAPI, sqlite3_bind_blob(_, _, _, _, _))
            .WillOnce(Return(SQLITE_OK));
    }
}

void MockExpectationsHelper::expectFailedBind(MockSQLiteAPI& mockAPI, int parameterIndex, int errorCode) {
    // Mock failed parameter binding
    EXPECT_CALL(mockAPI, sqlite3_bind_blob(_, parameterIndex, _, _, _))
        .WillOnce(Return(errorCode));
    EXPECT_CALL(mockAPI, sqlite3_errmsg(_))
        .WillOnce(Return("Mock bind error"));
}

void MockExpectationsHelper::expectErrorMessage(MockSQLiteAPI& mockAPI, const std::string& message) {
    // Mock error message
    EXPECT_CALL(mockAPI, sqlite3_errmsg(_))
        .WillOnce(Return(message.c_str()));
} 