#ifndef VTCPD_SETTINGS_H
#define VTCPD_SETTINGS_H

#include "../common/Types.h"

#include "../common/exceptions/IOError.h"
#include "../common/exceptions/RuntimeError.h"

#include "../../libs/json/json.h"

#include <string>
#include <fstream>
#include <streambuf>

using namespace std;
using json = nlohmann::json;

class Settings
{
public:
    vector<pair<string, string>> addresses(
        const json *conf = nullptr) const;

    vector<pair<string, string>> observers(
        const json *conf = nullptr) const;

    vector<SerializedEquivalent> iAmGateway(
        const json *conf = nullptr) const;

    string equivalentsRegistryAddress(
        const json *conf = nullptr) const;

    pair<string, uint16_t> interface(
            const json *conf = nullptr) const;

    json providers(
        const json *conf = nullptr) const;

    json events(
        const json *conf = nullptr) const;

    json cyclesClearing(
        const json *conf = nullptr) const;

	int hopsCount(
		const json *conf = nullptr) const;

    json loadParsedJSON() const;
};

#endif //VTCPD_SETTINGS_H