
#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>

std::string get_db_path_by_uid(std::string uid);

std::vector<std::string> get_all_db_path();

// 读取定时重扫配置，返回 "HH:MM"，默认 "00:00"
std::string get_rescan_schedule_time();

#endif