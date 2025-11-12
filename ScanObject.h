#ifndef SCANOBJECT_H
#define SCANOBJECT_H

#include <string>
#include <vector>
#include <memory>
#include <sqlite3.h>
#include <chrono>
#include <filesystem>

struct ScanObjectInfo {
    int id;
    std::string directory_path;
    std::string display_name;
    std::string description;
    int is_active;
    int is_recursive;
    std::string last_successful_scan_time;
};

class ScanObject {
public:
    ScanObject(const std::string& db_path = "file_scanner.db");
    ~ScanObject();
    
    // 禁止拷贝
    ScanObject(const ScanObject&) = delete;
    ScanObject& operator=(const ScanObject&) = delete;
    
    bool init_database();
    bool add_scan_object(const std::string& directory_path, 
                        const std::string& display_name = "",
                        const std::string& description = "",
                        bool is_recursive = true);
    
    bool update_last_scan_time(const std::string& directory_path);
    
    std::unique_ptr<ScanObjectInfo> get_scan_object(const std::string& directory_path);
    std::vector<ScanObjectInfo> get_all_scan_objects(bool active_only = true);
    bool scan_object_exists(const std::string& directory_path);
    
    void close();

private:
    bool execute_sql(const std::string& sql);
    bool execute_sql_with_params(const std::string& sql, 
                                const std::vector<std::string>& params);
    
    static std::string get_current_time();
    
    sqlite3* db_;
    std::string db_path_;
    bool is_connected_;
};

#endif // SCANOBJECT_H