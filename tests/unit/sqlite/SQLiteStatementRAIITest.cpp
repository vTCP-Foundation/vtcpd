#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <string>
#include <filesystem>

#include "../../../src/core/io/storage/sqlite/SQLiteStatementRAII.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/ValueError.h"

using ::testing::_;
using ::testing::Return;
using ::testing::Throw;

class SQLiteStatementRAIITest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test databases
        testDbDir = std::filesystem::temp_directory_path() / "vtcpd_test_sqlite_stmt";
        std::filesystem::create_directories(testDbDir);
        
        testDbPath = testDbDir / "test_statement.db";
        
        // Create test database
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to open test database: " << sqlite3_errmsg(db);
        
        // Create a test table
        const char* createTable = R"(
            CREATE TABLE test_table (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL,
                value INTEGER
            );
        )";
        
        rc = sqlite3_exec(db, createTable, nullptr, nullptr, nullptr);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to create test table: " << sqlite3_errmsg(db);
        
        // Insert test data
        const char* insertData = R"(
            INSERT INTO test_table (name, value) VALUES 
            ('test1', 100),
            ('test2', 200),
            ('test3', 300);
        )";
        
        rc = sqlite3_exec(db, insertData, nullptr, nullptr, nullptr);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to insert test data: " << sqlite3_errmsg(db);
    }
    
    void TearDown() override {
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
        
        // Clean up test files
        std::error_code ec;
        std::filesystem::remove_all(testDbDir, ec);
        // Ignore cleanup errors
    }
    
    // Helper function to create a raw SQLite statement
    sqlite3_stmt* createRawStatement(const char* query) {
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return nullptr;
        }
        return stmt;
    }

protected:
    std::filesystem::path testDbDir;
    std::filesystem::path testDbPath;
    sqlite3* db = nullptr;
};

// Constructor Tests
TEST_F(SQLiteStatementRAIITest, ConstructorWithValidQuery) {
    const char* query = "SELECT * FROM test_table";
    
    EXPECT_NO_THROW({
        SQLiteStatementRAII stmt(db, query);
        EXPECT_NE(stmt.get(), nullptr);
        EXPECT_NE(stmt.statement(), nullptr);
    });
}

TEST_F(SQLiteStatementRAIITest, ConstructorWithComplexQuery) {
    const char* query = "SELECT id, name, value FROM test_table WHERE value > ? ORDER BY id";
    
    EXPECT_NO_THROW({
        SQLiteStatementRAII stmt(db, query);
        EXPECT_NE(stmt.get(), nullptr);
        
        // Verify we can bind parameters
        int rc = sqlite3_bind_int(stmt.get(), 1, 150);
        EXPECT_EQ(rc, SQLITE_OK);
    });
}

TEST_F(SQLiteStatementRAIITest, ConstructorWithInvalidQuery) {
    const char* invalidQuery = "INVALID SQL SYNTAX HERE";
    
    EXPECT_THROW({
        SQLiteStatementRAII stmt(db, invalidQuery);
    }, IOError);
}

TEST_F(SQLiteStatementRAIITest, ConstructorWithNullDatabase) {
    const char* query = "SELECT * FROM test_table";
    
    EXPECT_THROW({
        SQLiteStatementRAII stmt(nullptr, query);
    }, IOError);
}

TEST_F(SQLiteStatementRAIITest, ConstructorWithEmptyQuery) {
    const char* query = "";
    
    EXPECT_NO_THROW({
        SQLiteStatementRAII stmt(db, query);
        EXPECT_EQ(stmt.get(), nullptr);
    });
}

TEST_F(SQLiteStatementRAIITest, ConstructorWithExistingStatement) {
    sqlite3_stmt* rawStmt = createRawStatement("SELECT * FROM test_table");
    ASSERT_NE(rawStmt, nullptr);
    
    EXPECT_NO_THROW({
        SQLiteStatementRAII stmt(rawStmt);
        EXPECT_EQ(stmt.get(), rawStmt);
        EXPECT_EQ(stmt.statement(), rawStmt);
    });
}

TEST_F(SQLiteStatementRAIITest, ConstructorWithNullStatement) {
    EXPECT_NO_THROW({
        SQLiteStatementRAII stmt(nullptr);
        EXPECT_EQ(stmt.get(), nullptr);
        EXPECT_EQ(stmt.statement(), nullptr);
    });
}

