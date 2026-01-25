#ifndef VTCPD_CONTRACTOR_H
#define VTCPD_CONTRACTOR_H

#include "../common/multiprecision/MultiprecisionUtils.h"
#include "../crypto/MsgEncryptor.h"
#include "../crypto/sphincskeys.h"

namespace sphincs = crypto::sphincs;

class Contractor
{
public:
    typedef shared_ptr<Contractor> Shared;

public:
    Contractor(
        ContractorID id,
        vector<BaseAddress::Shared> &addresses,
        const MsgEncryptor::KeyTrio::Shared &cryptoKey);

    Contractor(
        ContractorID id,
        ContractorID idOnContractorSide,
        const MsgEncryptor::KeyTrio::Shared &cryptoKey,
        bool isConfirmed);

    Contractor(
        vector<BaseAddress::Shared> addresses);

    Contractor(
        byte_t* buffer);

    const ContractorID getID() const;

    const ContractorID ownIdOnContractorSide() const;

    void setOwnIdOnContractorSide(ContractorID id);

    MsgEncryptor::KeyTrio::Shared cryptoKey();

    void setCryptoKey(
        MsgEncryptor::KeyTrio::Shared cryptoKey);

    /**
     * Returns stored payment public key if available.
     */
    sphincs::PublicKey::Shared paymentPublicKey() const;

    /**
     * Updates payment public key used for receipts.
     */
    void setPaymentPublicKey(sphincs::PublicKey::Shared key);

    const bool isConfirmed() const;

    void confirm();

    vector<BaseAddress::Shared> addresses() const;

    void setAddresses(
        vector<BaseAddress::Shared> &addresses);

    BaseAddress::Shared mainAddress() const;

    bool containsAddresses(
        vector<BaseAddress::Shared> &addresses) const;

    bool containsAtLeastOneAddress(
        vector<BaseAddress::Shared> addresses) const;

    BytesShared serializeToBytes() const;

    size_t serializedSize() const;

    friend bool operator==(
        Contractor::Shared contractor1,
        Contractor::Shared contractor2);

    friend bool operator!=(
        Contractor::Shared contractor1,
        Contractor::Shared contractor2);

    string toString() const;

    string outputString() const;

private:
    ContractorID mID;
    ContractorID mOwnIdOnContractorSide;
    MsgEncryptor::KeyTrio::Shared mCryptoKey;
    // Payment public key for receipt signing/verification; nullable for legacy channels.
    sphincs::PublicKey::Shared mPaymentPublicKey;
    bool mIsConfirmed;
    vector<BaseAddress::Shared> mAddresses;
};

#endif // VTCPD_CONTRACTOR_H
