#include "stdafx.h"
#include "WebService.h"
#include "ScanObject.h"
#include "FileScannerManager.h"
#include "Utils.h"
#include <iostream>
#include <cassert>
#include <string>

static unsigned char FromHex(unsigned char x) {
    if (x >= 'A' && x <= 'Z') return x - 'A' + 10;
    if (x >= 'a' && x <= 'z') return x - 'a' + 10;
    if (x >= '0' && x <= '9') return x - '0';
    assert(false); // Invalid character
    return 0;
}

static std::string UrlDecode(const std::string& str) {
    std::string decoded;
    size_t length = str.length();
    for (size_t i = 0; i < length; ++i) {
        if (str[i] == '+') {
            decoded += ' ';
        } else if (str[i] == '%') {
            // 修复1：检查是否有足够的字符
            if (i + 2 >= length) {
                // 不是完整的 %XX 格式，按原样输出
                decoded += str[i];
                continue;
            }
            
            // 修复2：检查是否是有效的十六进制字符
            unsigned char char1 = str[i+1];
            unsigned char char2 = str[i+2];
            
            if (!((char1 >= '0' && char1 <= '9') || (char1 >= 'A' && char1 <= 'F') || (char1 >= 'a' && char1 <= 'f')) ||
                !((char2 >= '0' && char2 <= '9') || (char2 >= 'A' && char2 <= 'F') || (char2 >= 'a' && char2 <= 'f'))) {
                // 不是有效的十六进制字符，按原样输出
                decoded += str[i];
                continue;
            }
            
            unsigned char high = FromHex(char1);
            unsigned char low = FromHex(char2);
            decoded += high * 16 + low;
            i += 2; // 跳过两个十六进制字符
        } else {
            decoded += str[i];
        }
    }
    return decoded;
}

// 设置 CORS 头部的辅助函数
static void set_cors_headers(crow::response& res) {
    res.add_header("Access-Control-Allow-Origin", "*");
    res.add_header("Access-Control-Allow-Headers", "Content-Type");
    res.add_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
}

// 创建错误响应
static crow::response create_error_response(const std::string& message) {
    crow::response res;
    res.code = 200;

    crow::json::wvalue error;
    error["result"] = "error";
    error["message"] = message;
    set_cors_headers(res);
    res.write(error.dump());

    std::cout << message << std::endl;
    return res;
}

// GET /api/scan_obj/{uid} - 获取scan_obj列表
crow::response WebService::get_scan_objs(const std::string& uid) {
    crow::response res;
    crow::json::wvalue result;
    std::string error_msg;
    
    int count = db_get_scan_objs(uid, result, error_msg);

    // 成功获取数据
    crow::json::wvalue response;
    response["result"] = "ok";
    response["count"] = count;
    response["scan_objs"] = std::move(result);
    set_cors_headers(res);
    res.code = 200;
    res.write(response.dump());
    
    return res;
}

// POST /api/scan_obj/{uid} - 添加新的scan_obj
crow::response WebService::add_scan_obj(const std::string& uid, const crow::request& req) {
    try {
        // 解析 JSON 请求体
        auto json = crow::json::load(req.body);
        if (!json) {
            return create_error_response("Invalid JSON");
        }
        
        // 验证必需字段
        if (!json.has("directory_path") || !json.has("description")) {
            return create_error_response("Missing required fields: 'directory_path' and 'description'");
        }
        
        std::string path = json["directory_path"].s();
        std::string description = json["description"].s();
        
        crow::response res;
        crow::json::wvalue result;
        std::string error_msg;
        
        if (db_add_scan_obj(uid, path, description, result, error_msg)) {
            // 成功添加数据
            crow::json::wvalue response;
            response["result"] = "ok";
            response["scan_obj"] = std::move(result);
            set_cors_headers(res);
            res.code = 200;
            res.write(response.dump());
        } else {
            // 添加数据失败
            res = create_error_response(std::string("Failed to add scan object, error message: ") + error_msg);
        }
        
        return res;
        
    } catch (const std::exception& e) {
        return create_error_response(std::string("Internal error: ") + e.what());
    }
}

// DELETE /api/scan_obj/{uid}/{id} - 删除指定id的表项
crow::response WebService::delete_scan_obj(const std::string& uid, const std::string& id) {
    crow::response res;
    crow::json::wvalue result;
    std::string error_msg;
    
    if (db_delete_scan_obj(uid, id, result, error_msg)) {
        // 成功删除数据
        crow::json::wvalue response;
        response["result"] = "ok";
        set_cors_headers(res);
        res.code = 200;
        res.write(response.dump());
    } else {
        // 删除数据失败
        res = create_error_response(std::string("Failed to delete scan object, error message: ") + error_msg);
    }
    
    return res;
}

// GET /api/filedb/{uid}/{search_text} - 获取指定数据库的filedb_objs列表
crow::response WebService::get_filedb_objs(const std::string& uid, const std::string& search_text)
{
    crow::response res;
    crow::json::wvalue result;
    std::string error_msg;

    int count = db_get_filedb_objs(uid, search_text, result, error_msg);
    
    // 获取数据成功
    crow::json::wvalue response;
    response["result"] = "ok";
    response["count"] = count;
    response["filedb_objs"] = std::move(result);
    set_cors_headers(res);
    res.code = 200;
    res.write(response.dump());
    
    return res;
}

