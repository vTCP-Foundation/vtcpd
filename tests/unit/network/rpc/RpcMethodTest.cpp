#include <gtest/gtest.h>
#include <set>

#include "core/network/rpc/RpcMethod.h"


// Test that all RpcMethod enum values are distinct
TEST(RpcMethodTest, AllValuesAreDistinct)
{
    std::set<int> values;

    values.insert(static_cast<int>(RpcMethod::Unknown));
    values.insert(static_cast<int>(RpcMethod::GetBlockNumber));
    values.insert(static_cast<int>(RpcMethod::AcceptClaim));
    values.insert(static_cast<int>(RpcMethod::GetClaimStatus));
    values.insert(static_cast<int>(RpcMethod::SubmitClaimVotes));
    values.insert(static_cast<int>(RpcMethod::GetClaimStatuses));

    // If all values are distinct, set size equals number of enum values
    EXPECT_EQ(values.size(), 6);
}

// Test that all expected enum values exist and have expected ordering
TEST(RpcMethodTest, AllExpectedValuesExist)
{
    EXPECT_EQ(static_cast<int>(RpcMethod::Unknown), 0);
    EXPECT_EQ(static_cast<int>(RpcMethod::GetBlockNumber), 1);
    EXPECT_EQ(static_cast<int>(RpcMethod::AcceptClaim), 2);
    EXPECT_EQ(static_cast<int>(RpcMethod::GetClaimStatus), 3);
    EXPECT_EQ(static_cast<int>(RpcMethod::SubmitClaimVotes), 4);
    EXPECT_EQ(static_cast<int>(RpcMethod::GetClaimStatuses), 5);
}

// Test that Unknown is the default/zero value
TEST(RpcMethodTest, UnknownIsDefaultValue)
{
    RpcMethod defaultMethod = static_cast<RpcMethod>(0);
    EXPECT_EQ(defaultMethod, RpcMethod::Unknown);
}

