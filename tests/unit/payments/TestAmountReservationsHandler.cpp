#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "core/payments/reservations/AmountReservationsHandler.h"
#include "core/transactions/transactions/base/TransactionUUID.h"
#include "core/common/exceptions/ValueError.h"
#include "core/common/exceptions/NotFoundError.h"

using namespace testing;

// Test Category 5: AmountReservationsHandler Enhancement (15+ tests)

TEST(AmountReservationsHandler, ReserveWithEquivalent) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();
    TrustLineAmount amount(500);
    SerializedEquivalent equivalent(2);

    auto reservation = handler.reserve(
        contractor, uuid, amount, AmountReservation::Outgoing, equivalent);

    ASSERT_NE(reservation, nullptr);
    EXPECT_EQ(reservation->equivalent(), equivalent);
    EXPECT_EQ(reservation->amount(), amount);
}

TEST(AmountReservationsHandler, ReserveMultipleEquivalentsSameContractor) {
    AmountReservationsHandler handler;
    ContractorID contractor(456);
    TransactionUUID uuid1 = TransactionUUID();
    TransactionUUID uuid2 = TransactionUUID();

    auto reservation1 = handler.reserve(
        contractor, uuid1, TrustLineAmount(100),
        AmountReservation::Outgoing, SerializedEquivalent(1));

    auto reservation2 = handler.reserve(
        contractor, uuid2, TrustLineAmount(200),
        AmountReservation::Outgoing, SerializedEquivalent(2));

    ASSERT_NE(reservation1, nullptr);
    ASSERT_NE(reservation2, nullptr);
    EXPECT_EQ(reservation1->equivalent(), SerializedEquivalent(1));
    EXPECT_EQ(reservation2->equivalent(), SerializedEquivalent(2));
}

TEST(AmountReservationsHandler, ReserveThrowsOnZeroAmount) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();

    EXPECT_THROW(
        handler.reserve(contractor, uuid, TrustLineAmount(0),
                       AmountReservation::Outgoing, SerializedEquivalent(1)),
        ValueError);
}

TEST(AmountReservationsHandler, UpdateReservationWithEquivalent) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();
    SerializedEquivalent equivalent(3);

    auto reservation = handler.reserve(
        contractor, uuid, TrustLineAmount(500),
        AmountReservation::Outgoing, equivalent);

    auto updated = handler.updateReservation(
        contractor, reservation, TrustLineAmount(800), equivalent);

    ASSERT_NE(updated, nullptr);
    EXPECT_EQ(updated->amount(), TrustLineAmount(800));
    EXPECT_EQ(updated->equivalent(), equivalent);
}

TEST(AmountReservationsHandler, UpdateReservationThrowsOnZeroAmount) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();

    auto reservation = handler.reserve(
        contractor, uuid, TrustLineAmount(500),
        AmountReservation::Outgoing, SerializedEquivalent(1));

    EXPECT_THROW(
        handler.updateReservation(contractor, reservation, TrustLineAmount(0), SerializedEquivalent(1)),
        ValueError);
}

TEST(AmountReservationsHandler, UpdateReservationThrowsOnNonExistentContractor) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();

    auto reservation = make_shared<const AmountReservation>(
        uuid, TrustLineAmount(500), AmountReservation::Outgoing, SerializedEquivalent(1));

    EXPECT_THROW(
        handler.updateReservation(contractor, reservation, TrustLineAmount(800), SerializedEquivalent(1)),
        NotFoundError);
}

TEST(AmountReservationsHandler, UpdateReservationThrowsOnNonExistentReservation) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid1 = TransactionUUID();
    TransactionUUID uuid2 = TransactionUUID();

    // Reserve with uuid1
    handler.reserve(contractor, uuid1, TrustLineAmount(500),
                   AmountReservation::Outgoing, SerializedEquivalent(1));

    // Try to update with different reservation (uuid2)
    auto nonExistentReservation = make_shared<const AmountReservation>(
        uuid2, TrustLineAmount(500), AmountReservation::Outgoing, SerializedEquivalent(1));

    EXPECT_THROW(
        handler.updateReservation(contractor, nonExistentReservation, TrustLineAmount(800), SerializedEquivalent(1)),
        NotFoundError);
}

TEST(AmountReservationsHandler, FreeWithEquivalent) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();
    SerializedEquivalent equivalent(2);

    auto reservation = handler.reserve(
        contractor, uuid, TrustLineAmount(500),
        AmountReservation::Outgoing, equivalent);

    EXPECT_NO_THROW(handler.free(contractor, reservation, equivalent));
}

TEST(AmountReservationsHandler, FreeRemovesContractorIfLastReservation) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();

    auto reservation = handler.reserve(
        contractor, uuid, TrustLineAmount(500),
        AmountReservation::Outgoing, SerializedEquivalent(1));

    handler.free(contractor, reservation, SerializedEquivalent(1));

    EXPECT_FALSE(handler.isReservationsPresentWithContractor(contractor));
}

