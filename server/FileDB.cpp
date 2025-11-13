#include "FileDB.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

FileDB::FileDB(const std::string& db_path) : 
    db_conn_(nullptr), 
    db_path_(db_path), 
    is_connected_(false), 
    transaction_depth_(0) {
    init_database();
}

FileDB::~FileDB() {
    cleanup_prepared_statements();
    close();
}

bool FileDB::init_database() {
    db_conn_ = DBManager::getInstance().getConnection(db_path_);
    if (db_conn_ == nullptr || !db_conn_->isValid()) {
        std::cerr << "无法打开数据库: " << std::endl;
        return false;
    }
    
    is_connected_ = true;
    std::cout << "数据库连接已建立: " << db_path_ << std::endl;
    
    // 创建表
    const char* create_table_sql = 
        "CREATE TABLE IF NOT EXISTS file_info ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "file_path TEXT NOT NULL UNIQUE,"
        "file_name TEXT NOT NULL,"
        "modified_time TEXT,"
        "created_time TEXT,"
        "file_extension TEXT,"
        "mime_type TEXT,"
        "is_directory INTEGER,"
        "parent_directory TEXT,"
        "last_scanned_time TEXT,"
        "scan_count INTEGER DEFAULT 0"
        ")";
    
    if (!execute_sql(create_table_sql)) {
        std::cerr << "创建表失败" << std::endl;
        return false;
    }
    
    // 创建索引
    std::vector<std::string> indexes = {
        "CREATE INDEX IF NOT EXISTS idx_file_path ON file_info(file_path)",
        "CREATE INDEX IF NOT EXISTS idx_file_name ON file_info(file_name)",
        "CREATE INDEX IF NOT EXISTS idx_file_extension ON file_info(file_extension)",
        "CREATE INDEX IF NOT EXISTS idx_mime_type ON file_info(mime_type)",
        "CREATE INDEX IF NOT EXISTS idx_parent_directory ON file_info(parent_directory)",
        "CREATE INDEX IF NOT EXISTS idx_is_directory ON file_info(is_directory)",
        "PRAGMA synchronous = NORMAL"      // 平衡模式（默认FULL）
        "PRAGMA journal_mode = WAL"        // 写前日志（比OFF安全）
        "PRAGMA cache_size = 100000"       // 100MB缓存
        "PRAGMA page_size = 4096"          // 保持默认页大小
        "PRAGMA mmap_size = 268435456 "    // 256MB内存映射
        "PRAGMA temp_store = MEMORY"       // 临时表在内存中
    };
   
    std::cout << "SQLite优化设置完成" << std::endl;
    
    for (const auto& index_sql : indexes) {
        if (!execute_sql(index_sql)) {
            std::cerr << "创建索引失败: " << index_sql << std::endl;
        }
    }
    
    std::cout << "数据库表结构初始化完成" << std::endl;
    return true;
}

bool FileDB::execute_sql(const std::string& sql) {
    if (!is_connected_) return false;

    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_conn_->get(), sql.c_str(), nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL执行错误: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    
    return true;
}

bool FileDB::begin_transaction() {
    if (transaction_depth_ == 0) {
        if (!execute_sql("BEGIN TRANSACTION")) {
            return false;
        }
    }
    transaction_depth_++;

    return true;
}

bool FileDB::commit_transaction() {
    if (transaction_depth_ == 0) {
        return false;  // 没有活跃事务
    }

    transaction_depth_--;
    if (transaction_depth_ == 0) {
        return execute_sql("COMMIT");
    }
    return true;  // 嵌套事务，不真正提交
}

bool FileDB::rollback_transaction() {
    if (transaction_depth_ == 0) {
        return false;
    }
    transaction_depth_ = 0;  // 所有嵌套都回滚
    return execute_sql("ROLLBACK");
}

sqlite3_stmt* FileDB::get_prepared_statement(const std::string& sql) {
    // 查找是否已经准备过这个SQL
    auto it = prepared_statements_.find(sql);
    if (it != prepared_statements_.end()) {
        return it->second;
    }
    
    // 准备新的statement
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_conn_->get(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "准备SQL语句失败: " << sqlite3_errmsg(db_conn_->get()) 
                  << " SQL: " << sql << std::endl;
        return nullptr;
    }
    
    // 缓存准备好的statement
    prepared_statements_[sql] = stmt;
    std::cout << "已缓存prepared statement: " << sql << std::endl;
    
    return stmt;
}

void FileDB::cleanup_prepared_statements() {
    for (auto& [sql, stmt] : prepared_statements_) {
        if (stmt) {
            sqlite3_finalize(stmt);
            std::cout << "已清理prepared statement: " << sql << std::endl;
        }
    }
    prepared_statements_.clear();
}

