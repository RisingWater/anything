#include "stdafx.h"
#include "FileScannerManager.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <ctime>
#include "Utils.h"

FileScannerManager& FileScannerManager::getInstance() {
    static FileScannerManager instance;
    return instance;
}

FileScannerManager::~FileScannerManager() {
    stop_all_ = true;
    queue_cv_.notify_all();

    {
        std::lock_guard<std::mutex> lock(scanners_mutex_);
        for (auto& [key, scanner] : scanners_) {
            if (scanner) {
                scanner->close();
            }
        }
        scanners_.clear();
    }

    if (scan_worker_thread_.joinable()) {
        scan_worker_thread_.join();
    }

    if (scheduled_rescan_thread_.joinable()) {
        scheduled_rescan_thread_.join();
    }
}

FileScannerManager::ScannerKey FileScannerManager::generateKey(
    const std::string& db_path,
    const std::string& directory_path) const {

    std::stringstream ss;
    ss << db_path << "##" << directory_path;

    return ss.str();
}

bool FileScannerManager::initializeAllScanners() {
    std::cout << "初始化所有文件扫描器..." << std::endl;

    // 启动扫描工作线程
    scan_worker_thread_ = std::thread(&FileScannerManager::scanWorker, this);

    // 从数据库加载扫描配置
    if (!loadScanConfigurations()) {
        std::cerr << "加载扫描配置失败" << std::endl;
        return false;
    }

    // 如果没有扫描器，尝试从备份恢复
    if (scanners_.empty()) {
        restoreScanConfigurations();
    }

    // 将所有扫描器加入队列
    {
        std::lock_guard<std::mutex> lock(scanners_mutex_);
        for (auto& [key, scanner] : scanners_) {
            if (scanner) {
                enqueueScan(scanner.get(), true);
            }
        }
    }

    std::cout << "已加入 " << scanners_.size() << " 个扫描任务到队列" << std::endl;

    startScheduledRescan();

    return true;
}

bool FileScannerManager::loadScanConfigurations() {
    auto db_list = get_all_db_path();

    for (auto& db_path : db_list) {
        ScanObject scan_object(db_path);
        if (!scan_object.init_database()) {
            std::cerr << "初始化数据库失败: " << db_path << std::endl;
            continue;
        }

        auto scan_objects = scan_object.get_all_scan_objects();

        for (auto& scan_object : scan_objects) {
            addScanner(db_path, scan_object.directory_path);
        }
    }

    return true;
}

bool FileScannerManager::addScanner(const std::string& db_path,
                                  const std::string& directory_path) {
    ScannerKey key = generateKey(db_path, directory_path);

    std::lock_guard<std::mutex> lock(scanners_mutex_);

    if (scanners_.find(key) != scanners_.end()) {
        std::cout << "扫描器已存在: " << key << std::endl;
        return false;
    }

    try {
        auto scanner = std::make_unique<FileScanner>(
            directory_path,
            db_path
        );

        scanners_[key] = std::move(scanner);
        std::cout << "成功添加扫描器: " << key << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "创建扫描器失败: " << e.what() << std::endl;
        return false;
    }
}

bool FileScannerManager::removeScanner(const std::string& db_path,
                                     const std::string& directory_path) {
    ScannerKey key = generateKey(db_path, directory_path);

    std::lock_guard<std::mutex> lock(scanners_mutex_);

    auto it = scanners_.find(key);
    if (it == scanners_.end()) {
        std::cout << "扫描器不存在: " << key << std::endl;
        return false;
    }

    if (it->second) {
        it->second->close();
    }

    scanners_.erase(it);
    std::cout << "成功移除扫描器: " << key << std::endl;
    return true;
}

bool FileScannerManager::startScanner(const std::string& db_path,
                                    const std::string& directory_path) {
    ScannerKey key = generateKey(db_path, directory_path);

    std::lock_guard<std::mutex> lock(scanners_mutex_);

    auto it = scanners_.find(key);
    if (it == scanners_.end() || !it->second) {
        std::cout << "扫描器不存在: " << key << std::endl;
        return false;
    }

    enqueueScan(it->second.get(), true);

    std::cout << "扫描器已加入队列: " << key << std::endl;
    return true;
}

bool FileScannerManager::stopScanner(const std::string& db_path,
                                   const std::string& directory_path) {
    ScannerKey key = generateKey(db_path, directory_path);

    std::lock_guard<std::mutex> lock(scanners_mutex_);

    auto it = scanners_.find(key);
    if (it == scanners_.end() || !it->second) {
        std::cout << "扫描器不存在: " << key << std::endl;
        return false;
    }

    it->second->close();
    std::cout << "停止扫描器: " << key << std::endl;
    return true;
}

