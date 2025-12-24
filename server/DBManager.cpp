// DBManager.cpp
#include "DBManager.h"
#include <iostream>

// DBConnection 实现
DBConnection::DBConnection(const std::string& db_path) 
    : db_path_(db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "无法打开数据库 " << db_path << ": " << sqlite3_errmsg(db_) << std::endl;
        db_ = nullptr;
        return;
    }
    
    // 优化设置
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL", 0, 0, 0);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL", 0, 0, 0);
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON", 0, 0, 0);
    
    std::cout << "数据库连接已打开: " << db_path << std::endl;
}

DBConnection::~DBConnection() {
    if (db_) {
        sqlite3_close(db_);
        std::cout << "数据库连接已关闭: " << db_path_ << std::endl;
    }
}

// DBManager 实现
DBManager& DBManager::getInstance() {
    static DBManager instance;
    return instance;
}

DBConnection* DBManager::getConnection(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = connections_.find(db_path);
    if (it != connections_.end()) {
        it->second->addRef();
        return it->second;
    }
    
    DBConnection* conn = new DBConnection(db_path);
    if (conn->isValid()) {
        conn->addRef(); // 初始引用计数为1
        connections_[db_path] = conn;
        return conn;
    } else {
        delete conn;
        return nullptr;
    }
}

void DBManager::releaseConnection(DBConnection* conn) {
    if (!conn) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    conn->release(); // 减少引用计数，如果为0会delete
}

void DBManager::closeAllConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : connections_) {
        // 强制删除，不管引用计数
        delete pair.second;
    }
    connections_.clear();
}

DBManager::~DBManager() {
    closeAllConnections();
}