bool FileDB::execute_sql_with_params(const std::string& sql, 
                                   const std::vector<std::string>& params) {
    if (!is_connected_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    if (!stmt) {
        return false;
    }
    
    // 绑定参数
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    
    // 执行
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "执行SQL失败: " << sqlite3_errmsg(db_conn_->get()) << std::endl;
        sqlite3_reset(stmt);
        return false;
    }
    
    sqlite3_reset(stmt);
    return true;
}

std::string FileDB::get_current_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

bool FileDB::insert_file(const FileInfo& file_info) {
    auto org = get_file(file_info.file_path);
    if (org) {
        if (org->modified_time != file_info.modified_time) {
            return update_file(file_info.file_path, file_info);
        } else {
            return true;
        }
    }

    const std::string sql = 
        "INSERT INTO file_info "
        "(file_path, file_name, modified_time, created_time, "
        "file_extension, mime_type, is_directory, parent_directory, last_scanned_time, scan_count) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1)";
    
    std::vector<std::string> params = {
        file_info.file_path,
        file_info.file_name,
        file_info.modified_time,
        file_info.created_time,
        file_info.file_extension,
        file_info.mime_type,
        std::to_string(file_info.is_directory),
        file_info.parent_directory,
        get_current_time()
    };
    
    if (!execute_sql_with_params(sql, params)) {
        return false;
    }

    return true;
}

bool FileDB::update_file(const std::string& file_path, const FileInfo& file_info) {
    std::string sql = "UPDATE file_info SET ";
    std::vector<std::string> params;
    std::vector<std::string> updates;
    
    if (!file_info.file_name.empty()) {
        updates.push_back("file_name = ?");
        params.push_back(file_info.file_name);
    }
    if (!file_info.modified_time.empty()) {
        updates.push_back("modified_time = ?");
        params.push_back(file_info.modified_time);
    }
    if (!file_info.created_time.empty()) {
        updates.push_back("created_time = ?");
        params.push_back(file_info.created_time);
    }
    if (!file_info.file_extension.empty()) {
        updates.push_back("file_extension = ?");
        params.push_back(file_info.file_extension);
    }
    if (!file_info.mime_type.empty()) {
        updates.push_back("mime_type = ?");
        params.push_back(file_info.mime_type);
    }
    updates.push_back("is_directory = ?");
    params.push_back(std::to_string(file_info.is_directory));
    
    if (!file_info.parent_directory.empty()) {
        updates.push_back("parent_directory = ?");
        params.push_back(file_info.parent_directory);
    }
    
    updates.push_back("last_scanned_time = ?");
    params.push_back(get_current_time());
    updates.push_back("scan_count = scan_count + 1");
    
    if (updates.empty()) {
        std::cerr << "没有可更新的字段" << std::endl;
        return false;
    }
    
    sql += " " + updates[0];
    for (size_t i = 1; i < updates.size(); ++i) {
        sql += ", " + updates[i];
    }
    sql += " WHERE file_path = ?";
    params.push_back(file_path);
    
    if (execute_sql_with_params(sql, params)) {
        return true;
    }
    
    std::cerr << "文件信息更新失败: " << file_path << std::endl;
    return false;
}

bool FileDB::delete_file(const std::string& file_path) {
    const std::string sql = "DELETE FROM file_info WHERE file_path = ?";
    std::vector<std::string> params = {file_path};
    
    if (execute_sql_with_params(sql, params)) {
        std::cout << "文件记录删除成功: " << file_path << std::endl;
        return true;
    }
    
    std::cerr << "文件记录删除失败: " << file_path << std::endl;
    return false;
}

bool FileDB::delete_files_by_directory(const std::string& directory_path) {
    const std::string sql = "DELETE FROM file_info WHERE parent_directory = ? OR file_path = ?";
    std::vector<std::string> params = {directory_path, directory_path};
    
    if (execute_sql_with_params(sql, params)) {
        std::cout << "删除目录记录成功: " << directory_path << std::endl;
        return true;
    }
    
    std::cerr << "删除目录记录失败: " << directory_path << std::endl;
    return false;
}

std::unique_ptr<FileInfo> FileDB::get_file(const std::string& file_path) {    
    const std::string sql = "SELECT * FROM file_info WHERE file_path = ?";
    std::vector<std::string> params = {file_path};
    
    if (!is_connected_) return nullptr;

    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    if (!stmt) {
        std::cerr << "准备SQL语句失败: " << sqlite3_errmsg(db_conn_->get()) << std::endl;
        return nullptr;
    }
        
    sqlite3_bind_text(stmt, 1, file_path.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto file_info = std::make_unique<FileInfo>();
        file_info->id = sqlite3_column_int(stmt, 0);
        file_info->file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        file_info->file_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        file_info->modified_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        file_info->created_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        file_info->file_extension = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        file_info->mime_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        file_info->is_directory = sqlite3_column_int(stmt, 7);
        file_info->parent_directory = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        file_info->last_scanned_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        file_info->scan_count = sqlite3_column_int(stmt, 10);
        
        sqlite3_reset(stmt);
        return file_info;
    }
    
    sqlite3_reset(stmt);
    return nullptr;
}

