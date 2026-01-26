#include <gtest/gtest.h>

#include "core/contractors/Contractor.h"
#include "core/crypto/MsgEncryptor.h"

namespace {
const ContractorID kTestContractorId = 1;
const size_t kPaymentKeySize = crypto::sphincs::PublicKey::keySize();

Contractor::Shared createTestContractor()
{
    auto keys = MsgEncryptor::generateKeyTrio();
    return std::make_shared<Contractor>(kTestContractorId, 0, keys, false);
}

sphincs::PublicKey::Shared createPaymentPublicKey(byte_t fill)
{
    std::vector<byte_t> keyBytes(kPaymentKeySize, fill);
    return std::make_shared<sphincs::PublicKey>(keyBytes.data());
}
} // namespace

TEST(ContractorTest, PaymentPublicKey_DefaultsToNull)
{
    auto contractor = createTestContractor();
    EXPECT_EQ(contractor->paymentPublicKey(), nullptr);
}

TEST(ContractorTest, PaymentPublicKey_SetGet_RoundTrip)
{
    auto contractor = createTestContractor();
    auto paymentKey = createPaymentPublicKey(0x5A);

    contractor->setPaymentPublicKey(paymentKey);

    ASSERT_NE(contractor->paymentPublicKey(), nullptr);
    EXPECT_EQ(*contractor->paymentPublicKey(), *paymentKey);
}

TEST(ContractorTest, PaymentPublicKey_SetToNull_ClearsValue)
{
    auto contractor = createTestContractor();
    contractor->setPaymentPublicKey(createPaymentPublicKey(0x7F));

    contractor->setPaymentPublicKey(nullptr);

    EXPECT_EQ(contractor->paymentPublicKey(), nullptr);
}
