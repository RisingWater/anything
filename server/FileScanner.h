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
#include "FileWatcher.h"

class FileScanner {
public:
    // 进度回调类型
    using ProgressCallback = std::function<void(const std::string&)>;

    FileScanner(const std::string& directory_path, 
                const std::string& db_path = "file_scanner.db",
                const std::unordered_set<std::string>& excluded_patterns = {});
    ~FileScanner();

    // 禁止拷贝
    FileScanner(const FileScanner&) = delete;

    FileScanner& operator=(const FileScanner&) = delete;

    bool directory_match(const std::string &file_path);

    bool scan_directory();
    
    bool run();
    
    void close();

    bool on_file_changed(const std::string& path, const std::string& event_type);
    
private:
    bool should_rescan();
    bool scan_directory_recursive(const std::string& current_dir, std::unordered_set<std::string>& visited_paths);
    bool scan_single_directory(const std::string& directory_path);
    bool should_exclude_directory(const std::filesystem::path& dir_path);
    bool is_path_contains_excluded_directory(const std::filesystem::path& file_path);
    void scan_new_directory_recursive(const std::string& directory_path);
    
    std::unique_ptr<FileInfo> get_file_info(const std::filesystem::path& file_path);
    std::unique_ptr<FileInfo> get_directory_info(const std::filesystem::path& dir_path);
    std::string get_mime_type(const std::filesystem::path& file_path);
    
    void start_file_watcher();
    void stop_file_watcher();
    
    static std::string get_current_time();
    static double get_current_timestamp();
    
    std::string directory_path_;
    std::string db_path_;
    std::unordered_set<std::string> excluded_patterns_;
    
    std::unique_ptr<FileDB> file_db_;
    std::unique_ptr<ScanObject> scan_obj_;
    
    std::thread watcher_thread_;
    std::atomic<bool> is_watching_;
    
    static const std::unordered_set<std::string> DEFAULT_EXCLUDED_DIRS;

    int total_file_count_;

    std::unique_ptr<FileWatcher> file_watcher_;
};

#endif // FILESCANNER_H