#include "lamportkeys.h"
#include <cstring>
#include <sstream>
#include <iomanip>

namespace crypto {
namespace lamport {


PrivateKey::PrivateKey() : mData(memory::SecureSegment(kRandomNumbersSlotsCount * kRandomNumberSlotSize)),
    mIsCropped(false)
{
    auto guard = mData.unlockAndInitGuard();

    auto offset = static_cast<byte_t*>(guard.address());
    for (size_t i = 0; i < kRandomNumbersSlotsCount; ++i) {
        randombytes_buf(offset, kRandomNumberSlotSize);
        offset += kRandomNumberSlotSize;
    }
}

PrivateKey::PrivateKey(
    const string& seedString) : mData(memory::SecureSegment(kRandomNumbersSlotsCount * kRandomNumberSlotSize)),
    mIsCropped(false)
{
    // Convert string to 32-byte seed
    byte_t seed[randombytes_SEEDBYTES];
    crypto_generichash(seed, randombytes_SEEDBYTES, 
                       reinterpret_cast<const byte_t*>(seedString.c_str()), 
                       seedString.length(), nullptr, 0);

    auto guard = mData.unlockAndInitGuard();

    auto offset = static_cast<byte_t*>(guard.address());
    for (size_t i = 0; i < kRandomNumbersSlotsCount; ++i) {
        // Create a modified seed for each slot by hashing the original seed with the slot index
        byte_t modifiedSeed[randombytes_SEEDBYTES];
        crypto_generichash(modifiedSeed, randombytes_SEEDBYTES, seed, randombytes_SEEDBYTES, 
                          reinterpret_cast<const byte_t*>(&i), sizeof(i));
        
        randombytes_buf_deterministic(offset, kRandomNumberSlotSize, modifiedSeed);
        offset += kRandomNumberSlotSize;
    }
}

PrivateKey::PrivateKey(
    byte_t* data) : mData(memory::SecureSegment(keySize())),
    mIsCropped(false)
{
    auto guard = mData.unlockAndInitGuard();
    auto offset = static_cast<byte_t*>(guard.address());
    memcpy(
        offset,
        data,
        keySize());
}

PublicKey::Shared PrivateKey::derivePublicKey()
{

    auto guard = mData.unlockAndInitGuard();
    auto generatedKey = make_shared<PublicKey>();

    // Numbers buffers memory allocation.
    generatedKey->mData = static_cast<byte_t*>(malloc(kRandomNumbersSlotsCount * kRandomNumberSlotSize));
    if (generatedKey->mData == nullptr) {
        return nullptr;
    }

    // Numbers buffers initialisation via hashing private key numbers.
    auto source = static_cast<byte_t*>(guard.address());
    auto destination = static_cast<byte_t*>(generatedKey->mData);

    for (size_t i = 0; i < kRandomNumbersSlotsCount; ++i) {
        crypto_generichash(destination, kRandomNumberSlotSize, source, kRandomNumberSlotSize, nullptr, 0);
        source += kRandomNumberSlotSize;
        destination += kRandomNumberSlotSize;
    }

    return generatedKey;
}

const memory::SecureSegment *PrivateKey::data() const
{
    return &mData;
}

const string PrivateKey::toString() const
{
    // Generate hash of private key for logging
    auto guard = mData.unlockAndInitGuard();
    
    byte_t keyHash[32];
    crypto_generichash(
        keyHash,
        32,
        static_cast<byte_t*>(guard.address()),
        keySize(),
        nullptr,
        0);
    
    stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; i++) {
        ss << std::setfill('0') << std::setw(2) << (int)keyHash[i];
    }
    return ss.str();
}

PublicKey::PublicKey(
    byte_t* data)
{
    mData = static_cast<byte_t*>(
                malloc(
                    keySize()));
    memcpy(
        mData,
        data,
        keySize());
}

PublicKey::~PublicKey() noexcept
{
    if (mData != nullptr) {
        free(mData);
        mData = nullptr;
    }
}

const byte_t* PublicKey::data() const
{
    return mData;
}

const KeyHash::Shared PublicKey::hash() const
{
    auto keyHashBuffer = (byte_t*)malloc(KeyHash::kBytesSize);
    crypto_generichash(
        keyHashBuffer,
        KeyHash::kBytesSize,
        mData,
        keySize(),
        nullptr,
        0);
    auto result = make_shared<KeyHash>(keyHashBuffer);

    // KeyHash constructor copies the buffer into internal memory,
    // so the original buffer must be freed.
    free(keyHashBuffer);
    return result;
}

KeyHash::KeyHash(
    byte_t* buffer)
{
    memcpy(
        mData,
        buffer,
        kBytesSize);
}

const byte_t* KeyHash::data() const
{
    return mData;
}

const string KeyHash::toString() const
{
    stringstream ss;
    ss << std::hex;
    for (byte_t i : mData)
        ss << (int)i;
    return ss.str();
}

bool operator==(const KeyHash &kh1, const KeyHash &kh2)
{
    return memcmp(kh1.mData, kh2.mData, KeyHash::kBytesSize) == 0;
}

bool operator!=(const KeyHash &kh1, const KeyHash &kh2)
{
    return !(kh1 == kh2);
}

}
}
