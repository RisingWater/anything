#ifndef FILESCANNERMANAGER_H
#define FILESCANNERMANAGER_H

#include "FileScanner.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>

class FileScannerManager {
public:
    using ScannerKey = std::string;

    static FileScannerManager& getInstance();

    FileScannerManager(const FileScannerManager&) = delete;
    FileScannerManager& operator=(const FileScannerManager&) = delete;

    bool initializeAllScanners();

    bool addScanner(const std::string& db_path, const std::string& directory_path);
    bool removeScanner(const std::string& db_path, const std::string& directory_path);
    bool startScanner(const std::string& db_path, const std::string& directory_path);
    bool stopScanner(const std::string& db_path, const std::string& directory_path);

    void onFileChange(const std::string& path, const std::string& type);

private:
    FileScannerManager() = default;
    ~FileScannerManager();

    ScannerKey generateKey(const std::string& db_path, const std::string& directory_path) const;

    bool loadScanConfigurations();

    void enqueueScan(FileScanner* scanner, bool start_watcher);
    void scanWorker();
    void restoreScanConfigurations();
    void startScheduledRescan();

    std::unordered_map<ScannerKey, std::unique_ptr<FileScanner>> scanners_;
    mutable std::mutex scanners_mutex_;

    std::queue<std::pair<FileScanner*, bool>> scan_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread scan_worker_thread_;
    std::thread scheduled_rescan_thread_;
    std::atomic<bool> stop_all_{false};
};

#endif // FILESCANNERMANAGER_H