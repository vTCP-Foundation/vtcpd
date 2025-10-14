#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "core/transactions/transactions/regular/payments/base/PathReservation.h"
#include "core/common/Types.h"

using namespace testing;

namespace {

// Helper function to create TrustLineAmount
inline ConstSharedTrustLineAmount A(uint64_t v) {
    return make_shared<const TrustLineAmount>(v);
}

} // namespace

// Test Category 1: PathReservation (2 tests)

TEST(PathReservation, Construction) {
    PathID pathID = 123;
    auto amount = A(1000);
    SerializedEquivalent equivalent = 5;

    PathReservation reservation(pathID, amount, equivalent);

    EXPECT_EQ(reservation.pathID, pathID);
    EXPECT_EQ(*reservation.amount, TrustLineAmount(1000));
    EXPECT_EQ(reservation.equivalent, equivalent);
}

TEST(PathReservation, FieldAccess) {
    PathID pathID = 456;
    auto amount = A(2500);
    SerializedEquivalent equivalent = 10;

    PathReservation reservation(pathID, amount, equivalent);

    // Access all fields directly
    PathID accessedPathID = reservation.pathID;
    TrustLineAmount accessedAmount = *reservation.amount;
    SerializedEquivalent accessedEquiv = reservation.equivalent;

    EXPECT_EQ(accessedPathID, 456);
    EXPECT_EQ(accessedAmount, TrustLineAmount(2500));
    EXPECT_EQ(accessedEquiv, 10);
}
