#pragma once
#include "crow.h"
#include <string>
#include "FileDB.h"

class WebService {
public:
    // 构造函数
    WebService() = default;
    
    // GET /api/scan_obj/{uid} - 获取scan_obj列表
    crow::response get_scan_objs(const std::string& uid);
    
    // POST /api/scan_obj/{uid} - 添加新的scan_obj
    crow::response add_scan_obj(const std::string& uid, const crow::request& req);
    
    // DELETE /api/scan_obj/{uid}/{id} - 删除指定id的表项
    crow::response delete_scan_obj(const std::string& uid, const std::string& id);

    // GET /api/filedb/{uid}/{search_text} - 获取scan_obj列表
    crow::response get_filedb_objs(const std::string& uid, const std::string& search_text);

    // POST /api/filedb/{uid}/task/{search_text} - 创建查找任务，获取task_id
    crow::response create_search_task(const std::string& uid, const std::string& search_text);

    // GET /api/filedb/{uid}/task/{task_id} - 获取查找任务，获取task_id的一部分查找结果，与查找状态
    crow::response get_search_task(const std::string& uid, const std::string& task_id);

    // DELETE /api/filedb/{uid}/task/{task_id} - 删除查找任务
    crow::response delete_search_task(const std::string& uid, const std::string& task_id);

    // POST /api/audit/events - 处理audit消息
    crow::response audit_event(const crow::request& req);

private:
    // 数据库操作函数声明（由你实现）
    int db_get_scan_objs(const std::string& uid,
        crow::json::wvalue& result,
        std::string &error_msg);

    bool db_add_scan_obj(const std::string& uid, 
        const std::string& path, 
        const std::string& description,
        crow::json::wvalue& result, 
        std::string &error_msg);

    bool db_delete_scan_obj(const std::string& uid, 
        const std::string& id, 
        crow::json::wvalue& result,
        std::string &error_msg);

    int db_get_filedb_objs(const std::string& uid, 
        const std::string& search_text, 
        crow::json::wvalue& result,
        std::string &error_msg);

    std::string db_create_search_task(const std::string& uid, 
        const std::string& decoded_search_text,
        int &max_file_count,
        std::string &error_msg);

    int db_get_search_task(const std::string& uid,
        const std::string& task_id,
        crow::json::wvalue& result,
        bool &is_finished,
        std::string &error_msg);

    void db_delete_search_task(const std::string& uid,
        const std::string& task_id,
        std::string &error_msg);

    std::shared_ptr<FileDB> get_db(const std::string& uid);

    std::mutex db_map_mutex_;
    std::map<std::string, std::shared_ptr<FileDB>> db_map_;
};