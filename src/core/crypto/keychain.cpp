#include "keychain.h"

namespace crypto {

Encryptor::Encryptor(memory::SecureSegment& key) noexcept
    :

    mKey(key)
{}

pair<BytesShared, size_t> Encryptor::encrypt(byte_t* data, size_t len)
{
    mKey.unlockAndInitGuard();
    // ...
    return make_pair(nullptr, 0);
}

pair<BytesShared, size_t> Encryptor::decrypt(byte_t* data, size_t len)
{
    mKey.unlockAndInitGuard();
    // ...
    return make_pair(nullptr, 0);
}

Keystore::Keystore(
    // todo memory::SecureSegment
    // &memoryKey,
    Logger& logger) noexcept
    :

    // todo
    // mEncryptor(Encryptor(memoryKey)),
    mLogger(logger)
{}

int Keystore::init(IOTransaction::Shared ioTransaction)
{
    int rc = sodium_init();
    // Ensure at least one payment key exists using provided transaction
    try {
        ensurePaymentKeyExists(ioTransaction);
    } catch (const exception &e) {
        warning() << string("Keystore::init key ensure failed: ") + e.what();
    }
    return rc;
}

TrustLineKeychain Keystore::keychain(const TrustLineID trustLineID) const
{
    return {trustLineID,
            // todo mEncryptor,
            mLogger};
}
sphincs::PublicKey::Shared Keystore::generateAndSaveKeyPairForPaymentTransaction(
    IOTransaction::Shared ioTransaction,
    const TransactionUUID& transactionUUID)
{
    // Deprecated behavior retained temporarily for compatibility.
    auto keyPair = sphincs::util::generateKeyPair();
    auto privateKey = keyPair.first;
    auto publicKey = keyPair.second;
    ioTransaction->paymentKeysHandler()->saveOwnKey(publicKey, privateKey.get());
    return publicKey;
}

void Keystore::ensurePaymentKeyExists(IOTransaction::Shared ioTransaction)
{
    if (!ioTransaction->paymentKeysHandler()->hasAnyKeys()) {
        auto keyPair = sphincs::util::generateKeyPair();
        ioTransaction->paymentKeysHandler()->saveOwnKey(keyPair.second, keyPair.first.get());
    }
}

std::optional<sphincs::Signature::Shared> Keystore::signPaymentTransaction(IOTransaction::Shared ioTransaction,
        BytesShared dataForSign,
        size_t dataForSignBytesCount)
{
    try {
        auto privateKey = ioTransaction->paymentKeysHandler()->getOwnPrivateKey();
        auto signature = sphincs::util::signData(*privateKey, dataForSign.get(), dataForSignBytesCount);
        
        // Clean up the private key
        delete privateKey;
        return signature;
    } catch (const NotFoundError &e) {
        error() << "Can't get private payment key. Details: " << e.what();
        return std::nullopt;
    }
}

LoggerStream Keystore::info() const
{
    return mLogger.info(logHeader());
}

LoggerStream Keystore::debug() const
{
    return mLogger.debug(logHeader());
}

LoggerStream Keystore::warning() const
{
    return mLogger.warning(logHeader());
}

LoggerStream Keystore::error() const
{
    return mLogger.error(logHeader());
}

const string Keystore::logHeader() const
{
    stringstream s;
    s << "Keystore ";
    return s.str();
}

TrustLineKeychain::TrustLineKeychain(const TrustLineID trustLineID,
                                     // todo Encryptor encryptor,
                                     Logger& logger) noexcept
    :