// Destructor and RAII Tests
TEST_F(SQLiteStatementRAIITest, RAIIBehaviorAutomaticCleanup) {
    sqlite3_stmt* stmtPtr = nullptr;
    
    {
        SQLiteStatementRAII stmt(db, "SELECT * FROM test_table");
        stmtPtr = stmt.get();
        EXPECT_NE(stmtPtr, nullptr);
        
        // Statement should be valid while in scope
        int rc = sqlite3_step(stmtPtr);
        EXPECT_TRUE(rc == SQLITE_ROW || rc == SQLITE_DONE);
    }
    
    // After leaving scope, statement should be finalized
    // Attempting to use it should not crash (though behavior is undefined)
    // We can't directly test this without risking undefined behavior
}

TEST_F(SQLiteStatementRAIITest, RAIIBehaviorWithException) {
    sqlite3_stmt* stmtPtr = nullptr;
    
    try {
        SQLiteStatementRAII stmt(db, "SELECT * FROM test_table");
        stmtPtr = stmt.get();
        EXPECT_NE(stmtPtr, nullptr);
        
        // Simulate an exception
        throw std::runtime_error("Test exception");
    } catch (const std::runtime_error&) {
        // Expected exception - statement should still be cleaned up
    }
    
    // Statement should have been cleaned up automatically
    // We can't directly test this without risking undefined behavior
}

// Move Semantics Tests
TEST_F(SQLiteStatementRAIITest, MoveConstructor) {
    SQLiteStatementRAII stmt1(db, "SELECT * FROM test_table");
    sqlite3_stmt* originalPtr = stmt1.get();
    EXPECT_NE(originalPtr, nullptr);
    
    // Move construct
    SQLiteStatementRAII stmt2(std::move(stmt1));
    
    // stmt2 should now own the statement
    EXPECT_EQ(stmt2.get(), originalPtr);
    
    // stmt1 should be empty
    EXPECT_EQ(stmt1.get(), nullptr);
}

TEST_F(SQLiteStatementRAIITest, MoveAssignment) {
    SQLiteStatementRAII stmt1(db, "SELECT * FROM test_table");
    SQLiteStatementRAII stmt2(db, "SELECT COUNT(*) FROM test_table");
    
    sqlite3_stmt* originalPtr1 = stmt1.get();
    sqlite3_stmt* originalPtr2 = stmt2.get();
    
    EXPECT_NE(originalPtr1, nullptr);
    EXPECT_NE(originalPtr2, nullptr);
    EXPECT_NE(originalPtr1, originalPtr2);
    
    // Move assign
    stmt2 = std::move(stmt1);
    
    // stmt2 should now own stmt1's statement
    EXPECT_EQ(stmt2.get(), originalPtr1);
    
    // stmt1 should be empty
    EXPECT_EQ(stmt1.get(), nullptr);
}

TEST_F(SQLiteStatementRAIITest, SelfMoveAssignment) {
    SQLiteStatementRAII stmt(db, "SELECT * FROM test_table");
    sqlite3_stmt* originalPtr = stmt.get();
    EXPECT_NE(originalPtr, nullptr);
    
    // Self move assignment
    stmt = std::move(stmt);
    
    // Should still be valid
    EXPECT_EQ(stmt.get(), originalPtr);
}

// Access Methods Tests
TEST_F(SQLiteStatementRAIITest, StatementAccessMethods) {
    SQLiteStatementRAII stmt(db, "SELECT * FROM test_table");
    
    sqlite3_stmt* ptr1 = stmt.statement();
    sqlite3_stmt* ptr2 = stmt.get();
    sqlite3_stmt* ptr3 = stmt;
    
    EXPECT_NE(ptr1, nullptr);
    EXPECT_EQ(ptr1, ptr2);
    EXPECT_EQ(ptr2, ptr3);
}

TEST_F(SQLiteStatementRAIITest, ConversionOperator) {
    SQLiteStatementRAII stmt(db, "SELECT * FROM test_table WHERE id = ?");
    
    // Should work directly in SQLite API calls
    int rc = sqlite3_bind_int(stmt, 1, 1);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    int id = sqlite3_column_int(stmt, 0);
    EXPECT_EQ(id, 1);
}

// Reset Method Tests
TEST_F(SQLiteStatementRAIITest, ResetValidStatement) {
    SQLiteStatementRAII stmt(db, "SELECT * FROM test_table WHERE value > ?");
    
    // Bind parameter and execute
    int rc = sqlite3_bind_int(stmt.get(), 1, 150);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt.get());
    EXPECT_EQ(rc, SQLITE_ROW);
    
    // Reset should clear bindings and reset statement
    rc = stmt.reset();
    EXPECT_EQ(rc, SQLITE_OK);
    
    // After reset, step should not return a row without new bindings
    rc = sqlite3_step(stmt.get());
    EXPECT_EQ(rc, SQLITE_DONE); // No rows because parameter is unbound (0)
}

