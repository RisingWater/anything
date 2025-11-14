#ifndef FILESCANNERMANAGER_H
#define FILESCANNERMANAGER_H

#include "FileScanner.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class FileScannerManager {
public:
    using ScannerKey = std::string;
    
    // 单例模式
    static FileScannerManager& getInstance();
    
    // 禁止拷贝
    FileScannerManager(const FileScannerManager&) = delete;
    FileScannerManager& operator=(const FileScannerManager&) = delete;
    
    // 初始化所有扫描器
    bool initializeAllScanners();
    
    // 扫描器管理
    bool addScanner(const std::string& db_path, const std::string& directory_path);
    bool removeScanner(const std::string& db_path, const std::string& directory_path);
    bool startScanner(const std::string& db_path, const std::string& directory_path);
    bool stopScanner(const std::string& db_path, const std::string& directory_path);

    void onFileChange(const std::string& path, const std::string& type);
    
private:
    FileScannerManager() = default;
    ~FileScannerManager();
    
    // 生成扫描器键值
    ScannerKey generateKey(const std::string& db_path, const std::string& directory_path) const;
    
    // 从数据库加载扫描配置
    bool loadScanConfigurations();
    
    std::unordered_map<ScannerKey, std::unique_ptr<FileScanner>> scanners_;
    mutable std::mutex scanners_mutex_;
    
    std::vector<std::thread> scanner_threads_;
    std::atomic<bool> stop_all_{false};
};

#endif // FILESCANNERMANAGER_H