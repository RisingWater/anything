#include "stdafx.h"
#include "Utils.h"

//#define INSTALL_PATH "/opt/apps/com.anything"
#define INSTALL_PATH "/workdir" //test path
#define DATABASE_FILE_PATH INSTALL_PATH "/files/db"
#define TARGET_DB_FILE "file_db.db"

std::string get_db_path_by_uid(std::string uid)
{
    return std::string(DATABASE_FILE_PATH) + "/" + uid + "/" + std::string(TARGET_DB_FILE);
}
