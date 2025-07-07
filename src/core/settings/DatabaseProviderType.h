#ifndef VTCPD_DATABASEPROVIDERTYPE_H
#define VTCPD_DATABASEPROVIDERTYPE_H

#include <string>

using namespace std;

enum class DatabaseProviderType {
    SQLite,
    PostgreSQL
};

struct DatabaseConfiguration {
    DatabaseProviderType providerType;
    string directory;        // для SQLite - директорія
    string host;            // для PostgreSQL - хост
    int port;               // для PostgreSQL - порт
    string username;        // для PostgreSQL - юзер
    string password;        // для PostgreSQL - пароль
    
    DatabaseConfiguration() : providerType(DatabaseProviderType::SQLite), port(0) {}
    
    // Конструктор для SQLite
    DatabaseConfiguration(const string &dir) 
        : providerType(DatabaseProviderType::SQLite), directory(dir), port(0) {}
    
    // Конструктор для PostgreSQL
    DatabaseConfiguration(const string &h, int p, const string &u, const string &pass)
        : providerType(DatabaseProviderType::PostgreSQL), host(h), port(p), username(u), password(pass) {}
};

#endif //VTCPD_DATABASEPROVIDERTYPE_H 