void FileScannerManager::onFileChange(const std::string& path, const std::string& type)
{
    std::lock_guard<std::mutex> lock(scanners_mutex_);

    for (auto it = scanners_.begin(); it != scanners_.end(); it++) {
        if (it->second->directory_match(path)) {
            if (it->second->on_file_changed(path, type)) {
                break;
            }
        }
    }
}

void FileScannerManager::enqueueScan(FileScanner* scanner, bool start_watcher)
{
    // 调用者需持有 scanners_mutex_ 或保证 scanner 指针有效
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        scan_queue_.push({scanner, start_watcher});
    }
    queue_cv_.notify_one();
}

void FileScannerManager::scanWorker()
{
    while (!stop_all_) {
        std::pair<FileScanner*, bool> task = {nullptr, false};
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !scan_queue_.empty() || stop_all_;
            });

            if (stop_all_ && scan_queue_.empty())
                break;

            if (!scan_queue_.empty()) {
                task = scan_queue_.front();
                scan_queue_.pop();
            }
        }

        if (task.first) {
            std::cout << "开始执行扫描任务..." << std::endl;
            if (task.second) {
                task.first->run();
            } else {
                task.first->scan_directory();
            }
            std::cout << "扫描任务执行完成" << std::endl;
        }
    }
}

void FileScannerManager::restoreScanConfigurations()
{
    std::ifstream backup(SCAN_OBJECTS_BACKUP_FILE);
    if (!backup.is_open()) {
        std::cout << "没有找到扫描配置备份文件，跳过恢复" << std::endl;
        return;
    }

    std::string line;
    int restored = 0;
    while (std::getline(backup, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string uid, dir_path, display_name, description, is_recursive_str;
        std::getline(ss, uid, '|');
        std::getline(ss, dir_path, '|');
        std::getline(ss, display_name, '|');
        std::getline(ss, description, '|');
        std::getline(ss, is_recursive_str, '|');

        if (uid.empty() || dir_path.empty()) continue;

        // 检查目录是否仍然存在
        if (!std::filesystem::exists(dir_path)) {
            std::cout << "目录不存在，跳过恢复: " << dir_path << std::endl;
            continue;
        }

        std::string db_path = get_db_path_by_uid(uid);

        // 确保数据库目录存在
        std::filesystem::path db_dir = std::filesystem::path(db_path).parent_path();
        if (!std::filesystem::exists(db_dir)) {
            std::filesystem::create_directories(db_dir);
        }

        // 写入 scan_objects 表
        ScanObject scan_obj(db_path);
        if (scan_obj.init_database()) {
            scan_obj.add_scan_object(dir_path, display_name, description,
                                     is_recursive_str == "1");
        }

        // 创建扫描器
        addScanner(db_path, dir_path);
        restored++;
    }

    if (restored > 0) {
        std::cout << "从备份恢复了 " << restored << " 个扫描目录" << std::endl;
        // 恢复后删除备份文件
        std::filesystem::remove(SCAN_OBJECTS_BACKUP_FILE);
    }
}

void FileScannerManager::startScheduledRescan()
{
    scheduled_rescan_thread_ = std::thread([this]() {
        while (!stop_all_) {
            // 从配置文件读取重扫时间，默认 00:00
            std::string schedule_time = get_rescan_schedule_time();
            int target_hour = std::stoi(schedule_time.substr(0, 2));
            int target_min = std::stoi(schedule_time.substr(3, 2));

            // 计算距离下一个目标时间的秒数
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm now_tm;
            localtime_r(&now_time_t, &now_tm);

            std::tm target_tm = now_tm;
            target_tm.tm_hour = target_hour;
            target_tm.tm_min = target_min;
            target_tm.tm_sec = 0;

            auto target_time_t = std::mktime(&target_tm);
            auto target = std::chrono::system_clock::from_time_t(target_time_t);

            if (target <= now) {
                target_tm.tm_mday += 1;
                target_time_t = std::mktime(&target_tm);
                target = std::chrono::system_clock::from_time_t(target_time_t);
            }

            auto wait_seconds = std::chrono::duration_cast<std::chrono::seconds>(target - now).count();

            std::cout << "下次定时重扫将在 " << wait_seconds << " 秒后（"
                      << schedule_time << "）执行" << std::endl;

            while (wait_seconds > 0 && !stop_all_) {
                auto sleep_time = std::min<int64_t>(wait_seconds, 60);
                std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
                wait_seconds -= sleep_time;
            }

            if (stop_all_)
                break;

            std::cout << "=== 定时重扫开始（" << schedule_time << "） ===" << std::endl;

            // 将所有扫描器加入队列（不需要启动 watcher）
            {
                std::lock_guard<std::mutex> lock(scanners_mutex_);
                for (auto& [key, scanner] : scanners_) {
                    if (scanner) {
                        enqueueScan(scanner.get(), false);
                    }
                }
            }

            std::cout << "=== 定时重扫任务已加入队列 ===" << std::endl;
        }
    });
}
