#pragma once
#include "../SQLlite/sqlite3.h"
#include <string>

class DBManager
{
public:
    DBManager();
    ~DBManager();

    bool Open(const std::string& dbPath);
    void Close();

    bool CreateUserTable();
    bool RegisterUser(const std::string& id, const std::string& hash, const std::string& salt);
    bool GetUserAuthData(const std::string& id, std::string& outHash, std::string& outSalt);

private:
    sqlite3* mDB = nullptr;
};