TEST(AmountReservationsHandler, FreeThrowsOnNonExistentContractor) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();

    auto reservation = make_shared<const AmountReservation>(
        uuid, TrustLineAmount(500), AmountReservation::Outgoing, SerializedEquivalent(1));

    EXPECT_THROW(
        handler.free(contractor, reservation, SerializedEquivalent(1)),
        NotFoundError);
}

TEST(AmountReservationsHandler, FreeThrowsOnNonExistentReservation) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid1 = TransactionUUID();
    TransactionUUID uuid2 = TransactionUUID();

    handler.reserve(contractor, uuid1, TrustLineAmount(500),
                   AmountReservation::Outgoing, SerializedEquivalent(1));

    auto nonExistentReservation = make_shared<const AmountReservation>(
        uuid2, TrustLineAmount(500), AmountReservation::Outgoing, SerializedEquivalent(1));

    EXPECT_THROW(
        handler.free(contractor, nonExistentReservation, SerializedEquivalent(1)),
        NotFoundError);
}

TEST(AmountReservationsHandler, TotalReservedFiltersByEquivalent) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid1 = TransactionUUID();
    TransactionUUID uuid2 = TransactionUUID();
    TransactionUUID uuid3 = TransactionUUID();

    handler.reserve(contractor, uuid1, TrustLineAmount(100),
                   AmountReservation::Outgoing, SerializedEquivalent(1));
    handler.reserve(contractor, uuid2, TrustLineAmount(200),
                   AmountReservation::Outgoing, SerializedEquivalent(1));
    handler.reserve(contractor, uuid3, TrustLineAmount(300),
                   AmountReservation::Outgoing, SerializedEquivalent(2));

    auto totalEq1 = handler.totalReserved(
        contractor, AmountReservation::Outgoing, SerializedEquivalent(1));
    auto totalEq2 = handler.totalReserved(
        contractor, AmountReservation::Outgoing, SerializedEquivalent(2));

    EXPECT_EQ(*totalEq1, TrustLineAmount(300)); // 100 + 200
    EXPECT_EQ(*totalEq2, TrustLineAmount(300));
}

TEST(AmountReservationsHandler, TotalReservedFiltersByDirection) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid1 = TransactionUUID();
    TransactionUUID uuid2 = TransactionUUID();

    handler.reserve(contractor, uuid1, TrustLineAmount(100),
                   AmountReservation::Outgoing, SerializedEquivalent(1));
    handler.reserve(contractor, uuid2, TrustLineAmount(200),
                   AmountReservation::Incoming, SerializedEquivalent(1));

    auto totalOutgoing = handler.totalReserved(
        contractor, AmountReservation::Outgoing, SerializedEquivalent(1));
    auto totalIncoming = handler.totalReserved(
        contractor, AmountReservation::Incoming, SerializedEquivalent(1));

    EXPECT_EQ(*totalOutgoing, TrustLineAmount(100));
    EXPECT_EQ(*totalIncoming, TrustLineAmount(200));
}

TEST(AmountReservationsHandler, TotalReservedReturnsZeroForNoReservations) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);

    auto total = handler.totalReserved(
        contractor, AmountReservation::Outgoing, SerializedEquivalent(1));

    EXPECT_EQ(*total, TrustLineAmount(0));
}

TEST(AmountReservationsHandler, GetReservationMatchesByEquivalent) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();
    TrustLineAmount amount(500);
    SerializedEquivalent equivalent(3);

    handler.reserve(contractor, uuid, amount, AmountReservation::Outgoing, equivalent);

    auto retrieved = handler.getReservation(
        contractor, uuid, amount, AmountReservation::Outgoing, equivalent);

    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->transactionUUID(), uuid);
    EXPECT_EQ(retrieved->amount(), amount);
    EXPECT_EQ(retrieved->equivalent(), equivalent);
}

TEST(AmountReservationsHandler, GetReservationThrowsOnDifferentEquivalent) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();
    TrustLineAmount amount(500);

    handler.reserve(contractor, uuid, amount,
                   AmountReservation::Outgoing, SerializedEquivalent(1));

    EXPECT_THROW(
        handler.getReservation(contractor, uuid, amount,
                              AmountReservation::Outgoing, SerializedEquivalent(2)),
        NotFoundError);
}

TEST(AmountReservationsHandler, GetReservationThrowsOnNonExistentContractor) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();

    EXPECT_THROW(
        handler.getReservation(contractor, uuid, TrustLineAmount(500),
                              AmountReservation::Outgoing, SerializedEquivalent(1)),
        NotFoundError);
}

