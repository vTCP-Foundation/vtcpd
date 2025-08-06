#include <gtest/gtest.h>
#include <sodium.h>
#include "../../../src/core/crypto/sphincskeys.h"
#include "../../../src/core/crypto/sphincsscheme.h"

using namespace crypto::sphincs;

class SphincsSignatureTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize sodium library for each test
        if (sodium_init() < 0) {
            FAIL() << "Failed to initialize sodium library";
        }
        
        // Generate a test key pair for most tests
        testKeyPair = util::generateKeyPair();
        ASSERT_NE(testKeyPair.first, nullptr);
        ASSERT_NE(testKeyPair.second, nullptr);
        ASSERT_TRUE(testKeyPair.first->isValid());
        ASSERT_TRUE(testKeyPair.second->isValid());
    }
    
    pair<PrivateKey::Shared, PublicKey::Shared> testKeyPair;
    const string testData = "This is test data for SPHINCS+ signature testing";
};

// ===== Signature Basic Tests =====

TEST_F(SphincsSignatureTest, DefaultConstructor) {
    Signature sig;
    EXPECT_FALSE(sig.isValid());
    EXPECT_EQ(sig.toString(), "");
}

TEST_F(SphincsSignatureTest, SignatureFromData) {
    // Create test signature data
    byte_t testSigData[Signature::signatureSize()];
    for (size_t i = 0; i < Signature::signatureSize(); ++i) {
        testSigData[i] = static_cast<byte_t>(i % 256);
    }
    
    Signature sig(testSigData);
    EXPECT_TRUE(sig.isValid());
    EXPECT_NE(sig.toString(), "");
    
    // Verify data matches
    const byte_t* sigData = sig.data();
    for (size_t i = 0; i < Signature::signatureSize(); ++i) {
        EXPECT_EQ(sigData[i], testSigData[i]);
    }
}

TEST_F(SphincsSignatureTest, SignatureFromValidHexString) {
    // Create a valid hex string (59584 characters for 29792 bytes)
    string hexString;
    for (int i = 0; i < 29792; ++i) {
        hexString += "ab"; // Each byte as hex (2 chars)
    }
    
    Signature sig(hexString);
    EXPECT_TRUE(sig.isValid());
    EXPECT_EQ(sig.toString(), hexString);
}

TEST_F(SphincsSignatureTest, SignatureFromInvalidHexString) {
    // Test with invalid length
    string invalidString = "short";
    Signature sig1(invalidString);
    EXPECT_FALSE(sig1.isValid());
    
    // Test with invalid characters (create string of correct length but with invalid char)
    string invalidChars;
    for (int i = 0; i < 29792; ++i) {
        invalidChars += (i == 100) ? "aG" : "ab"; // Invalid 'G' character at position 100
    }
    Signature sig2(invalidChars);
    EXPECT_FALSE(sig2.isValid());
}

TEST_F(SphincsSignatureTest, SignatureCopyConstructor) {
    byte_t testSigData[Signature::signatureSize()];
    for (size_t i = 0; i < Signature::signatureSize(); ++i) {
        testSigData[i] = static_cast<byte_t>(i % 256);
    }
    
    Signature original(testSigData);
    Signature copy(original);
    
    EXPECT_TRUE(copy.isValid());
    EXPECT_EQ(original, copy);
    EXPECT_EQ(original.toString(), copy.toString());
}

TEST_F(SphincsSignatureTest, SignatureAssignmentOperator) {
    byte_t testSigData[Signature::signatureSize()];
    for (size_t i = 0; i < Signature::signatureSize(); ++i) {
        testSigData[i] = static_cast<byte_t>(i % 256);
    }
    
    Signature original(testSigData);
    Signature assigned;
    
    EXPECT_FALSE(assigned.isValid());
    assigned = original;
    EXPECT_TRUE(assigned.isValid());
    EXPECT_EQ(original, assigned);
}

TEST_F(SphincsSignatureTest, SignatureEqualityOperators) {
    byte_t testSigData1[Signature::signatureSize()];
    byte_t testSigData2[Signature::signatureSize()];
    
    for (size_t i = 0; i < Signature::signatureSize(); ++i) {
        testSigData1[i] = static_cast<byte_t>(i % 256);
        testSigData2[i] = static_cast<byte_t>((i + 1) % 256);
    }
    
    Signature sig1(testSigData1);
    Signature sig2(testSigData1);  // Same data
    Signature sig3(testSigData2);  // Different data
    
    EXPECT_EQ(sig1, sig2);
    EXPECT_NE(sig1, sig3);
    EXPECT_FALSE(sig1 != sig2);
    EXPECT_TRUE(sig1 != sig3);
}

