#include "stdafx.h"
#include "crow_all.h"
#include <iomanip>

//#define INSTALL_PATH "/opt/apps/com.anything"
#define INSTALL_PATH "/workdir" //test path
#define DATABASE_FILE_PATH INSTALL_PATH "/files/db"
#define TARGET_DB_FILE "file_db.db"

struct DBInfo {
    std::string uid;
    std::string dbPath;
};

std::vector<DBInfo> checkDatabaseDirectories() {
    std::vector<DBInfo> results;
    
    std::filesystem::path basePath(DATABASE_FILE_PATH);
    
    // 检查基础目录是否存在
    if (!std::filesystem::exists(basePath)) {
        std::cerr << "错误: 基础目录不存在 - " << basePath << std::endl;
        return results;
    }
    
    // 遍历基础目录下的所有条目
    for (const auto& entry : fs::directory_iterator(basePath)) {
        if (entry.is_directory()) {
            DBInfo result;
            result.uid = entry.path.filename();
            result.dbPath = entry.path() / TARGET_DB_FILE;
            if(fs::exists(result.dbPath)) {
                results.push_back(result);
            }
        }
    }
    
    return results;
}

int main(int argc, char** argv)
{
    //std::vector<DBInfo> results = checkAllSubdirectories();

    
    return 0;
}