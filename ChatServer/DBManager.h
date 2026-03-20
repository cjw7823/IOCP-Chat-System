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
    bool RegisterUser(const std::string& id, const std::string& passwordHash);
    bool GetUserAuthData(const std::string& id, std::string& outPasswordHash);

private:
    sqlite3* mDB = nullptr;
};