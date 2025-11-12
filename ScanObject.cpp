#include "ScanObject.h"
#include <iostream>
#include <sstream>
#include <iomanip>

ScanObject::ScanObject(const std::string& db_path) 
    : db_(nullptr), db_path_(db_path), is_connected_(false) {
    init_database();
}

ScanObject::~ScanObject() {
    close();
}

bool ScanObject::init_database() {
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "无法打开数据库: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    
    is_connected_ = true;
    std::cout << "扫描对象数据库连接已建立: " << db_path_ << std::endl;
    
    // 创建表
    const char* create_table_sql = 
        "CREATE TABLE IF NOT EXISTS scan_objects ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "directory_path TEXT NOT NULL UNIQUE,"
        "display_name TEXT,"
        "description TEXT,"
        "is_active INTEGER DEFAULT 1,"
        "is_recursive INTEGER DEFAULT 1,"
        "last_successful_scan_time TEXT"
        ")";
    
    if (!execute_sql(create_table_sql)) {
        std::cerr << "创建表失败" << std::endl;
        return false;
    }
    
    // 创建索引
    std::vector<std::string> indexes = {
        "CREATE INDEX IF NOT EXISTS idx_scan_objects_path ON scan_objects(directory_path)",
        "CREATE INDEX IF NOT EXISTS idx_scan_objects_active ON scan_objects(is_active)"
    };
    
    for (const auto& index_sql : indexes) {
        if (!execute_sql(index_sql)) {
            std::cerr << "创建索引失败: " << index_sql << std::endl;
        }
    }
    
    std::cout << "扫描对象表结构初始化完成" << std::endl;
    return true;
}

bool ScanObject::execute_sql(const std::string& sql) {
    if (!is_connected_) return false;
    
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL执行错误: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    
    return true;
}

bool ScanObject::execute_sql_with_params(const std::string& sql, 
                                       const std::vector<std::string>& params) {
    if (!is_connected_) return false;
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "准备SQL语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    
    // 绑定参数
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    
    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    sqlite3_finalize(stmt);
    return success;
}

std::string ScanObject::get_current_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

bool ScanObject::add_scan_object(const std::string& directory_path,
                                const std::string& display_name,
                                const std::string& description,
                                bool is_recursive) {
    // 验证目录是否存在
    std::filesystem::path dir_path(directory_path);
    if (!std::filesystem::exists(dir_path)) {
        std::cerr << "目录不存在: " << directory_path << std::endl;
        return false;
    }
    
    if (!std::filesystem::is_directory(dir_path)) {
        std::cerr << "路径不是目录: " << directory_path << std::endl;
        return false;
    }
    
    // 设置默认显示名称
    std::string actual_display_name = display_name;
    if (actual_display_name.empty()) {
        actual_display_name = dir_path.filename().string();
    }
    
    const std::string sql = 
        "INSERT INTO scan_objects "
        "(directory_path, display_name, description, is_recursive) "
        "VALUES (?, ?, ?, ?)";
    
    std::vector<std::string> params = {
        std::filesystem::absolute(dir_path).string(),
        actual_display_name,
        description,
        is_recursive ? "1" : "0"
    };
    
    if (execute_sql_with_params(sql, params)) {
        std::cout << "扫描对象添加成功: " << directory_path << std::endl;
        return true;
    }
    
    std::cerr << "扫描对象已存在: " << directory_path << std::endl;
    return false;
}

bool ScanObject::delete_scan_object(const std::string& id)
{
    const std::string sql = "DELETE FROM scan_objects WHERE id = ?";

    std::vector<std::string> params = {id};

    if (execute_sql_with_params(sql, params)) {
        std::cout << "扫描对象删除成功: " << id << std::endl;
        return true;
    }

    std::cerr << "扫描对象不存在: " << id << std::endl;
    return false;
}

bool ScanObject::update_last_scan_time(const std::string& directory_path) {
    std::filesystem::path dir_path(directory_path);
    std::string absolute_path = std::filesystem::absolute(dir_path).string();
    
    const std::string sql = 
        "UPDATE scan_objects "
        "SET last_successful_scan_time = ? "
        "WHERE directory_path = ?";
    
    std::vector<std::string> params = {
        get_current_time(),
        absolute_path
    };
    
    if (execute_sql_with_params(sql, params)) {
        std::cout << "扫描时间更新成功: " << directory_path << std::endl;
        return true;
    }
    
    std::cerr << "扫描对象不存在: " << directory_path << std::endl;
    return false;
}

std::unique_ptr<ScanObjectInfo> ScanObject::get_scan_object(const std::string& directory_path) {
    std::filesystem::path dir_path(directory_path);
    std::string absolute_path = std::filesystem::absolute(dir_path).string();
    
    const std::string sql = "SELECT * FROM scan_objects WHERE directory_path = ?";
    std::vector<std::string> params = {absolute_path};
    
    if (!is_connected_) return nullptr;
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "准备SQL语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return nullptr;
    }
    
    sqlite3_bind_text(stmt, 1, absolute_path.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto scan_obj = std::make_unique<ScanObjectInfo>();
        scan_obj->id = sqlite3_column_int(stmt, 0);
        scan_obj->directory_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        scan_obj->display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        scan_obj->description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        scan_obj->is_active = sqlite3_column_int(stmt, 4);
        scan_obj->is_recursive = sqlite3_column_int(stmt, 5);
        
        const char* scan_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (scan_time) {
            scan_obj->last_successful_scan_time = scan_time;
        }
        
        sqlite3_finalize(stmt);
        return scan_obj;
    }
    
    sqlite3_finalize(stmt);
    return nullptr;
}

std::vector<ScanObjectInfo> ScanObject::get_all_scan_objects(bool active_only) {
    std::vector<ScanObjectInfo> results;
    
    std::string sql = "SELECT * FROM scan_objects";
    if (active_only) {
        sql += " WHERE is_active = 1";
    }
    sql += " ORDER BY directory_path";
    
    if (!is_connected_) return results;
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "准备SQL语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return results;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ScanObjectInfo scan_obj;
        scan_obj.id = sqlite3_column_int(stmt, 0);
        scan_obj.directory_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        scan_obj.display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        scan_obj.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        scan_obj.is_active = sqlite3_column_int(stmt, 4);
        scan_obj.is_recursive = sqlite3_column_int(stmt, 5);
        
        const char* scan_time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (scan_time) {
            scan_obj.last_successful_scan_time = scan_time;
        }
        
        results.push_back(scan_obj);
    }
    
    sqlite3_finalize(stmt);
    return results;
}

bool ScanObject::scan_object_exists(const std::string& directory_path) {
    std::filesystem::path dir_path(directory_path);
    std::string absolute_path = std::filesystem::absolute(dir_path).string();
    
    const std::string sql = "SELECT COUNT(*) FROM scan_objects WHERE directory_path = ?";
    std::vector<std::string> params = {absolute_path};
    
    if (!is_connected_) return false;
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "准备SQL语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, absolute_path.c_str(), -1, SQLITE_TRANSIENT);
    
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0) > 0;
    }
    
    sqlite3_finalize(stmt);
    return exists;
}

void ScanObject::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
        is_connected_ = false;
        std::cout << "扫描对象数据库连接已关闭" << std::endl;
    }
}