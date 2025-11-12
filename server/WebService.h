#pragma once
#include "crow.h"
#include <string>

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

private:
    // 数据库操作函数声明（由你实现）
    bool db_get_scan_objs(const std::string& uid,
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

    bool db_get_filedb_objs(const std::string& uid, 
        const std::string& search_text, 
        crow::json::wvalue& result,
        std::string &error_msg);
};