#include "Settings.h"

#define MAX_HOPS_COUNT 5

json Settings::loadParsedJSON() const
{
    string buffer;
    try {
        ifstream stream("conf.json");
        buffer.assign(
            (std::istreambuf_iterator<char>(stream)), // parentheses are valuable!
            std::istreambuf_iterator<char>());
        return json::parse(buffer.data());
    } catch (...) {
        throw IOError(
            "Settings::loadParsedJSON: "
            "Can't read conf.json");
    }
}

vector<pair<string, string>> Settings::addresses(
    const json *conf) const {
    if (conf == nullptr)
    {
        auto j = loadParsedJSON();
        conf = &j;
    }
    vector<pair<string, string>> result;
    try
    {
        auto addresses = (*conf).at("addresses");
        for (const auto &address : addresses) {
            result.emplace_back(
                address.at("type").get<string>(),
                address.at("address").get<string>());
        }
        return result;
    } catch (...)
    {
        // todo : throw RuntimeError
        return result;
    }
}

vector<pair<string, string>> Settings::observers(
    const json *conf) const {
    if (conf == nullptr)
    {
        auto j = loadParsedJSON();
        conf = &j;
    }
    vector<pair<string, string>> result;
    try
    {
        auto addresses = (*conf).at("observers");
        for (const auto &address : addresses) {
            result.emplace_back(
                address.at("type").get<string>(),
                address.at("address").get<string>());
        }
        return result;
    } catch (...)
    {
        // todo : throw RuntimeError
        return result;
    }
}

vector<SerializedEquivalent> Settings::iAmGateway(
    const json *conf) const
{
    if (conf == nullptr) {
        auto j = loadParsedJSON();
        conf = &j;
    }
    vector<SerializedEquivalent> result;
    try {
        result = (*conf).at("gateway").get<vector<SerializedEquivalent>>();
        return result;
    }
    catch (...) {
        // todo : throw RuntimeError
        return result;
    }
}

string Settings::equivalentsRegistryAddress(
    const json *conf) const {
    if (conf == nullptr)
    {
        auto j = loadParsedJSON();
        conf = &j;
    }
    try {
        auto result = (*conf).at("equivalents_registry_address").get<string>();
        return result;
    } catch (...)
    {
        // todo : throw RuntimeError
        return "";
    }
}

pair<string, uint16_t> Settings::interface(
    const json *conf) const
{
    if (conf == nullptr) {
        auto j = loadParsedJSON();
        conf = &j;
    }
    pair<string, uint16_t> result;
    try {
        auto interface = (*conf).at("interface");
        auto address = interface.at("host").get<string>();
        auto port = (uint16_t)interface.at("port").get<int>();
        return make_pair(
                   address,
                   port);
    } catch (...) {
        // todo : throw RuntimeError
        return make_pair("", 0);
    }
}

json Settings::providers(
    const json *conf) const
{
    if (conf == nullptr) {
        auto j = loadParsedJSON();
        conf = &j;
    }
    try {
        auto result = (*conf).at("providers");
        return result;
    } catch (...) {
        // todo : throw RuntimeError
        return nullptr;
    }
}

json Settings::events(
    const json *conf) const
{
    if (conf == nullptr) {
        auto j = loadParsedJSON();
        conf = &j;
    }
    try {
        auto result = (*conf).at("event_files");
        return result;
    } catch (...) {
        // todo : throw RuntimeError
        return nullptr;
    }
}

json Settings::cyclesClearing(
    const json *conf) const
{
    if (conf == nullptr) {
        auto j = loadParsedJSON();
        conf = &j;
    }
    try {
        auto result = (*conf).at("cycles_clearing");
        return result;
    } catch (...) {
        // todo : throw RuntimeError
        return nullptr;
    }
}

int Settings::hopsCount(const json *conf) const {
	if (conf == nullptr) {
        auto j = loadParsedJSON();
        conf = &j;
    }
    try {

        auto result = (*conf).at("max_hops_count").get<int>();
		if(result > 5) {
			 throw("Max max_hops_count > 5"); 
		}
        return result;

    }
	catch(const char* msg) {
		 throw IOError(msg);  
	}
	catch (...) {
        // todo : throw RuntimeError
		 throw IOError("Not found max_hops_count!");   
    } 

	return 0;
}

DatabaseConfiguration Settings::databaseConfiguration(const json *conf) const {
    if (conf == nullptr) {
        auto j = loadParsedJSON();
        conf = &j;
    }
    
    try {
        if (conf->contains("database_config")) {
            auto dbConfigStr = (*conf).at("database_config").get<string>();
            return parseDatabaseURI(dbConfigStr);
        } else {
            // Default SQLite configuration if not specified
            return DatabaseConfiguration("io");
        }
    } catch (const std::exception &e) {
        throw IOError("Settings::databaseConfiguration: Failed to parse database configuration: " + string(e.what()));
    }
}

