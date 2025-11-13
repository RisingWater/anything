// DBManager.h
#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <sqlite3.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <atomic>

class DBConnection {
public:
    DBConnection(const std::string& db_path);
    ~DBConnection();
    
    sqlite3* get() const { return db_; }
    bool isValid() const { return db_ != nullptr; }
    const std::string& getPath() const { return db_path_; }
    
    void addRef() { ref_count_++; }
    void release() { 
        if (--ref_count_ == 0) {
            delete this;
        }
    }

private:
    sqlite3* db_;
    std::string db_path_;
    std::atomic<int> ref_count_{0};
};

class DBManager {
public:
    static DBManager& getInstance();
    
    // 获取数据库连接（线程安全）
    DBConnection* getConnection(const std::string& db_path);
    
    // 释放连接
    void releaseConnection(DBConnection* conn);
    
    // 关闭所有连接（用于程序退出）
    void closeAllConnections();

private:
    DBManager() = default;
    ~DBManager();
    
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, DBConnection*> connections_;
};
#endif