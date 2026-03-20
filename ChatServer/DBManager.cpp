#include "DBManager.h"
#include <iostream>

DBManager::DBManager() {}

DBManager::~DBManager()
{
    Close();
}

bool DBManager::Open(const std::string& dbPath)
{
    int rc = sqlite3_open(dbPath.c_str(), &mDB);
    if (rc != SQLITE_OK)
    {
        std::cout << "DB open failed: " << sqlite3_errmsg(mDB) << std::endl;
        return false;
    }
    return true;
}

void DBManager::Close()
{
    if (mDB)
    {
        sqlite3_close(mDB);
        mDB = nullptr;
    }
}

//이미 테이블이 존재해도 성공.
bool DBManager::CreateUserTable()
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id TEXT PRIMARY KEY, "
        "password_hash TEXT NOT NULL);";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(mDB, sql, nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK)
    {
        std::cout << "Create table failed: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool DBManager::RegisterUser(const std::string& id, const std::string& passwordHash)
{
    const char* sql =
        "INSERT INTO users (id, password_hash) VALUES (?, ?);";

    //SQL 실행 객체
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(mDB, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool DBManager::GetUserAuthData(const std::string& id, std::string& outPasswordHash)
{
    const char* sql =
        "SELECT password_hash FROM users WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(mDB, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        const unsigned char* hash = sqlite3_column_text(stmt, 0);

        outPasswordHash = hash ? reinterpret_cast<const char*>(hash) : "";

        sqlite3_finalize(stmt);
        return true;
    }

    sqlite3_finalize(stmt);
    return false;
}