bool FileDB::file_exists(const std::string& file_path) {
    const std::string sql = "SELECT COUNT(*) FROM file_info WHERE file_path = ?";
    std::vector<std::string> params = {file_path};
    
    if (!is_connected_) return false;

    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    sqlite3_stmt* stmt = get_prepared_statement(sql);
    if (!stmt) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, file_path.c_str(), -1, SQLITE_TRANSIENT);
    
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0) > 0;
    }
    
    sqlite3_reset(stmt);
    return exists;
}

std::vector<FileInfo> FileDB::search_files(const std::string& search_term, 
                                         const std::string& search_field,
                                         int limit) {
    std::vector<FileInfo> results;
    
    std::vector<std::string> valid_fields = {
        "file_name", "file_path", "file_extension", "mime_type", "parent_directory"
    };
    
    if (std::find(valid_fields.begin(), valid_fields.end(), search_field) == valid_fields.end()) {
        std::cerr << "无效的搜索字段: " << search_field << std::endl;
        return results;
    }
    
    std::string sql = "SELECT * FROM file_info WHERE " + search_field + " LIKE ? ORDER BY file_path LIMIT ?";
    std::string pattern = "%" + search_term + "%";
    
    if (!is_connected_) return results;

    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_conn_->get(), sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "准备SQL语句失败: " << sqlite3_errmsg(db_conn_->get()) << std::endl;
        return results;
    }
    
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FileInfo file_info;
        file_info.id = sqlite3_column_int(stmt, 0);
        file_info.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        file_info.file_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        file_info.modified_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        file_info.created_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        file_info.file_extension = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        file_info.mime_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        file_info.is_directory = sqlite3_column_int(stmt, 7);
        file_info.parent_directory = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        file_info.last_scanned_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        file_info.scan_count = sqlite3_column_int(stmt, 10);
        
        results.push_back(file_info);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

std::vector<FileInfo> FileDB::get_files_by_parent_directory(const std::string& parent_directory) {    
    std::vector<FileInfo> results;
    const std::string sql = "SELECT * FROM file_info WHERE parent_directory = ?";
    
    if (!is_connected_) return results;

    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_conn_->get(), sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "准备SQL语句失败: " << sqlite3_errmsg(db_conn_->get()) << std::endl;
        return results;
    }
    
    sqlite3_bind_text(stmt, 1, parent_directory.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FileInfo file_info;
        file_info.id = sqlite3_column_int(stmt, 0);
        file_info.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        file_info.file_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        file_info.modified_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        file_info.created_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        file_info.file_extension = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        file_info.mime_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        file_info.is_directory = sqlite3_column_int(stmt, 7);
        file_info.parent_directory = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        file_info.last_scanned_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        file_info.scan_count = sqlite3_column_int(stmt, 10);
        
        results.push_back(file_info);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

bool FileDB::batch_delete_files(const std::vector<std::string>& file_paths) {
    if (file_paths.empty()) return true;

    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    std::string sql = "DELETE FROM file_info WHERE file_path IN (";
    for (size_t i = 0; i < file_paths.size(); ++i) {
        sql += "?";
        if (i < file_paths.size() - 1) sql += ",";
    }
    sql += ")";
    
    if (execute_sql_with_params(sql, file_paths)) {
        std::cout << "批量删除文件成功，数量: " << file_paths.size() << std::endl;
        return true;
    }
    
    std::cerr << "批量删除文件失败" << std::endl;
    return false;
}

std::unordered_map<std::string, int> FileDB::get_database_stats() {    
    std::unordered_map<std::string, int> stats;
    
    if (!is_connected_) return stats;

    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    // 总文件数
    sqlite3_stmt* stmt;
    const char* sql = "SELECT COUNT(*) FROM file_info";
    if (sqlite3_prepare_v2(db_conn_->get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats["total_files"] = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    // 目录数
    sql = "SELECT COUNT(*) FROM file_info WHERE is_directory = 1";
    if (sqlite3_prepare_v2(db_conn_->get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats["total_dirs"] = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    // 实际文件数
    sql = "SELECT COUNT(*) FROM file_info WHERE is_directory = 0";
    if (sqlite3_prepare_v2(db_conn_->get(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats["total_real_files"] = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    return stats;
}

bool FileDB::clear_database() {
    const std::string sql = "DELETE FROM file_info";
    
    if (execute_sql(sql)) {
        std::cout << "数据库已清空" << std::endl;
        return true;
    }
    
    std::cerr << "清空数据库失败" << std::endl;
    return false;
}

void FileDB::close() {
    if (db_conn_) {
        DBManager::getInstance().releaseConnection(db_conn_);
        db_conn_ = nullptr;
        is_connected_ = false;
        std::cout << "数据库连接已关闭" << std::endl;
    }
}