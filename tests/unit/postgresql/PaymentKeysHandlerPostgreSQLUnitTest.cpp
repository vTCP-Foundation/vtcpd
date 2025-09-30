#include "gtest/gtest.h"
#include <vector>
#include <cstdlib>
#include <cstring>

// Unit test for hex conversion logic used in PaymentKeysHandlerPostgreSQL
class PaymentKeysHandlerPostgreSQLUnitTest : public ::testing::Test {
protected:
    // Helper function that mimics the hex conversion logic from PaymentKeysHandlerPostgreSQL
    static std::vector<unsigned char> convertHexToBytes(const unsigned char* rawData, int dataLength, size_t expectedSize) {
        // Check if data is in hex format (starts with \x and has 2*keySize + 2 length)
        if (dataLength == static_cast<int>(expectedSize * 2 + 2) && 
            rawData[0] == '\\' && rawData[1] == 'x') {
            // Convert from hex format
            std::vector<unsigned char> binaryData(expectedSize);
            for (size_t i = 0; i < expectedSize; ++i) {
                char hex[3] = {static_cast<char>(rawData[2 + i*2]), static_cast<char>(rawData[3 + i*2]), '\0'};
                binaryData[i] = static_cast<unsigned char>(strtoul(hex, nullptr, 16));
            }
            return binaryData;
        } else if (dataLength == static_cast<int>(expectedSize)) {
            // Data is already in binary format
            std::vector<unsigned char> binaryData(expectedSize);
            memcpy(binaryData.data(), rawData, expectedSize);
            return binaryData;
        } else {
            throw std::runtime_error("Invalid data length");
        }
    }
};

// Test hex conversion for PublicKey size (64 bytes)
TEST_F(PaymentKeysHandlerPostgreSQLUnitTest, HexConversion_PublicKeySize_ConvertsCorrectly) {
    const std::string hexData = "\\x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"
                               "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
    const unsigned char* rawData = reinterpret_cast<const unsigned char*>(hexData.c_str());
    int dataLength = static_cast<int>(hexData.length());
    const size_t publicKeySize = 64;
    
    EXPECT_EQ(dataLength, 130); // 64 * 2 + 2 for \x
    
    auto result = convertHexToBytes(rawData, dataLength, publicKeySize);
    
    EXPECT_EQ(result.size(), publicKeySize);
    EXPECT_EQ(result[0], 0x12);
    EXPECT_EQ(result[1], 0x34);
    EXPECT_EQ(result[2], 0x56);
    EXPECT_EQ(result[3], 0x78);
    EXPECT_EQ(result[4], 0x90);
    EXPECT_EQ(result[5], 0xab);
    EXPECT_EQ(result[6], 0xcd);
    EXPECT_EQ(result[7], 0xef);
}

// Test hex conversion for PrivateKey size (128 bytes)
TEST_F(PaymentKeysHandlerPostgreSQLUnitTest, HexConversion_PrivateKeySize_ConvertsCorrectly) {
    // Create 128-byte hex string (256 hex chars + \x)
    std::string hexData = "\\x";
    for (int i = 0; i < 128; ++i) {
        char hex[3];
        sprintf(hex, "%02x", i % 256);
        hexData += hex;
    }
    
    const unsigned char* rawData = reinterpret_cast<const unsigned char*>(hexData.c_str());
    int dataLength = static_cast<int>(hexData.length());
    const size_t privateKeySize = 128;
    
    EXPECT_EQ(dataLength, 258); // 128 * 2 + 2 for \x
    
    auto result = convertHexToBytes(rawData, dataLength, privateKeySize);
    
    EXPECT_EQ(result.size(), privateKeySize);
    // Check first few bytes match the pattern
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(result[i], i % 256);
    }
}

// Test binary data pass-through (no conversion needed)
TEST_F(PaymentKeysHandlerPostgreSQLUnitTest, BinaryData_PublicKeySize_PassedThrough) {
    std::vector<unsigned char> binaryData(64);
    for (int i = 0; i < 64; ++i) {
        binaryData[i] = static_cast<unsigned char>(i);
    }
    
    auto result = convertHexToBytes(binaryData.data(), 64, 64);
    
    EXPECT_EQ(result.size(), 64);
    for (int i = 0; i < 64; ++i) {
        EXPECT_EQ(result[i], i);
    }
}

// Test invalid data length throws error
TEST_F(PaymentKeysHandlerPostgreSQLUnitTest, InvalidDataLength_ThrowsError) {
    const std::string invalidData = "\\xinvalidlength";
    const unsigned char* rawData = reinterpret_cast<const unsigned char*>(invalidData.c_str());
    int dataLength = static_cast<int>(invalidData.length());
    
    EXPECT_THROW(convertHexToBytes(rawData, dataLength, 64), std::runtime_error);
}