// POST /api/audit/events - 处理audit消息
crow::response WebService::audit_event(const crow::request& req)
{
    crow::response res;

    // 添加请求日志
    std::cout << "=== Received audit event request ===" << std::endl;
    std::cout << "URL: " << req.url << std::endl;
    std::cout << "Body: " << req.body << std::endl;
    std::cout << "Content-Type: " << req.get_header_value("Content-Type") << std::endl;
    std::cout << "====================================" << std::endl;

    try {
        // 解析 JSON 请求体
        auto json = crow::json::load(req.body);
        if (!json) {
            return create_error_response("Invalid JSON");
        }
        
        // 验证必需字段
        if (!json.has("path") || !json.has("type")) {
            return create_error_response("Missing required fields: 'path' and 'type'");
        }

        std::string path = json["path"].s();
        std::string type = json["type"].s();

        FileScannerManager::getInstance().onFileChange(path, type);

        crow::json::wvalue response;
        response["result"] = "ok";
        set_cors_headers(res);
        res.code = 200;
        res.write(response.dump());

        return res;

    } catch (const std::exception& e) {
        return create_error_response(std::string("Internal error: ") + e.what());
    }

}

// 数据库操作函数 - 需要你来实现这些函数
int WebService::db_get_scan_objs(const std::string& uid, crow::json::wvalue& result, std::string &error_msg) {
    
    std::string db_path = get_db_path_by_uid(uid);
    result = crow::json::wvalue();

    if (!std::filesystem::exists(db_path)) {
        return 0;
    }

    // 数据库文件存在，开始处理
    ScanObject scan_object(db_path);

    if (!scan_object.init_database()) {
        return 0;
    }

    // 数据库初始化成功，开始处理
    std::vector<ScanObjectInfo> scan_objects = scan_object.get_all_scan_objects();

    int index = 0;
    for (const auto& object : scan_objects) {
        crow::json::wvalue scan_object_json;
        scan_object_json["id"] = object.id;
        scan_object_json["directory_path"] = object.directory_path;
        scan_object_json["display_name"] = object.display_name;
        scan_object_json["description"] = object.description;
        scan_object_json["is_active"] = object.is_active;
        scan_object_json["is_recursive"] = object.is_recursive;
        scan_object_json["last_successful_scan_time"] = object.last_successful_scan_time;
        result[index++] = std::move(scan_object_json);
    }

    return index;
}

bool WebService::db_add_scan_obj(const std::string& uid, const std::string& path, 
                                const std::string& description, crow::json::wvalue& result, std::string& error_msg) {
    
    std::string db_path = get_db_path_by_uid(uid);
    // 确保数据库目录存在
    std::filesystem::path db_dir = std::filesystem::path(db_path).parent_path();
    if (!std::filesystem::exists(db_dir)) {
        std::filesystem::create_directories(db_dir);  // 创建目录（包括父目录）
    }

    // 现在处理数据库操作
    ScanObject scan_object(db_path);
    if (!scan_object.init_database()) {
        error_msg = "Failed to initialize database.";
        return false;
    }

    // 使用 std::filesystem::path 来获取文件名
    std::filesystem::path fs_path(path);
    std::string display_name = fs_path.filename().string();  // 正确获取文件名

    // 添加扫描对象到数据库
    bool success = scan_object.add_scan_object(path, display_name, description);
    if (!success) {
        error_msg = "Failed to add scan object to database.";
        return false;
    }

    auto object = scan_object.get_scan_object(path);

    if (!object) {
        error_msg = "Failed to retrieve added scan object from database.";
        return false;
    }

    result["id"] = object->id;
    result["directory_path"] = object->directory_path;
    result["display_name"] = object->display_name;
    result["description"] = object->description;
    result["is_active"] = object->is_active;
    result["is_recursive"] = object->is_recursive;
    result["last_successful_scan_time"] = object->last_successful_scan_time;

    FileScannerManager::getInstance().addScanner(db_path, path);
    FileScannerManager::getInstance().startScanner(db_path, path);

    return true;
}

bool WebService::db_delete_scan_obj(const std::string& uid, const std::string& id, 
                                   crow::json::wvalue& result, std::string& error_msg) {
    std::string db_path = get_db_path_by_uid(uid);
    // 现在处理数据库操作
    ScanObject scan_object(db_path);
    if (!scan_object.init_database()) {
        error_msg = "Failed to initialize database.";
        return false;
    }

    auto object = scan_object.get_scan_object_by_id(id);

    if (object == nullptr) {
        error_msg = "Scan object not found.";
        return false;
    }

    FileScannerManager::getInstance().removeScanner(db_path, object->directory_path);

    scan_object.delete_scan_object(id);

    return true;
}

int WebService::db_get_filedb_objs(const std::string& uid, const std::string& search_text, 
                                   crow::json::wvalue& result, std::string& error_msg) {
    std::string db_path = get_db_path_by_uid(uid);
    result = crow::json::wvalue();

    if (!std::filesystem::exists(db_path)) {
        return 0;
    }

    // 现在处理数据库操作
    FileDB filedb(db_path);
    if (!filedb.init_database()) {
        return 0;
    }

    // 使用 URL 解码
    std::string decoded_search_text = UrlDecode(search_text);
    std::cout << "原始搜索文本: " << search_text << std::endl;
    std::cout << "解码后搜索文本: " << decoded_search_text << std::endl;

    int index = 0;
    std::vector<FileInfo> files = filedb.search_files(decoded_search_text, "file_name");
    for (const auto& file : files) {
        crow::json::wvalue file_json;
        file_json["id"] = file.id;
        file_json["file_name"] = file.file_name;
        file_json["file_path"] = file.file_path;
        file_json["file_extension"] = file.file_extension;
        file_json["mime_type"] = file.mime_type;
        file_json["is_directory"] = file.is_directory;

        result[index++] = std::move(file_json);
    }

    return index;
}
                                 