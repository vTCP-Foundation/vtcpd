#include <gtest/gtest.h>
#include <sodium.h>
#include "../../../src/core/crypto/sphincskeys.h"
#include "../../../src/core/crypto/sphincsscheme.h"

using namespace crypto::sphincs;

class SphincsKeysTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize sodium library for each test
        if (sodium_init() < 0) {
            FAIL() << "Failed to initialize sodium library";
        }
    }
};

// ===== PublicKey Tests =====

TEST_F(SphincsKeysTest, PublicKeyDefaultConstructor) {
    PublicKey key;
    EXPECT_FALSE(key.isValid());
    EXPECT_EQ(key.toString(), "");
}

TEST_F(SphincsKeysTest, PublicKeyFromData) {
    byte_t testData[PublicKey::keySize()];
    for (size_t i = 0; i < PublicKey::keySize(); ++i) {
        testData[i] = static_cast<byte_t>(i % 256);
    }
    
    PublicKey key(testData);
    EXPECT_TRUE(key.isValid());
    EXPECT_NE(key.toString(), "");
    
    // Verify data matches
    const byte_t* keyData = key.data();
    for (size_t i = 0; i < PublicKey::keySize(); ++i) {
        EXPECT_EQ(keyData[i], testData[i]);
    }
}

TEST_F(SphincsKeysTest, PublicKeyFromValidHexString) {
    // Create a valid hex string (64 characters for 32 bytes)
    string hexString = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    
    PublicKey key(hexString);
    EXPECT_TRUE(key.isValid());
    EXPECT_EQ(key.toString(), hexString);
}

TEST_F(SphincsKeysTest, PublicKeyFromInvalidHexString) {
    // Test with invalid length
    string invalidString = "short";
    PublicKey key1(invalidString);
    EXPECT_FALSE(key1.isValid());
    
    // Test with invalid characters
    string invalidChars = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeG";
    PublicKey key2(invalidChars);
    EXPECT_FALSE(key2.isValid());
}

TEST_F(SphincsKeysTest, PublicKeyCopyConstructor) {
    byte_t testData[PublicKey::keySize()];
    for (size_t i = 0; i < PublicKey::keySize(); ++i) {
        testData[i] = static_cast<byte_t>(i % 256);
    }
    
    PublicKey original(testData);
    PublicKey copy(original);
    
    EXPECT_TRUE(copy.isValid());
    EXPECT_EQ(original, copy);
    EXPECT_EQ(original.toString(), copy.toString());
}

TEST_F(SphincsKeysTest, PublicKeyAssignmentOperator) {
    byte_t testData[PublicKey::keySize()];
    for (size_t i = 0; i < PublicKey::keySize(); ++i) {
        testData[i] = static_cast<byte_t>(i % 256);
    }
    
    PublicKey original(testData);
    PublicKey assigned;
    
    EXPECT_FALSE(assigned.isValid());
    assigned = original;
    EXPECT_TRUE(assigned.isValid());
    EXPECT_EQ(original, assigned);
}

TEST_F(SphincsKeysTest, PublicKeyEqualityOperators) {
    byte_t testData1[PublicKey::keySize()];
    byte_t testData2[PublicKey::keySize()];
    
    for (size_t i = 0; i < PublicKey::keySize(); ++i) {
        testData1[i] = static_cast<byte_t>(i % 256);
        testData2[i] = static_cast<byte_t>((i + 1) % 256);
    }
    
    PublicKey key1(testData1);
    PublicKey key2(testData1);  // Same data
    PublicKey key3(testData2);  // Different data
    
    EXPECT_EQ(key1, key2);
    EXPECT_NE(key1, key3);
    EXPECT_FALSE(key1 != key2);
    EXPECT_TRUE(key1 != key3);
}

TEST_F(SphincsKeysTest, PublicKeyHashString) {
    byte_t testData[PublicKey::keySize()];
    for (size_t i = 0; i < PublicKey::keySize(); ++i) {
        testData[i] = static_cast<byte_t>(i % 256);
    }
    
    PublicKey key1(testData);
    PublicKey key2(testData);
    
    string hash1 = key1.hashString();
    string hash2 = key2.hashString();
    
    EXPECT_FALSE(hash1.empty());
    EXPECT_EQ(hash1, hash2); // Same key should produce same hash
    EXPECT_EQ(hash1.length(), 64); // SHA256 hash as hex string
}

TEST_F(SphincsKeysTest, PublicKeySerialization) {
    byte_t testData[PublicKey::keySize()];
    for (size_t i = 0; i < PublicKey::keySize(); ++i) {
        testData[i] = static_cast<byte_t>(i % 256);
    }
    
    PublicKey original(testData);
    
    byte_t buffer[PublicKey::keySize()];
    original.serialize(buffer);
    
    PublicKey deserialized;
    deserialized.deserialize(buffer);
    
    EXPECT_TRUE(deserialized.isValid());
    EXPECT_EQ(original, deserialized);
}

