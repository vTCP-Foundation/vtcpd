#include <gtest/gtest.h>
#include <set>

#include "core/io/storage/interfaces/PaymentTransactionsHandler.h"

// Task 16-06: Unit tests for PaymentObservingState enum
// Verifies enum values are stable for database compatibility

// Test that all PaymentObservingState enum values are distinct
TEST(PaymentObservingStateTest, AllValuesAreDistinct)
{
    std::set<int> values;

    values.insert(static_cast<int>(PaymentObservingState::Init));
    values.insert(static_cast<int>(PaymentObservingState::Committed));
    values.insert(static_cast<int>(PaymentObservingState::ParticipantsVotesPresent));
    values.insert(static_cast<int>(PaymentObservingState::RejectedByObserving));
    values.insert(static_cast<int>(PaymentObservingState::Conflicted));

    // If all values are distinct, set size equals number of enum values
    EXPECT_EQ(values.size(), 5);
}

// Test that all expected enum values exist and have expected values
TEST(PaymentObservingStateTest, AllExpectedValuesExist)
{
    EXPECT_EQ(static_cast<int>(PaymentObservingState::Init), 0);
    EXPECT_EQ(static_cast<int>(PaymentObservingState::Committed), 1);
    EXPECT_EQ(static_cast<int>(PaymentObservingState::ParticipantsVotesPresent), 2);
    EXPECT_EQ(static_cast<int>(PaymentObservingState::RejectedByObserving), 3);
    EXPECT_EQ(static_cast<int>(PaymentObservingState::Conflicted), 4);
}

// Test that Init is the default/zero value
TEST(PaymentObservingStateTest, InitIsDefaultValue)
{
    PaymentObservingState defaultState = static_cast<PaymentObservingState>(0);
    EXPECT_EQ(defaultState, PaymentObservingState::Init);
}


