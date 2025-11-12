#ifndef FILESCANNER_H
#define FILESCANNER_H

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <filesystem>
#include "FileDB.h"
#include "ScanObject.h"

class FileScanner {
public:
    // 进度回调类型
    using ProgressCallback = std::function<void(const std::string&)>;

    FileScanner(const std::string& directory_path, 
                const std::string& db_path = "file_scanner.db",
                const std::unordered_set<std::string>& excluded_patterns = {});
    ~FileScanner();

    // 设置进度回调
    void setProgressCallback(ProgressCallback callback);
    
    // 禁止拷贝
    FileScanner(const FileScanner&) = delete;
    FileScanner& operator=(const FileScanner&) = delete;
    
    bool scan_directory(bool db_operation = true);
    bool run();
    void close();
    
private:
    bool should_rescan();
    bool scan_directory_recursive(const std::string& current_dir, bool db_operation, std::unordered_set<std::string>& visited_paths);
    bool scan_single_directory(const std::string& directory_path, bool db_operation);
    bool should_exclude_directory(const std::filesystem::path& dir_path);
    
    std::unique_ptr<FileInfo> get_file_info(const std::filesystem::path& file_path);
    std::unique_ptr<FileInfo> get_directory_info(const std::filesystem::path& dir_path);
    std::string get_mime_type(const std::filesystem::path& file_path);
    
    void start_file_watcher();
    void stop_file_watcher();
    void handle_file_changes();
    
    static std::string get_current_time();
    static double get_current_timestamp();

    void report_progress(const std::string& message);
    
    std::string directory_path_;
    std::string db_path_;
    std::unordered_set<std::string> excluded_patterns_;
    
    std::unique_ptr<FileDB> file_db_;
    std::unique_ptr<ScanObject> scan_obj_;
    
    std::thread watcher_thread_;
    std::atomic<bool> stop_watching_;
    
    static const std::unordered_set<std::string> DEFAULT_EXCLUDED_DIRS;

    ProgressCallback progress_callback_;

    int total_file_count_;
};

#endif // FILESCANNER_H