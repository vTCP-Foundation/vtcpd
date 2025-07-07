#ifndef VTCPD_STORAGEPROVIDERFACTORY_H
#define VTCPD_STORAGEPROVIDERFACTORY_H

#include "../../settings/DatabaseProviderType.h"
#include "interfaces/StorageHandler.h"
#include "interfaces/CommunicatorStorageHandler.h"
#include "../../logger/Logger.h"

#include <memory>
#include <string>

using namespace std;

class StorageProviderFactory
{
public:
    // Статичні методи для створення Storage handlers
    static unique_ptr<StorageHandler> createStorageHandler(
        const DatabaseConfiguration &config,
        const string &mainDbName,
        Logger &logger);

    static unique_ptr<CommunicatorStorageHandler> createCommunicatorStorageHandler(
        const DatabaseConfiguration &config,
        const string &communicatorDbName,
        Logger &logger);

private:
    // Приватні методи для створення конкретних provider handlers
    static unique_ptr<StorageHandler> createSQLiteStorageHandler(
        const DatabaseConfiguration &config,
        const string &mainDbName,
        Logger &logger);

#ifdef POSTGRESQL_PROVIDER_AVAILABLE
    static unique_ptr<StorageHandler> createPostgreSQLStorageHandler(
        const DatabaseConfiguration &config,
        const string &mainDbName,
        Logger &logger);
#endif

    static unique_ptr<CommunicatorStorageHandler> createSQLiteCommunicatorStorageHandler(
        const DatabaseConfiguration &config,
        const string &communicatorDbName,
        Logger &logger);

#ifdef POSTGRESQL_PROVIDER_AVAILABLE
    static unique_ptr<CommunicatorStorageHandler> createPostgreSQLCommunicatorStorageHandler(
        const DatabaseConfiguration &config,
        const string &communicatorDbName,
        Logger &logger);

    // Допоміжні методи для створення connection strings
    static string createPostgreSQLConnectionString(
        const DatabaseConfiguration &config,
        const string &dbName);
#endif

    static void validateConfiguration(const DatabaseConfiguration &config);
};

#endif //VTCPD_STORAGEPROVIDERFACTORY_H 