DatabaseConfiguration Settings::parseDatabaseURI(const string &uri) const {
    if (uri.empty()) {
        throw IOError("Settings::parseDatabaseURI: URI string is empty");
    }

    // Check for SQLite URI scheme
    if (uri.find("sqlite3://") == 0) {
        return parseSQLiteURI(uri);
    }
    // Check for PostgreSQL URI scheme
    else if (uri.find("postgresql://") == 0) {
        return parsePostgreSQLURI(uri);
    }
    else {
        throw IOError("Settings::parseDatabaseURI: Unsupported URI scheme. Expected 'sqlite3://' or 'postgresql://'");
    }
}

DatabaseConfiguration Settings::parseSQLiteURI(const string &uri) const {
    // Format: sqlite3:///path/to/directory
    if (uri.length() < 12) { // "sqlite3:///" is 12 characters
        throw IOError("Settings::parseSQLiteURI: Invalid SQLite URI format");
    }

    string path = uri.substr(11); // Remove "sqlite3://"
    if (path.empty()) {
        throw IOError("Settings::parseSQLiteURI: Directory path is empty");
    }

    DatabaseConfiguration config(path);
    validateDatabaseConfiguration(config);
    return config;
}

DatabaseConfiguration Settings::parsePostgreSQLURI(const string &uri) const {
    // Format: postgresql://user:password@host:port/database
    if (uri.length() < 15) { // "postgresql://" is 13 characters
        throw IOError("Settings::parsePostgreSQLURI: Invalid PostgreSQL URI format");
    }

    string connectionPart = uri.substr(13); // Remove "postgresql://"
    
    // Find user:password@host:port/database pattern
    size_t atPos = connectionPart.find('@');
    if (atPos == string::npos) {
        throw IOError("Settings::parsePostgreSQLURI: Missing '@' separator in URI");
    }

    string userPass = connectionPart.substr(0, atPos);
    string hostPortDb = connectionPart.substr(atPos + 1);

    // Parse user:password
    size_t colonPos = userPass.find(':');
    if (colonPos == string::npos) {
        throw IOError("Settings::parsePostgreSQLURI: Missing ':' separator in user:password");
    }

    string username = userPass.substr(0, colonPos);
    string password = userPass.substr(colonPos + 1);

    // Parse host:port/database
    size_t colonPos2 = hostPortDb.find(':');
    if (colonPos2 == string::npos) {
        throw IOError("Settings::parsePostgreSQLURI: Missing ':' separator in host:port");
    }

    string host = hostPortDb.substr(0, colonPos2);
    string portDbStr = hostPortDb.substr(colonPos2 + 1);
    
    // Parse port/database
    size_t slashPos = portDbStr.find('/');
    string portStr;
    string database;
    
    if (slashPos != string::npos) {
        portStr = portDbStr.substr(0, slashPos);
        database = portDbStr.substr(slashPos + 1);
    } else {
        // No database specified, use default behavior
        portStr = portDbStr;
        database = "";
    }

    int port;
    try {
        port = stoi(portStr);
    } catch (const std::exception &e) {
        throw IOError("Settings::parsePostgreSQLURI: Invalid port number: " + portStr);
    }

    DatabaseConfiguration config(host, port, username, password, database);
    validateDatabaseConfiguration(config);
    return config;
}

void Settings::validateDatabaseConfiguration(const DatabaseConfiguration &config) const {
    if (config.providerType == DatabaseProviderType::SQLite) {
        if (config.directory.empty()) {
            throw IOError("Settings::validateDatabaseConfiguration: SQLite directory cannot be empty");
        }
    } else if (config.providerType == DatabaseProviderType::PostgreSQL) {
        if (config.host.empty()) {
            throw IOError("Settings::validateDatabaseConfiguration: PostgreSQL host cannot be empty");
        }
        if (config.port <= 0 || config.port > 65535) {
            throw IOError("Settings::validateDatabaseConfiguration: PostgreSQL port must be between 1 and 65535");
        }
        if (config.username.empty()) {
            throw IOError("Settings::validateDatabaseConfiguration: PostgreSQL username cannot be empty");
        }
        if (config.password.empty()) {
            throw IOError("Settings::validateDatabaseConfiguration: PostgreSQL password cannot be empty");
        }
        // Note: database name is optional - if not provided, default names will be used
    }
}

vector<pair<SerializedEquivalent, uint64_t>> Settings::commissions(
    const json *conf) const
{
    if (conf == nullptr) {
        auto j = loadParsedJSON();
        conf = &j;
    }
    
    vector<pair<SerializedEquivalent, uint64_t>> result;
    try {
        if (conf->contains("commissions") && conf->at("commissions").contains("byEquivalent")) {
            auto commissionsByEquiv = conf->at("commissions").at("byEquivalent");
            
            for (auto& [key, value] : commissionsByEquiv.items()) {
                try {
                    SerializedEquivalent equivalent = stoul(key);
                    uint64_t amount = value.at("amount").get<uint64_t>();
                    result.emplace_back(equivalent, amount);
                } catch (const exception& e) {
                    // Skip malformed entries
                    continue;
                }
            }
        }
        return result;
    } catch (...) {
        return result;
    }
}