    mTrustLineID(trustLineID),
    // todo mEncryptor(encryptor),
    mLogger(logger)
{}

void TrustLineKeychain::generateKeyPair(IOTransaction::Shared ioTransaction)
{
    KeyNumber currentKeysSetSequenceNumber;
    try {
        currentKeysSetSequenceNumber =
            ioTransaction->ownKeysHandler()->maxKeySetSequenceNumber(mTrustLineID);
        currentKeysSetSequenceNumber++;
    } catch (NotFoundError&) {
        currentKeysSetSequenceNumber = 0;
    }
    info() << "Keys set sequence number " << currentKeysSetSequenceNumber;
    
    auto keyPair = sphincs::util::generateKeyPair();
    auto privateKey = keyPair.first;
    auto publicKey = keyPair.second;
    
    try {
        ioTransaction->ownKeysHandler()->saveKey(
            mTrustLineID, currentKeysSetSequenceNumber, publicKey, privateKey.get());
    } catch (IOError& e) {
        warning() << "Can't save key pair. Details: " << e.what();
        throw e;
    }
}

sphincs::PublicKey::Shared TrustLineKeychain::publicKey(IOTransaction::Shared ioTransaction) const
{
    try {
        auto publicKey = ioTransaction->ownKeysHandler()->getPublicKey(mTrustLineID);
        return publicKey;
    } catch (NotFoundError& e) {
        return nullptr;
    } catch (IOError& e) {
        error() << "Can't get public key. Details: " << e.what();
        throw e;
    }
}

void TrustLineKeychain::setContractorPublicKey(IOTransaction::Shared ioTransaction,
        KeyNumber currentKeysSetSequenceNumber,
        const sphincs::PublicKey::Shared key)
{
    // todo: throw ConsistencyError in
    // case if contractor already has
    // key in this position.
    ioTransaction->contractorKeysHandler()->saveKey(
        mTrustLineID, currentKeysSetSequenceNumber, key);
}

bool TrustLineKeychain::contractorKeysPresent(IOTransaction::Shared ioTransaction)
{
    return ioTransaction->contractorKeysHandler()->hasKey(mTrustLineID);
}

bool TrustLineKeychain::ownKeysPresent(IOTransaction::Shared ioTransaction)
{
    return ioTransaction->ownKeysHandler()->hasKey(mTrustLineID);
}

bool TrustLineKeychain::isInitialAuditCondition(
    IOTransaction::Shared ioTransaction)
{
    AuditNumber currentAuditNumber = ioTransaction->auditHandler()->getActualAuditFull(mTrustLineID)->auditNumber();
    info() << "currentAuditNumber " << currentAuditNumber;
    return currentAuditNumber == 1;
}

sphincs::Signature::Shared TrustLineKeychain::sign(
    IOTransaction::Shared ioTransaction,
    BytesShared data,
    const std::size_t size)
{
    dataGuard(data, size);

    std::unique_ptr<PrivateKey> privateKey;
    try {
        privateKey = ioTransaction->ownKeysHandler()->getPrivateKey(mTrustLineID);
    } catch (NotFoundError& e) {
        warning() << "Can't get private key for TL " << mTrustLineID;
        throw e;
    }

    auto signature = sphincs::util::signData(*privateKey, data.get(), size);
    return signature;
}

bool TrustLineKeychain::checkSign(IOTransaction::Shared ioTransaction,
                                  BytesShared data,
                                  const size_t size,
                                  const sphincs::Signature::Shared signature)
{
    dataGuard(data, size);

    try {
        auto contractorPublicKey =
            ioTransaction->contractorKeysHandler()->getPublicKey(mTrustLineID);
        return signature->verify(*contractorPublicKey, data.get(), size);
    } catch (NotFoundError& e) {
        warning() << "There are no "
                     "data for TL "
                  << mTrustLineID;
        return false;
    } catch (IOError& e) {
        warning() << "Can't get contractor "
                     "public key. Details: "
                  << e.what();
        return false;
    }
}

bool TrustLineKeychain::saveOutgoingPaymentReceipt(IOTransaction::Shared ioTransaction,
        const AuditNumber auditNumber,
        const TransactionUUID& transactionUUID,
        const TrustLineAmount& amount,
        const sphincs::Signature::Shared signature)
{
    try {
        auto ownKeyHash =
            ioTransaction->ownKeysHandler()->getPublicKeyHash(mTrustLineID);

        ioTransaction->outgoingPaymentReceiptHandler()->saveRecord(
            mTrustLineID, auditNumber, transactionUUID, ownKeyHash, amount);

    } catch (NotFoundError& e) {
        warning() << "There are no valid own key";
        return false;
    } catch (IOError& e) {
        warning() << "Can't save outgoing receipt into storage. Details: " << e.what();
        return false;
    }
    return true;
}

bool TrustLineKeychain::saveIncomingPaymentReceipt(IOTransaction::Shared ioTransaction,
        const AuditNumber auditNumber,
        const TransactionUUID& transactionUUID,
        const TrustLineAmount& amount,
        const sphincs::Signature::Shared contractorSignature)
{
    try {
        auto contractorKeyHash = ioTransaction->contractorKeysHandler()->getPublicKeyHash(
                                     mTrustLineID);

        ioTransaction->incomingPaymentReceiptHandler()->saveRecord(mTrustLineID,
                auditNumber,
                transactionUUID,
                contractorKeyHash,
                amount,
                contractorSignature);

    } catch (NotFoundError& e) {
        warning() << "There are no valid contractor key";
        return false;                                                                                                                                   
    } catch (IOError& e) {
        warning() << "Can't save incoming receipt into storage. Details: " << e.what();
        return false;
    }
    return true;
}

void TrustLineKeychain::saveFullAudit(IOTransaction::Shared ioTransaction,
                                      const AuditNumber auditNumber,
                                      const sphincs::Signature::Shared ownSignature,
                                      const sphincs::Signature::Shared contractorSignature,
                                      const sphincs::KeyHash::Shared ownKeysSetHash,
                                      const sphincs::KeyHash::Shared contractorKeysSetHash,
                                      const TrustLineAmount& incomingAmount,
                                      const TrustLineAmount& outgoingAmount,
                                      const TrustLineBalance& balance)
{
    auto ownKeyHash = ioTransaction->ownKeysHandler()->getPublicKeyHash(mTrustLineID);
    auto contractorKeyHash = ioTransaction->contractorKeysHandler()->getPublicKeyHash(
                                 mTrustLineID);

    ioTransaction->auditHandler()->saveFullAudit(auditNumber,
            mTrustLineID,
            ownKeyHash,
            ownSignature,
            contractorKeyHash,
            contractorSignature,
            ownKeysSetHash,
            contractorKeysSetHash,
            incomingAmount,
            outgoingAmount,
            balance);
}

void TrustLineKeychain::saveOwnAuditPart(IOTransaction::Shared ioTransaction,
        const AuditNumber auditNumber,
        const sphincs::Signature::Shared ownSignature,
        const sphincs::KeyHash::Shared ownKeysSetHash,
        const sphincs::KeyHash::Shared contractorKeysSetHash,
        const TrustLineAmount& incomingAmount,
        const TrustLineAmount& outgoingAmount,
        const TrustLineBalance& balance)
{
    auto ownKeyHash = ioTransaction->ownKeysHandler()->getPublicKeyHash(mTrustLineID);

    ioTransaction->auditHandler()->saveOwnAuditPart(auditNumber,
            mTrustLineID,
            ownKeyHash,
            ownSignature,
            ownKeysSetHash,
            contractorKeysSetHash,
            incomingAmount,
            outgoingAmount,
            balance);
}

void TrustLineKeychain::removeCancelledOwnAuditPart(IOTransaction::Shared ioTransaction)
{
    auto actualAudit = ioTransaction->auditHandler()->getActualAuditFull(mTrustLineID);
    if (actualAudit->contractorSignature() != nullptr) {
        throw ValueError("Current audit is signed by contractor");
    }
    ioTransaction->auditHandler()->deleteAuditByNumber(mTrustLineID, actualAudit->auditNumber());
}

void TrustLineKeychain::saveContractorAuditPart(IOTransaction::Shared ioTransaction,
        const AuditNumber auditNumber,
        const sphincs::Signature::Shared contractorSignature)
{
    auto contractorKeyHash = ioTransaction->contractorKeysHandler()->getPublicKeyHash(
                                 mTrustLineID);

    ioTransaction->auditHandler()->saveContractorAuditPart(
        auditNumber, mTrustLineID, contractorKeyHash, contractorSignature);
}

bool TrustLineKeychain::isAuditWasCancelled(IOTransaction::Shared ioTransaction,
        const AuditNumber auditNumber)
{
    auto actualAudit = ioTransaction->auditHandler()->getActualAuditFull(
                           mTrustLineID);
    if (actualAudit->auditNumber() > auditNumber) {
        return true;
    }
    if (actualAudit->contractorSignature() != nullptr) {
        return true;
    }
    return false;
}

bool TrustLineKeychain::isActualAuditFull(
    IOTransaction::Shared ioTransaction)
{
    auto actualAudit = ioTransaction->auditHandler()->getActualAuditFull(
                           mTrustLineID);
    return actualAudit->contractorSignature() != nullptr;
}


sphincs::Signature::Shared
TrustLineKeychain::getSignatureForPendingAudit(IOTransaction::Shared ioTransaction,
        const AuditNumber auditNumber)
{
    auto auditRecord = ioTransaction->auditHandler()->getActualAuditFull(mTrustLineID);
    if (auditRecord->auditNumber() != auditNumber) {
        warning() << "Audit numbers are different: number from memory " << auditNumber
                  << " number from storage " << auditRecord->auditNumber();
        throw ValueError("Invalid audit number");
    }
    if (auditRecord->contractorSignature() != nullptr) {
        throw ValueError("Not empty contractor signature");
    }

    return auditRecord->ownSignature();
}

TrustLineAmount TrustLineKeychain::incomingCommittedReceiptsAmountsSum(
    IOTransaction::Shared ioTransaction,
    const AuditNumber auditNumber)
{
    TrustLineAmount result = TrustLine::kZeroAmount();
    auto incomingReceiptsAmounts =
        ioTransaction->incomingPaymentReceiptHandler()->auditAmounts(mTrustLineID, auditNumber);
    for (const auto& incomingReceiptAmount : incomingReceiptsAmounts) {
        if (!ioTransaction->transactionHandler()->isTransactionSerialized(
                    incomingReceiptAmount.first) and
                ioTransaction->paymentTransactionsHandler()->isTransactionPresent(
                    incomingReceiptAmount.first)) {
            result = result + incomingReceiptAmount.second;
        }
    }
    return result;
}

TrustLineAmount TrustLineKeychain::outgoingCommittedReceiptsAmountsSum(
    IOTransaction::Shared ioTransaction,
    const AuditNumber auditNumber)
{
    TrustLineAmount result = TrustLine::kZeroAmount();
    auto outgoingReceiptsAmounts =
        ioTransaction->outgoingPaymentReceiptHandler()->auditAmounts(mTrustLineID, auditNumber);
    for (const auto& outgoingReceiptAmount : outgoingReceiptsAmounts) {
        if (!ioTransaction->transactionHandler()->isTransactionSerialized(
                    outgoingReceiptAmount.first) and
                ioTransaction->paymentTransactionsHandler()->isTransactionPresent(
                    outgoingReceiptAmount.first)) {
            result = result + outgoingReceiptAmount.second;
        }
    }
    return result;
}

AuditRecord::Shared TrustLineKeychain::actualFullAudit(IOTransaction::Shared ioTransaction)
{
    return ioTransaction->auditHandler()->getActualAuditFull(mTrustLineID);
}

bool TrustLineKeychain::checkOwnConflictedSignature(IOTransaction::Shared ioTransaction,
        BytesShared data,
        const size_t size,
        const sphincs::Signature::Shared ownSignature,
        sphincs::KeyHash::Shared ownKeyHash)
{
    auto publicKey = ioTransaction->ownKeysHandler()->getPublicKeyByHash(mTrustLineID, ownKeyHash);
    return ownSignature->verify(*publicKey, data.get(), size);
}

bool TrustLineKeychain::checkContractorConflictedSignature(IOTransaction::Shared ioTransaction,
        BytesShared data,
        const size_t size,
        const sphincs::Signature::Shared contractorSignature,
        sphincs::KeyHash::Shared contractorKeyHash)
{
    auto publicKey =
        ioTransaction->contractorKeysHandler()->keyByHash(mTrustLineID, contractorKeyHash);
    return contractorSignature->verify(*publicKey, data.get(), size);
}

vector<ReceiptRecord::Shared>
TrustLineKeychain::incomingReceipts(IOTransaction::Shared ioTransaction, AuditNumber auditNumber)
{
    return ioTransaction->incomingPaymentReceiptHandler()->receiptsByAuditNumber(
               mTrustLineID, auditNumber);
}

vector<ReceiptRecord::Shared>
TrustLineKeychain::outgoingReceipts(IOTransaction::Shared ioTransaction, AuditNumber auditNumber)
{
    return ioTransaction->outgoingPaymentReceiptHandler()->receiptsByAuditNumber(
               mTrustLineID, auditNumber);
}

bool TrustLineKeychain::checkConflictedIncomingReceipt(IOTransaction::Shared ioTransaction,
        BytesShared data,
        const size_t size,
        const sphincs::Signature::Shared ownSignature,
        sphincs::KeyHash::Shared ownKeyHash)
{
    auto publicKey = ioTransaction->ownKeysHandler()->getPublicKeyByHash(mTrustLineID, ownKeyHash);
    return ownSignature->verify(*publicKey, data.get(), size);
}

bool TrustLineKeychain::checkConflictedOutgoingReceipt(IOTransaction::Shared ioTransaction,
        BytesShared data,
        const size_t size,
        const sphincs::Signature::Shared ownSignature,
        sphincs::KeyHash::Shared ownKeyHash)
{
    auto publicKey = ioTransaction->contractorKeysHandler()->keyByHash(mTrustLineID, ownKeyHash);
    return ownSignature->verify(*publicKey, data.get(), size);
}

void TrustLineKeychain::acceptAudit(IOTransaction::Shared ioTransaction,
                                    AuditRecord::Shared auditRecord)
{
    ioTransaction->auditHandler()->saveFullAudit(auditRecord->auditNumber(),
            mTrustLineID,
            auditRecord->contractorKeyHash(),
            auditRecord->contractorSignature(),
            auditRecord->ownKeyHash(),
            auditRecord->ownSignature(),
            auditRecord->ownKeysSetHash(),
            auditRecord->contractorKeysSetHash(),
            auditRecord->outgoingAmount(),
            auditRecord->incomingAmount(),
            auditRecord->balance() * (-1));

    ioTransaction->ownKeysHandler()->invalidateKeyByHash(
        mTrustLineID, auditRecord->contractorKeyHash(), auditRecord->contractorSignature());

    ioTransaction->contractorKeysHandler()->invalidateKeyByHash(
        mTrustLineID, auditRecord->ownKeyHash());
}

void TrustLineKeychain::acceptReceipts(IOTransaction::Shared ioTransaction,
                                       vector<ReceiptRecord::Shared> contractorIncomingReceipts,
                                       vector<ReceiptRecord::Shared> contractorOutgoingReceipts)
{
    for (const auto& contractorIncomingReceipt : contractorIncomingReceipts) {
        ioTransaction->outgoingPaymentReceiptHandler()->saveRecord(mTrustLineID,
                contractorIncomingReceipt->auditNumber(),
                contractorIncomingReceipt->transactionUUID(),
                contractorIncomingReceipt->keyHash(),
                contractorIncomingReceipt->amount());

        ioTransaction->ownKeysHandler()->invalidateKeyByHash(mTrustLineID,
                contractorIncomingReceipt->keyHash(),
                contractorIncomingReceipt->signature());
    }

    for (const auto& contractorOutgoingReceipt : contractorOutgoingReceipts) {
        ioTransaction->incomingPaymentReceiptHandler()->saveRecord(mTrustLineID,
                contractorOutgoingReceipt->auditNumber(),
                contractorOutgoingReceipt->transactionUUID(),
                contractorOutgoingReceipt->keyHash(),
                contractorOutgoingReceipt->amount(),
                contractorOutgoingReceipt->signature());

        ioTransaction->contractorKeysHandler()->invalidateKeyByHash(
            mTrustLineID, contractorOutgoingReceipt->keyHash());
    }
}

sphincs::Signature::Shared TrustLineKeychain::getCurrentAuditSignature(
    IOTransaction::Shared ioTransaction)
{
    auto auditRecord = ioTransaction->auditHandler()->getActualAuditFull(mTrustLineID);
    return auditRecord->ownSignature();
}

sphincs::KeyHash::Shared TrustLineKeychain::ownPublicKeysHash(
    IOTransaction::Shared ioTransaction) const
{
    auto currentKeysSetSequenceNumber =
        ioTransaction->ownKeysHandler()->maxKeySetSequenceNumber(mTrustLineID);
    auto ownPublicKey = ioTransaction->ownKeysHandler()->getPublicKey(
                            mTrustLineID);
    auto keyHash = make_shared<sphincs::KeyHash>(ownPublicKey->data());
    return keyHash;
}

sphincs::KeyHash::Shared TrustLineKeychain::contractorPublicKeysHash(
    IOTransaction::Shared ioTransaction) const
{
    auto currentKeysSetSequenceNumber =
        ioTransaction->contractorKeysHandler()->maxKeySetSequenceNumber(mTrustLineID);
    auto contractorPublicKey = ioTransaction->contractorKeysHandler()->getPublicKey(
                                   mTrustLineID);
    auto keyHash = make_shared<sphincs::KeyHash>(contractorPublicKey->data());
    return keyHash;
}

pair<bool, bool> TrustLineKeychain::checkKeysSetAppropriate(IOTransaction::Shared ioTransaction,
        sphincs::KeyHash::Shared auditOwnKeysSetHash,
        sphincs::KeyHash::Shared auditContractorKeysSetHash) const
{
    auto ownKeysSetHash = ownPublicKeysHash(ioTransaction);
    auto contractorKeysSetHash = contractorPublicKeysHash(ioTransaction);
    return make_pair(*auditOwnKeysSetHash == *ownKeysSetHash,
                     *auditContractorKeysSetHash == *contractorKeysSetHash);
}

void TrustLineKeychain::removeAllTrustLineData(IOTransaction::Shared ioTransaction)
{
    ioTransaction->outgoingPaymentReceiptHandler()->deleteRecords(mTrustLineID);
    ioTransaction->incomingPaymentReceiptHandler()->deleteRecords(mTrustLineID);
    ioTransaction->auditHandler()->deleteRecords(mTrustLineID);
    ioTransaction->ownKeysHandler()->deleteKeysByTrustLineID(mTrustLineID);
    ioTransaction->contractorKeysHandler()->deleteKeysByTrustLineID(mTrustLineID);
    // todo : remove audit rules
    // todo : remove all transactions
    // crypto data ??
}

void TrustLineKeychain::removeOutdatedCryptoData(IOTransaction::Shared ioTransaction,
        AuditNumber auditNumber)
{
    auto currentOwnKeysSetSequenceNumber =
        ioTransaction->ownKeysHandler()->maxKeySetSequenceNumber(mTrustLineID);
    auto currentContractorKeysSetSequenceNumber =
        ioTransaction->contractorKeysHandler()->maxKeySetSequenceNumber(mTrustLineID);

    auto outgoingReceipts = ioTransaction->outgoingPaymentReceiptHandler()->receiptsLessEqualThanAuditNumber(
                                mTrustLineID,
                                auditNumber);
    for (const auto &outgoingReceipt : outgoingReceipts) {
        ioTransaction->outgoingPaymentReceiptHandler()->deleteRecords(
            outgoingReceipt->transactionUUID());
        if (!isReceiptsPresent(ioTransaction, outgoingReceipt->transactionUUID())) {
            ioTransaction->paymentParticipantsVotesHandler()->deleteRecords(
                outgoingReceipt->transactionUUID());
            ioTransaction->paymentTransactionsHandler()->deleteRecord(
                outgoingReceipt->transactionUUID());
            // No per-transaction payment keys in single-key architecture
        }
        ioTransaction->ownKeysHandler()->deleteKeyByHashExceptSequenceNumber(
            outgoingReceipt->keyHash(),
            currentOwnKeysSetSequenceNumber);
    }
    auto incomingReceipts = ioTransaction->incomingPaymentReceiptHandler()->receiptsLessEqualThanAuditNumber(
                                mTrustLineID,
                                auditNumber);
    for (const auto &incomingReceipt : incomingReceipts) {
        ioTransaction->incomingPaymentReceiptHandler()->deleteRecords(
            incomingReceipt->transactionUUID());
        if (!isReceiptsPresent(ioTransaction, incomingReceipt->transactionUUID())) {
            ioTransaction->paymentParticipantsVotesHandler()->deleteRecords(
                incomingReceipt->transactionUUID());
            ioTransaction->paymentTransactionsHandler()->deleteRecord(
                incomingReceipt->transactionUUID());
            // No per-transaction payment keys in single-key architecture
        }
        ioTransaction->contractorKeysHandler()->deleteKeyByHashExceptSequenceNumber(
            incomingReceipt->keyHash(),
            currentContractorKeysSetSequenceNumber);
    }
    auto audits = ioTransaction->auditHandler()->auditsLessEqualThanAuditNumber(
                      mTrustLineID,
                      auditNumber);
    for (const auto &audit : audits) {
        ioTransaction->auditHandler()->deleteAuditByNumber(
            mTrustLineID,
            audit->auditNumber());
        ioTransaction->ownKeysHandler()->deleteKeyByHashExceptSequenceNumber(
            audit->ownKeyHash(),
            currentOwnKeysSetSequenceNumber);
        ioTransaction->contractorKeysHandler()->deleteKeyByHashExceptSequenceNumber(
            audit->contractorKeyHash(),
            currentContractorKeysSetSequenceNumber);
    }

    removeOutdatedKeys(
        ioTransaction,
        currentOwnKeysSetSequenceNumber);
}

void TrustLineKeychain::removeOutdatedCryptoPaymentsData(
    IOTransaction::Shared ioTransaction)
{
    auto paymentsTransactionsUUID = ioTransaction->paymentTransactionsHandler()->allTransactionsUUID();
    for (const auto &transactionUUID : paymentsTransactionsUUID) {
        if (!isReceiptsPresent(ioTransaction, transactionUUID)) {
            ioTransaction->paymentParticipantsVotesHandler()->deleteRecords(transactionUUID);
            ioTransaction->paymentTransactionsHandler()->deleteRecord(transactionUUID);
            // Single-key architecture: no per-transaction payment keys to delete
        }
    }
}


bool TrustLineKeychain::isReceiptsPresent(IOTransaction::Shared ioTransaction,
        const TransactionUUID& transactionUUID) const
{
    return ioTransaction->incomingPaymentReceiptHandler()->isContainsTransaction(transactionUUID) or
           ioTransaction->outgoingPaymentReceiptHandler()->isContainsTransaction(transactionUUID);
}

void TrustLineKeychain::removeOutdatedKeys(IOTransaction::Shared ioTransaction,
        const KeyNumber currentKeysSetSequenceNumber)
{
    auto ownKeyHashes = ioTransaction->ownKeysHandler()->publicKeyHashesLessThanSetNumber(
                            mTrustLineID, currentKeysSetSequenceNumber);
    for (auto& ownKeyHash : ownKeyHashes) {
        if (!ioTransaction->outgoingPaymentReceiptHandler()->isContainsKeyHash(ownKeyHash) and
                !ioTransaction->auditHandler()->isContainsKeyHash(ownKeyHash)) {
            ioTransaction->ownKeysHandler()->deleteKeyByHashExceptSequenceNumber(
                ownKeyHash, currentKeysSetSequenceNumber + 1);
        }
    }

    auto contractorKeyHashes =
        ioTransaction->contractorKeysHandler()->publicKeyHashesLessThanSetNumber(
            mTrustLineID, currentKeysSetSequenceNumber);
    for (auto& contractorKeyHash : contractorKeyHashes) {
        if (!ioTransaction->incomingPaymentReceiptHandler()->isContainsKeyHash(
                    contractorKeyHash) and
                !ioTransaction->auditHandler()->isContainsKeyHash(contractorKeyHash)) {
            ioTransaction->contractorKeysHandler()->deleteKeyByHashExceptSequenceNumber(
                contractorKeyHash, currentKeysSetSequenceNumber + 1);
        }
    }
}


void TrustLineKeychain::dataGuard(const BytesShared data, const size_t size) const
{
    if (data == nullptr) {
        // todo: throw ValueError;
    }

    if (size == 0) {
        // todo: throw ValueError;
    }
}

LoggerStream TrustLineKeychain::info() const
{
    return mLogger.info(logHeader());
}

LoggerStream TrustLineKeychain::error() const
{
    return mLogger.error(logHeader());
}

LoggerStream TrustLineKeychain::warning() const
{
    return mLogger.warning(logHeader());
}

const string TrustLineKeychain::logHeader() const
{
    stringstream s;
    s << "TrustLineKeychain: " << mTrustLineID << " ";
    return s.str();
}

} // namespace crypto
