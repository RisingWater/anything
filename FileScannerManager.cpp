#include "stdafx.h"
#include "FileScannerManager.h"
#include <iostream>
#include <sstream>
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