// ===== Signing Tests =====

TEST_F(SphincsSignatureTest, BasicSigning) {
    Signature sig;
    bool result = sig.sign(*testKeyPair.first, testData);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(sig.isValid());
    EXPECT_NE(sig.toString(), "");
}

TEST_F(SphincsSignatureTest, SigningWithByteArray) {
    const byte_t* dataBytes = reinterpret_cast<const byte_t*>(testData.c_str());
    size_t dataSize = testData.length();
    
    Signature sig;
    bool result = sig.sign(*testKeyPair.first, dataBytes, dataSize);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(sig.isValid());
}

TEST_F(SphincsSignatureTest, SigningWithInvalidKey) {
    PrivateKey invalidKey(nullptr);
    
    Signature sig;
    bool result = sig.sign(invalidKey, testData);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(sig.isValid());
}

TEST_F(SphincsSignatureTest, SigningWithEmptyData) {
    Signature sig;
    bool result = sig.sign(*testKeyPair.first, "");
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(sig.isValid());
}

TEST_F(SphincsSignatureTest, SigningWithNullData) {
    Signature sig;
    bool result = sig.sign(*testKeyPair.first, nullptr, 0);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(sig.isValid());
}

// ===== Verification Tests =====

TEST_F(SphincsSignatureTest, BasicVerification) {
    Signature sig;
    ASSERT_TRUE(sig.sign(*testKeyPair.first, testData));
    
    bool result = sig.verify(*testKeyPair.second, testData);
    EXPECT_TRUE(result);
}

TEST_F(SphincsSignatureTest, VerificationWithByteArray) {
    const byte_t* dataBytes = reinterpret_cast<const byte_t*>(testData.c_str());
    size_t dataSize = testData.length();
    
    Signature sig;
    ASSERT_TRUE(sig.sign(*testKeyPair.first, dataBytes, dataSize));
    
    bool result = sig.verify(*testKeyPair.second, dataBytes, dataSize);
    EXPECT_TRUE(result);
}

TEST_F(SphincsSignatureTest, VerificationWithWrongKey) {
    auto wrongKeyPair = util::generateKeyPair();
    ASSERT_NE(wrongKeyPair.second, nullptr);
    
    Signature sig;
    ASSERT_TRUE(sig.sign(*testKeyPair.first, testData));
    
    bool result = sig.verify(*wrongKeyPair.second, testData);
    EXPECT_FALSE(result);
}

TEST_F(SphincsSignatureTest, VerificationWithTamperedData) {
    Signature sig;
    ASSERT_TRUE(sig.sign(*testKeyPair.first, testData));
    
    string tamperedData = testData + " TAMPERED";
    bool result = sig.verify(*testKeyPair.second, tamperedData);
    EXPECT_FALSE(result);
}

TEST_F(SphincsSignatureTest, VerificationWithInvalidSignature) {
    Signature invalidSig;
    
    bool result = invalidSig.verify(*testKeyPair.second, testData);
    EXPECT_FALSE(result);
}

TEST_F(SphincsSignatureTest, VerificationWithInvalidKey) {
    PublicKey invalidKey;
    
    Signature sig;
    ASSERT_TRUE(sig.sign(*testKeyPair.first, testData));
    
    bool result = sig.verify(invalidKey, testData);
    EXPECT_FALSE(result);
}

// ===== Deterministic Behavior Tests =====

TEST_F(SphincsSignatureTest, DeterministicSigning) {
    string seed = "deterministic_test_seed";
    auto keyPair1 = util::generateKeyPairFromSeed(seed);
    auto keyPair2 = util::generateKeyPairFromSeed(seed);
    
    Signature sig1, sig2;
    ASSERT_TRUE(sig1.sign(*keyPair1.first, testData));
    ASSERT_TRUE(sig2.sign(*keyPair2.first, testData));
    
    // Signatures should be identical (deterministic behavior)
    EXPECT_EQ(sig1, sig2);
    EXPECT_EQ(sig1.toString(), sig2.toString());
}

