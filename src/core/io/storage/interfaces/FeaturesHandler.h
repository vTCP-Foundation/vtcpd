#ifndef VTCPD_INTERFACES_FEATURESHANDLER_H
#define VTCPD_INTERFACES_FEATURESHANDLER_H

#include "../../../common/Types.h"
#include "../../../logger/Logger.h"
#include "../../../common/memory/MemoryUtils.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"

#include <string>
#include <memory>

using namespace std;

class FeaturesHandler
{
public:
    virtual ~FeaturesHandler() = default;

    virtual void saveFeature(
        const string &featureName,
        const string &featureValue) = 0;

    virtual string getFeature(
        const string &featureName) = 0;
};

#endif //VTCPD_INTERFACES_FEATURESHANDLER_H 