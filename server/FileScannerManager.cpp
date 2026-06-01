#include "stdafx.h"
#include "FileScannerManager.h"
#include <iostream>
#include <sstream>
#include <ctime>
#include "Utils.h"

FileScannerManager& FileScannerManager::getInstance() {
    static FileScannerManager instance;
    return instance;
}

FileScannerManager::~FileScannerManager() {
    stop_all_ = true;

    // 停止所有扫描器
    {
        std::lock_guard<std::mutex> lock(scanners_mutex_);
        for (auto& [key, scanner] : scanners_) {
            if (scanner) {
                scanner->close();
            }
        }
        scanners_.clear();
    }

    // 等待所有线程结束
    for (auto& thread : scanner_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
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
    
    // 从数据库加载扫描配置
    if (!loadScanConfigurations()) {
        std::cerr << "加载扫描配置失败" << std::endl;
        return false;
    }
    
    // 启动所有扫描器
    std::lock_guard<std::mutex> lock(scanners_mutex_);
    for (auto& [key, scanner] : scanners_) {
        if (scanner) {
            // 在新线程中启动扫描器
            scanner_threads_.emplace_back([scanner_ptr = scanner.get()]() {
                std::cout << "启动文件扫描器..." << std::endl;
                scanner_ptr->run();
            });
        }
    }
    
    std::cout << "已启动 " << scanner_threads_.size() << " 个文件扫描器" << std::endl;

    startScheduledRescan();

    return true;
}

bool FileScannerManager::loadScanConfigurations() {
    auto db_list = get_all_db_path();

    for (auto& db_path : db_list) {
        // 从数据库中加载扫描配置
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
        // 创建新的扫描器
        auto scanner = std::make_unique<FileScanner>(
            directory_path, 
            db_path
            // 可以在这里添加排除模式
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
    
    // 在新线程中启动扫描器
    scanner_threads_.emplace_back([scanner_ptr = it->second.get()]() {
        scanner_ptr->run();
    });
    
    std::cout << "启动扫描器: " << key << std::endl;
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

            // 如果今天的目标时间已过，则改为明天
            if (target <= now) {
                target_tm.tm_mday += 1;
                target_time_t = std::mktime(&target_tm);
                target = std::chrono::system_clock::from_time_t(target_time_t);
            }

            auto wait_seconds = std::chrono::duration_cast<std::chrono::seconds>(target - now).count();

            std::cout << "下次定时重扫将在 " << wait_seconds << " 秒后（"
                      << schedule_time << "）执行" << std::endl;

            // 分段 sleep，以便能响应 stop_all_
            while (wait_seconds > 0 && !stop_all_) {
                auto sleep_time = std::min<int64_t>(wait_seconds, 60);
                std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
                wait_seconds -= sleep_time;
            }

            if (stop_all_)
                break;

            std::cout << "=== 定时重扫开始（" << schedule_time << "） ===" << std::endl;

            // 收集所有扫描器指针
            std::vector<FileScanner*> scanner_ptrs;
            {
                std::lock_guard<std::mutex> lock(scanners_mutex_);
                for (auto& [key, scanner] : scanners_) {
                    if (scanner) {
                        scanner_ptrs.push_back(scanner.get());
                    }
                }
            }

            for (auto* scanner : scanner_ptrs) {
                if (stop_all_)
                    break;
                scanner->scan_directory();
            }

            std::cout << "=== 定时重扫完成 ===" << std::endl;
        }
    });
}