TEST_F(SphincsSignatureTest, ConsistentSigning) {
    Signature sig1, sig2;
    ASSERT_TRUE(sig1.sign(*testKeyPair.first, testData));
    ASSERT_TRUE(sig2.sign(*testKeyPair.first, testData));
    
    // Same key and data should produce same signature
    EXPECT_EQ(sig1, sig2);
}

TEST_F(SphincsSignatureTest, DifferentDataProducesDifferentSignatures) {
    string data1 = "First test data";
    string data2 = "Second test data";
    
    Signature sig1, sig2;
    ASSERT_TRUE(sig1.sign(*testKeyPair.first, data1));
    ASSERT_TRUE(sig2.sign(*testKeyPair.first, data2));
    
    // Different data should produce different signatures
    EXPECT_NE(sig1, sig2);
}

// ===== Serialization Tests =====

TEST_F(SphincsSignatureTest, SignatureSerialization) {
    Signature original;
    ASSERT_TRUE(original.sign(*testKeyPair.first, testData));
    
    byte_t buffer[Signature::signatureSize()];
    original.serialize(buffer);
    
    Signature deserialized;
    deserialized.deserialize(buffer);
    
    EXPECT_TRUE(deserialized.isValid());
    EXPECT_EQ(original, deserialized);
    
    // Verify that deserialized signature still works
    bool result = deserialized.verify(*testKeyPair.second, testData);
    EXPECT_TRUE(result);
}

TEST_F(SphincsSignatureTest, SerializationWithNullBuffer) {
    Signature sig;
    ASSERT_TRUE(sig.sign(*testKeyPair.first, testData));
    
    EXPECT_THROW(sig.serialize(nullptr), std::invalid_argument);
    EXPECT_THROW(sig.deserialize(nullptr), std::invalid_argument);
}

// ===== Clear Tests =====

TEST_F(SphincsSignatureTest, SignatureClear) {
    Signature sig;
    ASSERT_TRUE(sig.sign(*testKeyPair.first, testData));
    ASSERT_TRUE(sig.isValid());
    
    sig.clear();
    EXPECT_FALSE(sig.isValid());
    EXPECT_EQ(sig.toString(), "");
}

// ===== Size Tests =====

TEST_F(SphincsSignatureTest, SignatureSize) {
    EXPECT_EQ(Signature::signatureSize(), 29792);  // SPHINCS+ SLH-DSA-SHA2-256s signature size
    EXPECT_GT(Signature::signatureSize(), 0);
}

// ===== Utility Function Tests =====

TEST_F(SphincsSignatureTest, UtilSignData) {
    auto signature = util::signData(*testKeyPair.first, testData);
    
    ASSERT_NE(signature, nullptr);
    EXPECT_TRUE(signature->isValid());
    
    bool result = util::verifySignature(*testKeyPair.second, *signature, testData);
    EXPECT_TRUE(result);
}

TEST_F(SphincsSignatureTest, UtilSignDataWithByteArray) {
    const byte_t* dataBytes = reinterpret_cast<const byte_t*>(testData.c_str());
    size_t dataSize = testData.length();
    
    auto signature = util::signData(*testKeyPair.first, dataBytes, dataSize);
    
    ASSERT_NE(signature, nullptr);
    EXPECT_TRUE(signature->isValid());
    
    bool result = util::verifySignature(*testKeyPair.second, *signature, dataBytes, dataSize);
    EXPECT_TRUE(result);
}

TEST_F(SphincsSignatureTest, UtilVerifySignature) {
    auto signature = util::signData(*testKeyPair.first, testData);
    ASSERT_NE(signature, nullptr);
    
    bool result = util::verifySignature(*testKeyPair.second, *signature, testData);
    EXPECT_TRUE(result);
    
    // Test with wrong key
    auto wrongKeyPair = util::generateKeyPair();
    bool wrongResult = util::verifySignature(*wrongKeyPair.second, *signature, testData);
    EXPECT_FALSE(wrongResult);
}

TEST_F(SphincsSignatureTest, UtilWithInvalidKey) {
    PrivateKey invalidKey(nullptr);
    
    auto signature = util::signData(invalidKey, testData);
    EXPECT_EQ(signature, nullptr);
}