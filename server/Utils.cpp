#include "stdafx.h"
#include "Utils.h"
#include <filesystem>
#include "Defines.h"

std::string get_db_path_by_uid(std::string uid)
{
    return std::string(DATABASE_FILE_PATH) + "/" + uid + "/" + std::string(TARGET_DB_FILE);
}

std::vector<std::string> get_all_db_path() {
    std::vector<std::string> results;
    
    std::filesystem::path basePath(DATABASE_FILE_PATH);
    
    // 检查基础目录是否存在
    if (!std::filesystem::exists(basePath)) {
        std::cerr << "错误: 基础目录不存在 - " << basePath << std::endl;
        return results;
    }
    
    // 遍历基础目录下的所有条目
    for (const auto& entry : std::filesystem::directory_iterator(basePath)) {
        if (entry.is_directory()) {
            std::string dbPath = entry.path() / TARGET_DB_FILE;
            if(std::filesystem::exists(dbPath)) {
                results.push_back(dbPath);
            }
        }
    }
    
    return results;
}