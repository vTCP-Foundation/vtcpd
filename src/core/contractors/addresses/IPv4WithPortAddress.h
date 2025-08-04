#ifndef VTCPD_IPV4WITHPORTADDRESS_H
#define VTCPD_IPV4WITHPORTADDRESS_H

#include "BaseAddress.h"
#include "../../common/Constraints.h"
#include "../../network/communicator/internal/common/Types.h"
#include "../../common/exceptions/ValueError.h"

#include <string>

class IPv4WithPortAddress : public BaseAddress
{
public:
    typedef shared_ptr<IPv4WithPortAddress> Shared;

public:
    IPv4WithPortAddress(
        const string &fullAddress);

    IPv4WithPortAddress(
        const Host &host,
        const Port port);

    IPv4WithPortAddress(
        byte_t* buffer);

    // Overload accepting const pointer
    IPv4WithPortAddress(
        const byte_t* buffer) : IPv4WithPortAddress(const_cast<byte_t*>(buffer)) {}

    const string host() const override;

    const Port port() const override;

    const string fullAddress() const override;

    const AddressType typeID() const override;

    BytesShared serializeToBytes() const override;

    size_t serializedSize() const override;

private:
    byte_t mAddress[4];
    Port mPort;
};

#endif // VTCPD_IPV4WITHPORTADDRESS_H
