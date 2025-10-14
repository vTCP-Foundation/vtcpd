#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "core/payments/reservations/AmountReservation.h"
#include "core/transactions/transactions/base/TransactionUUID.h"

using namespace testing;

// Test Category 4: AmountReservation Enhancement (4 tests)

TEST(AmountReservation, ConstructorWithEquivalent) {
    TransactionUUID uuid;
    TrustLineAmount amount(500);
    AmountReservation::ReservationDirection direction = AmountReservation::Outgoing;
    SerializedEquivalent equivalent = 3;

    AmountReservation reservation(uuid, amount, direction, equivalent);

    EXPECT_EQ(reservation.transactionUUID(), uuid);
    EXPECT_EQ(reservation.amount(), amount);
    EXPECT_EQ(reservation.direction(), direction);
    EXPECT_EQ(reservation.equivalent(), equivalent);
}

TEST(AmountReservation, EquivalentGetter) {
    TransactionUUID uuid;
    TrustLineAmount amount(1000);
    SerializedEquivalent equivalent = 7;

    AmountReservation reservation(
        uuid, amount, AmountReservation::Incoming, equivalent);

    EXPECT_EQ(reservation.equivalent(), 7);
}

TEST(AmountReservation, EqualityWithEquivalent) {
    TransactionUUID uuid;
    TrustLineAmount amount(1000);

    AmountReservation reservation1(
        uuid, amount, AmountReservation::Outgoing, SerializedEquivalent(1));
    AmountReservation reservation2(
        uuid, amount, AmountReservation::Outgoing, SerializedEquivalent(1));
    AmountReservation reservation3(
        uuid, amount, AmountReservation::Outgoing, SerializedEquivalent(2));

    EXPECT_EQ(reservation1, reservation2);
    EXPECT_NE(reservation1, reservation3);  // Different equivalent
}

TEST(AmountReservation, EqualityWithDifferentAmounts) {
    TransactionUUID uuid;
    SerializedEquivalent equivalent = 1;

    AmountReservation reservation1(
        uuid, TrustLineAmount(1000), AmountReservation::Outgoing, equivalent);
    AmountReservation reservation2(
        uuid, TrustLineAmount(2000), AmountReservation::Outgoing, equivalent);

    EXPECT_NE(reservation1, reservation2);  // Different amounts
}

TEST(AmountReservation, EqualityWithDifferentDirections) {
    TransactionUUID uuid;
    TrustLineAmount amount(1000);
    SerializedEquivalent equivalent = 1;

    AmountReservation reservation1(uuid, amount, AmountReservation::Outgoing, equivalent);
    AmountReservation reservation2(uuid, amount, AmountReservation::Incoming, equivalent);

    EXPECT_NE(reservation1, reservation2);  // Different directions
}

TEST(AmountReservation, EqualityWithDifferentUUIDs) {
    TransactionUUID uuid1;
    TransactionUUID uuid2;
    TrustLineAmount amount(1000);
    SerializedEquivalent equivalent = 1;

    AmountReservation reservation1(uuid1, amount, AmountReservation::Outgoing, equivalent);
    AmountReservation reservation2(uuid2, amount, AmountReservation::Outgoing, equivalent);

    EXPECT_NE(reservation1, reservation2);  // Different UUIDs
}

TEST(AmountReservation, InequalityOperator) {
    TransactionUUID uuid;
    TrustLineAmount amount(1000);

    AmountReservation reservation1(
        uuid, amount, AmountReservation::Outgoing, SerializedEquivalent(1));
    AmountReservation reservation2(
        uuid, amount, AmountReservation::Outgoing, SerializedEquivalent(2));

    EXPECT_TRUE(reservation1 != reservation2);
}

TEST(AmountReservation, AssignmentOperator) {
    TransactionUUID uuid1;
    TransactionUUID uuid2;

    AmountReservation reservation1(
        uuid1, TrustLineAmount(1000), AmountReservation::Outgoing, SerializedEquivalent(1));
    AmountReservation reservation2(
        uuid2, TrustLineAmount(2000), AmountReservation::Incoming, SerializedEquivalent(2));

    reservation1 = reservation2;

    EXPECT_EQ(reservation1.transactionUUID(), uuid2);
    EXPECT_EQ(reservation1.amount(), TrustLineAmount(2000));
    EXPECT_EQ(reservation1.direction(), AmountReservation::Incoming);
    EXPECT_EQ(reservation1.equivalent(), SerializedEquivalent(2));
}

TEST(AmountReservation, FieldsComplete) {
    TransactionUUID uuid;
    AmountReservation reservation(
        uuid,
        TrustLineAmount(100),
        AmountReservation::Incoming,
        SerializedEquivalent(5));

    // Verify all getters work
    auto uuid_ret = reservation.transactionUUID();
    auto amount = reservation.amount();
    auto direction = reservation.direction();
    auto equivalent = reservation.equivalent();

    SUCCEED();  // Compilation is the test
}
