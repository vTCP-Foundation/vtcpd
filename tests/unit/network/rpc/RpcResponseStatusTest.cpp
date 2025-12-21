#include <gtest/gtest.h>
#include <set>

#include "core/network/rpc/RpcResponseStatus.h"


// Test that all RpcResponseStatus enum values are distinct
TEST(RpcResponseStatusTest, AllValuesAreDistinct)
{
    std::set<int> values;

    values.insert(static_cast<int>(RpcResponseStatus::Success));
    values.insert(static_cast<int>(RpcResponseStatus::Timeout));
    values.insert(static_cast<int>(RpcResponseStatus::NetworkError));
    values.insert(static_cast<int>(RpcResponseStatus::ParseError));
    values.insert(static_cast<int>(RpcResponseStatus::RpcError));

    // If all values are distinct, set size equals number of enum values
    EXPECT_EQ(values.size(), 5);
}

// Test that all expected enum values exist and have expected ordering
TEST(RpcResponseStatusTest, AllExpectedValuesExist)
{
    EXPECT_EQ(static_cast<int>(RpcResponseStatus::Success), 0);
    EXPECT_EQ(static_cast<int>(RpcResponseStatus::Timeout), 1);
    EXPECT_EQ(static_cast<int>(RpcResponseStatus::NetworkError), 2);
    EXPECT_EQ(static_cast<int>(RpcResponseStatus::ParseError), 3);
    EXPECT_EQ(static_cast<int>(RpcResponseStatus::RpcError), 4);
}

// Test that Success is the default/zero value
TEST(RpcResponseStatusTest, SuccessIsDefaultValue)
{
    RpcResponseStatus defaultStatus = static_cast<RpcResponseStatus>(0);
    EXPECT_EQ(defaultStatus, RpcResponseStatus::Success);
}