TEST_F(SQLiteStatementRAIITest, ResetNullStatement) {
    SQLiteStatementRAII stmt(nullptr);
    
    int rc = stmt.reset();
    EXPECT_EQ(rc, SQLITE_OK);
}

// Release Method Tests
TEST_F(SQLiteStatementRAIITest, ReleaseValidStatement) {
    SQLiteStatementRAII stmt(db, "SELECT * FROM test_table");
    sqlite3_stmt* originalPtr = stmt.get();
    EXPECT_NE(originalPtr, nullptr);
    
    // Release ownership
    sqlite3_stmt* releasedPtr = stmt.release();
    
    EXPECT_EQ(releasedPtr, originalPtr);
    EXPECT_EQ(stmt.get(), nullptr);
    
    // We need to clean up the released statement manually
    sqlite3_finalize(releasedPtr);
}

TEST_F(SQLiteStatementRAIITest, ReleaseNullStatement) {
    SQLiteStatementRAII stmt(nullptr);
    
    sqlite3_stmt* releasedPtr = stmt.release();
    EXPECT_EQ(releasedPtr, nullptr);
    EXPECT_EQ(stmt.get(), nullptr);
}

TEST_F(SQLiteStatementRAIITest, ReleaseAfterRelease) {
    SQLiteStatementRAII stmt(db, "SELECT * FROM test_table");
    
    sqlite3_stmt* firstRelease = stmt.release();
    EXPECT_NE(firstRelease, nullptr);
    
    sqlite3_stmt* secondRelease = stmt.release();
    EXPECT_EQ(secondRelease, nullptr);
    
    // Clean up
    sqlite3_finalize(firstRelease);
}

// Functionality Tests
TEST_F(SQLiteStatementRAIITest, ExecuteSelectQuery) {
    SQLiteStatementRAII stmt(db, "SELECT id, name, value FROM test_table ORDER BY id");
    
    int rowCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        int value = sqlite3_column_int(stmt, 2);
        
        EXPECT_GT(id, 0);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(value, 0);
        
        rowCount++;
    }
    
    EXPECT_EQ(rowCount, 3); // We inserted 3 rows
}

TEST_F(SQLiteStatementRAIITest, ExecuteParameterizedQuery) {
    SQLiteStatementRAII stmt(db, "SELECT name, value FROM test_table WHERE value > ? AND value < ?");
    
    int rc = sqlite3_bind_int(stmt.get(), 1, 150);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_bind_int(stmt.get(), 2, 250);
    EXPECT_EQ(rc, SQLITE_OK);
    
    int rowCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int value = sqlite3_column_int(stmt, 1);
        
        EXPECT_NE(name, nullptr);
        EXPECT_GT(value, 150);
        EXPECT_LT(value, 250);
        
        rowCount++;
    }
    
    EXPECT_EQ(rowCount, 1); // Only test2 with value 200 should match
}

TEST_F(SQLiteStatementRAIITest, ExecuteInsertQuery) {
    SQLiteStatementRAII stmt(db, "INSERT INTO test_table (name, value) VALUES (?, ?)");
    
    int rc = sqlite3_bind_text(stmt.get(), 1, "test4", -1, SQLITE_STATIC);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_bind_int(stmt.get(), 2, 400);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt.get());
    EXPECT_EQ(rc, SQLITE_DONE);
    
    // Verify the insert worked
    SQLiteStatementRAII verifyStmt(db, "SELECT COUNT(*) FROM test_table WHERE name = 'test4'");
    rc = sqlite3_step(verifyStmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    int count = sqlite3_column_int(verifyStmt, 0);
    EXPECT_EQ(count, 1);
}

TEST_F(SQLiteStatementRAIITest, ExecuteUpdateQuery) {
    SQLiteStatementRAII stmt(db, "UPDATE test_table SET value = ? WHERE name = ?");
    
    int rc = sqlite3_bind_int(stmt.get(), 1, 999);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_bind_text(stmt.get(), 2, "test1", -1, SQLITE_STATIC);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt.get());
    EXPECT_EQ(rc, SQLITE_DONE);
    
    // Verify the update worked
    SQLiteStatementRAII verifyStmt(db, "SELECT value FROM test_table WHERE name = 'test1'");
    rc = sqlite3_step(verifyStmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    int value = sqlite3_column_int(verifyStmt, 0);
    EXPECT_EQ(value, 999);
}