TEST(AmountReservationsHandler, MultipleReservationsSameContractorDifferentEquivalents) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid1 = TransactionUUID();
    TransactionUUID uuid2 = TransactionUUID();

    auto res1 = handler.reserve(contractor, uuid1, TrustLineAmount(100),
                               AmountReservation::Outgoing, SerializedEquivalent(1));
    auto res2 = handler.reserve(contractor, uuid2, TrustLineAmount(100),
                               AmountReservation::Outgoing, SerializedEquivalent(2));

    // Both reservations should exist independently
    EXPECT_NE(res1, res2);
    EXPECT_EQ(res1->equivalent(), SerializedEquivalent(1));
    EXPECT_EQ(res2->equivalent(), SerializedEquivalent(2));

    // Total reserved should be separate
    auto total1 = handler.totalReserved(contractor, AmountReservation::Outgoing, SerializedEquivalent(1));
    auto total2 = handler.totalReserved(contractor, AmountReservation::Outgoing, SerializedEquivalent(2));

    EXPECT_EQ(*total1, TrustLineAmount(100));
    EXPECT_EQ(*total2, TrustLineAmount(100));
}

TEST(AmountReservationsHandler, SameParametersDifferentEquivalentsAreDistinct) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();
    TrustLineAmount amount(500);

    // Reserve same amount, direction, but different equivalents
    auto res1 = handler.reserve(contractor, uuid, amount,
                               AmountReservation::Outgoing, SerializedEquivalent(1));

    // Cannot reserve same UUID again
    TransactionUUID uuid2 = TransactionUUID();
    auto res2 = handler.reserve(contractor, uuid2, amount,
                               AmountReservation::Outgoing, SerializedEquivalent(2));

    EXPECT_NE(res1, res2);
    EXPECT_NE(res1->equivalent(), res2->equivalent());
}

TEST(AmountReservationsHandler, IsReservationsPresentWithContractor) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    ContractorID otherContractor(456);

    EXPECT_FALSE(handler.isReservationsPresentWithContractor(contractor));

    handler.reserve(contractor, TransactionUUID(), TrustLineAmount(100),
                   AmountReservation::Outgoing, SerializedEquivalent(1));

    EXPECT_TRUE(handler.isReservationsPresentWithContractor(contractor));
    EXPECT_FALSE(handler.isReservationsPresentWithContractor(otherContractor));
}

TEST(AmountReservationsHandler, IsTransactionReservationsPresent) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid = TransactionUUID();
    TransactionUUID otherUuid = TransactionUUID();

    EXPECT_FALSE(handler.isTransactionReservationsPresent(uuid));

    handler.reserve(contractor, uuid, TrustLineAmount(100),
                   AmountReservation::Outgoing, SerializedEquivalent(1));

    EXPECT_TRUE(handler.isTransactionReservationsPresent(uuid));
    EXPECT_FALSE(handler.isTransactionReservationsPresent(otherUuid));
}

TEST(AmountReservationsHandler, ContractorReservationsFiltersByDirection) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);

    handler.reserve(contractor, TransactionUUID(), TrustLineAmount(100),
                   AmountReservation::Outgoing, SerializedEquivalent(1));
    handler.reserve(contractor, TransactionUUID(), TrustLineAmount(200),
                   AmountReservation::Incoming, SerializedEquivalent(1));
    handler.reserve(contractor, TransactionUUID(), TrustLineAmount(300),
                   AmountReservation::Outgoing, SerializedEquivalent(2));

    auto outgoing = handler.contractorReservations(contractor, AmountReservation::Outgoing);
    auto incoming = handler.contractorReservations(contractor, AmountReservation::Incoming);

    EXPECT_EQ(outgoing.size(), 2); // Two outgoing (eq 1 and eq 2)
    EXPECT_EQ(incoming.size(), 1); // One incoming
}

TEST(AmountReservationsHandler, TotalReservedWithTransactionUUIDFilter) {
    AmountReservationsHandler handler;
    ContractorID contractor(123);
    TransactionUUID uuid1 = TransactionUUID();
    TransactionUUID uuid2 = TransactionUUID();

    handler.reserve(contractor, uuid1, TrustLineAmount(100),
                   AmountReservation::Outgoing, SerializedEquivalent(1));
    handler.reserve(contractor, uuid2, TrustLineAmount(200),
                   AmountReservation::Outgoing, SerializedEquivalent(1));

    auto totalAll = handler.totalReserved(
        contractor, AmountReservation::Outgoing, SerializedEquivalent(1), nullptr);
    auto totalUuid1 = handler.totalReserved(
        contractor, AmountReservation::Outgoing, SerializedEquivalent(1), &uuid1);

    EXPECT_EQ(*totalAll, TrustLineAmount(300));
    EXPECT_EQ(*totalUuid1, TrustLineAmount(100));
}
