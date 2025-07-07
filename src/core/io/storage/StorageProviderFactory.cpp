#include "StorageProviderFactory.h"
#include "sqlite/StorageHandlerSQLite.h"
#include "sqlite/CommunicatorStorageHandlerSQLite.h"

#ifdef POSTGRESQL_PROVIDER_AVAILABLE
#include "postgresql/StorageHandlerPostgreSQL.h"
#include "postgresql/CommunicatorStorageHandlerPostgreSQL.h"
#endif

#include "../../common/exceptions/IOError.h"
#include "../../common/exceptions/ValueError.h"

#include <sstream>

using namespace std;

unique_ptr<StorageHandler> StorageProviderFactory::createStorageHandler(
    const DatabaseConfiguration &config,
    const string &mainDbName,
    Logger &logger) {
    
    validateConfiguration(config);
    
    switch (config.providerType) {
        case DatabaseProviderType::SQLite:
            return createSQLiteStorageHandler(config, mainDbName, logger);
        case DatabaseProviderType::PostgreSQL:
#ifdef POSTGRESQL_PROVIDER_AVAILABLE
            return createPostgreSQLStorageHandler(config, mainDbName, logger);
#else
            throw IOError("StorageProviderFactory::createStorageHandler: PostgreSQL provider is not available. This binary was compiled without PostgreSQL support.");
#endif
        default:
            throw IOError("StorageProviderFactory::createStorageHandler: Unsupported database provider type");
    }
}

unique_ptr<CommunicatorStorageHandler> StorageProviderFactory::createCommunicatorStorageHandler(
    const DatabaseConfiguration &config,
    const string &communicatorDbName,
    Logger &logger) {
    
    validateConfiguration(config);
    
    switch (config.providerType) {
        case DatabaseProviderType::SQLite:
            return createSQLiteCommunicatorStorageHandler(config, communicatorDbName, logger);
        case DatabaseProviderType::PostgreSQL:
#ifdef POSTGRESQL_PROVIDER_AVAILABLE
            return createPostgreSQLCommunicatorStorageHandler(config, communicatorDbName, logger);
#else
            throw IOError("StorageProviderFactory::createCommunicatorStorageHandler: PostgreSQL provider is not available. This binary was compiled without PostgreSQL support.");
#endif
        default:
            throw IOError("StorageProviderFactory::createCommunicatorStorageHandler: Unsupported database provider type");
    }
}

unique_ptr<StorageHandler> StorageProviderFactory::createSQLiteStorageHandler(
    const DatabaseConfiguration &config,
    const string &mainDbName,
    Logger &logger) {
    
    return make_unique<StorageHandlerSQLite>(
        config.directory,
        mainDbName,
        logger);
}

#ifdef POSTGRESQL_PROVIDER_AVAILABLE
unique_ptr<StorageHandler> StorageProviderFactory::createPostgreSQLStorageHandler(
    const DatabaseConfiguration &config,
    const string &mainDbName,
    Logger &logger) {
    
    string connectionString = createPostgreSQLConnectionString(config, mainDbName);
    return make_unique<StorageHandlerPostgreSQL>(
        connectionString,
        logger);
}
#endif

unique_ptr<CommunicatorStorageHandler> StorageProviderFactory::createSQLiteCommunicatorStorageHandler(
    const DatabaseConfiguration &config,
    const string &communicatorDbName,
    Logger &logger) {
    
    return make_unique<CommunicatorStorageHandlerSQLite>(
        config.directory,
        communicatorDbName,
        logger);
}

#ifdef POSTGRESQL_PROVIDER_AVAILABLE
unique_ptr<CommunicatorStorageHandler> StorageProviderFactory::createPostgreSQLCommunicatorStorageHandler(
    const DatabaseConfiguration &config,
    const string &communicatorDbName,
    Logger &logger) {
    
    string connectionString = createPostgreSQLConnectionString(config, communicatorDbName);
    return make_unique<CommunicatorStorageHandlerPostgreSQL>(
        connectionString,
        logger);
}

string StorageProviderFactory::createPostgreSQLConnectionString(
    const DatabaseConfiguration &config,
    const string &dbName) {
    
    stringstream ss;
    ss << "host=" << config.host
       << " port=" << config.port
       << " user=" << config.username
       << " password=" << config.password
       << " dbname=" << dbName;
    
    return ss.str();
}
#endif

void StorageProviderFactory::validateConfiguration(const DatabaseConfiguration &config) {
    if (config.providerType == DatabaseProviderType::SQLite) {
        if (config.directory.empty()) {
            throw ValueError("StorageProviderFactory::validateConfiguration: SQLite directory cannot be empty");
        }
    } else if (config.providerType == DatabaseProviderType::PostgreSQL) {
#ifndef POSTGRESQL_PROVIDER_AVAILABLE
        throw ValueError("StorageProviderFactory::validateConfiguration: PostgreSQL provider is not available. This binary was compiled without PostgreSQL support.");
#endif
        if (config.host.empty()) {
            throw ValueError("StorageProviderFactory::validateConfiguration: PostgreSQL host cannot be empty");
        }
        if (config.port <= 0 || config.port > 65535) {
            throw ValueError("StorageProviderFactory::validateConfiguration: PostgreSQL port must be between 1 and 65535");
        }
        if (config.username.empty()) {
            throw ValueError("StorageProviderFactory::validateConfiguration: PostgreSQL username cannot be empty");
        }
        if (config.password.empty()) {
            throw ValueError("StorageProviderFactory::validateConfiguration: PostgreSQL password cannot be empty");
        }
    } else {
        throw ValueError("StorageProviderFactory::validateConfiguration: Unknown database provider type");
    }
} 