// ===== PrivateKey Tests =====

TEST_F(SphincsKeysTest, PrivateKeyRandomGeneration) {
    PrivateKey key1;
    PrivateKey key2;
    
    EXPECT_TRUE(key1.isValid());
    EXPECT_TRUE(key2.isValid());
    
    // Two random keys should be different
    auto pubKey1 = key1.derivePublicKey();
    auto pubKey2 = key2.derivePublicKey();
    
    EXPECT_NE(*pubKey1, *pubKey2);
}

TEST_F(SphincsKeysTest, PrivateKeyDeterministicGeneration) {
    string seed = "test_seed_for_deterministic_key_generation";
    
    PrivateKey key1(seed);
    PrivateKey key2(seed);
    
    EXPECT_TRUE(key1.isValid());
    EXPECT_TRUE(key2.isValid());
    
    // Same seed should produce same keys
    auto pubKey1 = key1.derivePublicKey();
    auto pubKey2 = key2.derivePublicKey();
    
    EXPECT_EQ(*pubKey1, *pubKey2);
}

TEST_F(SphincsKeysTest, PrivateKeyFromData) {
    byte_t keyData[PrivateKey::privateKeySize()];
    for (size_t i = 0; i < PrivateKey::privateKeySize(); ++i) {
        keyData[i] = static_cast<byte_t>(i % 256);
    }
    
    PrivateKey key(keyData);
    EXPECT_TRUE(key.isValid());
    
    auto pubKey = key.derivePublicKey();
    EXPECT_TRUE(pubKey->isValid());
}

TEST_F(SphincsKeysTest, PrivateKeyPublicKeyDerivation) {
    PrivateKey privateKey;
    
    auto publicKey = privateKey.derivePublicKey();
    ASSERT_NE(publicKey, nullptr);
    EXPECT_TRUE(publicKey->isValid());
    
    // Derive again and check consistency
    auto publicKey2 = privateKey.derivePublicKey();
    ASSERT_NE(publicKey2, nullptr);
    EXPECT_EQ(*publicKey, *publicKey2);
}

TEST_F(SphincsKeysTest, PrivateKeySerialization) {
    PrivateKey original;
    
    auto serialized = original.serialize();
    EXPECT_EQ(serialized.size(), PrivateKey::privateKeySize());
    
    PrivateKey deserialized;
    deserialized.deserialize(serialized);
    
    EXPECT_TRUE(deserialized.isValid());
    
    // Check that they derive the same public key
    auto pubKey1 = original.derivePublicKey();
    auto pubKey2 = deserialized.derivePublicKey();
    
    EXPECT_EQ(*pubKey1, *pubKey2);
}

TEST_F(SphincsKeysTest, PrivateKeyInvalidData) {
    PrivateKey key(nullptr);
    EXPECT_FALSE(key.isValid());
    
    auto pubKey = key.derivePublicKey();
    EXPECT_EQ(pubKey, nullptr);
}

// ===== Key Size Tests =====

TEST_F(SphincsKeysTest, KeySizeConstants) {
    EXPECT_EQ(PublicKey::keySize(), 64);  // SPHINCS+ SLH-DSA-SHA2-256s public key size
    EXPECT_EQ(PrivateKey::privateKeySize(), 128); // SPHINCS+ SLH-DSA-SHA2-256s private key size
    EXPECT_GT(PublicKey::keySize(), 0);
    EXPECT_GT(PrivateKey::privateKeySize(), 0);
}

// ===== Integration Tests =====

TEST_F(SphincsKeysTest, KeyPairIntegration) {
    // Test full key pair generation and usage
    auto keyPair = util::generateKeyPair();
    
    ASSERT_NE(keyPair.first, nullptr);
    ASSERT_NE(keyPair.second, nullptr);
    EXPECT_TRUE(keyPair.first->isValid());
    EXPECT_TRUE(keyPair.second->isValid());
    
    // Verify that the public key matches what the private key derives
    auto derivedPubKey = keyPair.first->derivePublicKey();
    EXPECT_EQ(*keyPair.second, *derivedPubKey);
}

TEST_F(SphincsKeysTest, DeterministicKeyPairIntegration) {
    string seed = "integration_test_seed";
    
    auto keyPair1 = util::generateKeyPairFromSeed(seed);
    auto keyPair2 = util::generateKeyPairFromSeed(seed);
    
    ASSERT_NE(keyPair1.first, nullptr);
    ASSERT_NE(keyPair1.second, nullptr);
    ASSERT_NE(keyPair2.first, nullptr);
    ASSERT_NE(keyPair2.second, nullptr);
    
    // Public keys should be identical
    EXPECT_EQ(*keyPair1.second, *keyPair2.second);
    
    // Both should derive the same public key from private key
    auto derived1 = keyPair1.first->derivePublicKey();
    auto derived2 = keyPair2.first->derivePublicKey();
    EXPECT_EQ(*derived1, *derived2);
    EXPECT_EQ(*keyPair1.second, *derived1);
}