TEST_F(SQLiteStatementRAIITest, ExecuteDeleteQuery) {
    SQLiteStatementRAII stmt(db, "DELETE FROM test_table WHERE name = ?");
    
    int rc = sqlite3_bind_text(stmt.get(), 1, "test2", -1, SQLITE_STATIC);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt.get());
    EXPECT_EQ(rc, SQLITE_DONE);
    
    // Verify the delete worked
    SQLiteStatementRAII verifyStmt(db, "SELECT COUNT(*) FROM test_table");
    rc = sqlite3_step(verifyStmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    int count = sqlite3_column_int(verifyStmt, 0);
    EXPECT_EQ(count, 2); // Should have 2 rows left
}

// Error Handling Tests
TEST_F(SQLiteStatementRAIITest, ErrorHandlingInvalidBinding) {
    SQLiteStatementRAII stmt(db, "SELECT * FROM test_table WHERE id = ?");
    
    // Try to bind to parameter index that doesn't exist
    int rc = sqlite3_bind_int(stmt.get(), 2, 1); // Only parameter 1 exists
    EXPECT_NE(rc, SQLITE_OK);
}

TEST_F(SQLiteStatementRAIITest, ErrorHandlingConstraintViolation) {
    // Create table with unique constraint
    const char* createConstraintTable = R"(
        CREATE TABLE constraint_test (
            id INTEGER PRIMARY KEY,
            unique_value INTEGER UNIQUE
        );
    )";
    
    int rc = sqlite3_exec(db, createConstraintTable, nullptr, nullptr, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    // Insert first value
    SQLiteStatementRAII stmt1(db, "INSERT INTO constraint_test (unique_value) VALUES (?)");
    rc = sqlite3_bind_int(stmt1.get(), 1, 42);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt1.get());
    EXPECT_EQ(rc, SQLITE_DONE);
    
    // Try to insert duplicate value
    SQLiteStatementRAII stmt2(db, "INSERT INTO constraint_test (unique_value) VALUES (?)");
    rc = sqlite3_bind_int(stmt2.get(), 1, 42);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt2.get());
    EXPECT_EQ(rc, SQLITE_CONSTRAINT); // Should fail with constraint violation
}

// Multiple Usage Tests
TEST_F(SQLiteStatementRAIITest, ReuseAfterReset) {
    SQLiteStatementRAII stmt(db, "SELECT value FROM test_table WHERE name = ?");
    
    // First execution
    int rc = sqlite3_bind_text(stmt.get(), 1, "test1", -1, SQLITE_STATIC);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt.get());
    EXPECT_EQ(rc, SQLITE_ROW);
    
    int value1 = sqlite3_column_int(stmt.get(), 0);
    EXPECT_EQ(value1, 100);
    
    // Reset and reuse
    stmt.reset();
    
    rc = sqlite3_bind_text(stmt.get(), 1, "test3", -1, SQLITE_STATIC);
    EXPECT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt.get());
    EXPECT_EQ(rc, SQLITE_ROW);
    
    int value3 = sqlite3_column_int(stmt.get(), 0);
    EXPECT_EQ(value3, 300);
}

TEST_F(SQLiteStatementRAIITest, MultipleStatementInstances) {
    SQLiteStatementRAII stmt1(db, "SELECT COUNT(*) FROM test_table");
    SQLiteStatementRAII stmt2(db, "SELECT MAX(value) FROM test_table");
    SQLiteStatementRAII stmt3(db, "SELECT MIN(value) FROM test_table");
    
    // Execute all statements
    int rc1 = sqlite3_step(stmt1);
    int rc2 = sqlite3_step(stmt2);
    int rc3 = sqlite3_step(stmt3);
    
    EXPECT_EQ(rc1, SQLITE_ROW);
    EXPECT_EQ(rc2, SQLITE_ROW);
    EXPECT_EQ(rc3, SQLITE_ROW);
    
    int count = sqlite3_column_int(stmt1, 0);
    int maxValue = sqlite3_column_int(stmt2, 0);
    int minValue = sqlite3_column_int(stmt3, 0);
    
    EXPECT_EQ(count, 3);
    EXPECT_EQ(maxValue, 300);
    EXPECT_EQ(minValue, 100);
} 