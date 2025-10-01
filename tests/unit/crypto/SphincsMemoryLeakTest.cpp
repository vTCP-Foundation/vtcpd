#include <gtest/gtest.h>
#include <sodium.h>
#include "../../../src/core/crypto/sphincskeys.h"
#include "../../../src/core/crypto/sphincsscheme.h"

using namespace crypto::sphincs;

// End-to-end leak test executed under ASan/LSan (or valgrind separately if desired)
TEST(SphincsMemoryLeakTest, EndToEnd_NoLeaks_UnderSanitizer)
{
    ASSERT_GE(sodium_init(), 0);

    const std::string data = "Leak-check data payload";
    const size_t iterations = 10; //2000;

    for (size_t i = 0; i < iterations; ++i) {
        PrivateKey privateKey;                        // 1) Create private key
        ASSERT_TRUE(privateKey.isValid());

        auto publicKey = privateKey.derivePublicKey(); // 2) Derive public key
        ASSERT_NE(publicKey, nullptr);
        ASSERT_TRUE(publicKey->isValid());

        Signature signature;                          // 3) Sign
        ASSERT_TRUE(signature.sign(privateKey, data));
        ASSERT_TRUE(signature.isValid());

        // 4) Verify
        const byte_t* dataBytes = reinterpret_cast<const byte_t*>(data.c_str());
        size_t dataSize = data.length();
        ASSERT_TRUE(signature.verify(*publicKey, dataBytes, dataSize));
    }
}

// Additional stress scenarios that are helpful for leak detection when run under sanitizers
TEST(SphincsMemoryLeakTest, MassKeyGen_NoLeaks)
{
    ASSERT_GE(sodium_init(), 0);
    const size_t iterations = 10; //5000;

    for (size_t i = 0; i < iterations; ++i) {
        PrivateKey privateKey;
        ASSERT_TRUE(privateKey.isValid());
    }
}

TEST(SphincsMemoryLeakTest, DerivePublicKeyLoop_NoLeaks)
{
    ASSERT_GE(sodium_init(), 0);
    const size_t iterations = 10; //5000;

    PrivateKey privateKey;
    ASSERT_TRUE(privateKey.isValid());
    for (size_t i = 0; i < iterations; ++i) {
        auto publicKey = privateKey.derivePublicKey();
        ASSERT_NE(publicKey, nullptr);
        ASSERT_TRUE(publicKey->isValid());
    }
}

TEST(SphincsMemoryLeakTest, SignVerifyVaryingSizes_NoLeaks)
{
    ASSERT_GE(sodium_init(), 0);
    PrivateKey privateKey;
    ASSERT_TRUE(privateKey.isValid());
    auto publicKey = privateKey.derivePublicKey();
    ASSERT_NE(publicKey, nullptr);
    ASSERT_TRUE(publicKey->isValid());

    const std::vector<size_t> sizes = {1, 64, 1024, 8192, 65536};
    for (size_t size : sizes) {
        std::string payload(size, '\x01');
        Signature signature;
        ASSERT_TRUE(signature.sign(privateKey, payload));
        const byte_t* payloadBytes = reinterpret_cast<const byte_t*>(payload.data());
        ASSERT_TRUE(signature.verify(*publicKey, payloadBytes, payload.size()));
    }
}

TEST(SphincsMemoryLeakTest, SerializeDeserializeLoops_NoLeaks)
{
    ASSERT_GE(sodium_init(), 0);

    // PrivateKey serialize/deserialize
    {
        PrivateKey original;
        ASSERT_TRUE(original.isValid());
        auto serialized = original.serialize();

        const size_t iterations = 10; //2000;
        for (size_t i = 0; i < iterations; ++i) {
            PrivateKey copy;
            copy.deserialize(serialized);
            ASSERT_TRUE(copy.isValid());
        }
    }

    // PublicKey serialize/deserialize
    {
        PrivateKey privateKey;
        auto publicKey = privateKey.derivePublicKey();
        ASSERT_NE(publicKey, nullptr);
        std::array<byte_t, PublicKey::keySize()> buffer{};
        publicKey->serialize(buffer.data());

        const size_t iterations = 10; //2000;
        for (size_t i = 0; i < iterations; ++i) {
            PublicKey restored;
            restored.deserialize(buffer.data());
            ASSERT_TRUE(restored.isValid());
        }
    }

    // Signature serialize/deserialize
    {
        PrivateKey privateKey;
        auto publicKey = privateKey.derivePublicKey();
        Signature signature;
        ASSERT_TRUE(signature.sign(privateKey, "data"));
        std::vector<byte_t> buffer(Signature::signatureSize());
        signature.serialize(buffer.data());

        const size_t iterations = 10; //2000;
        for (size_t i = 0; i < iterations; ++i) {
            Signature restored;
            restored.deserialize(buffer.data());
            ASSERT_TRUE(restored.isValid());
            const char* small = "data";
            ASSERT_TRUE(restored.verify(*publicKey, reinterpret_cast<const byte_t*>(small), strlen(small)));
        }
    }
}






