#ifndef FILEDB_H
#define FILEDB_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "sqlite3.h"
#include <chrono>
#include "DBManager.h"

// 搜索任务状态
enum class SearchStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    CANCELLED,
    ERROR
};

// 搜索任务结构
struct SearchTask {
    std::string task_id;
    std::string search_term;
    std::string search_field;
    int total_results = 0;
    int limit = 0;                     // 总限制
    SearchStatus status = SearchStatus::PENDING;
    std::string pattern;
    std::chrono::system_clock::time_point created_time;
    
    // ID范围相关
    int current_min_id = 1;            // 当前查询的起始ID
    int max_id = 0;                    // 最大ID（用于判断结束）
    bool include_hidden = false;        // 是否包含隐藏文件夹
};

struct FileInfo {
    int id;
    std::string file_path;
    std::string file_name;
    std::string modified_time;
    std::string created_time;
    std::string file_extension;
    std::string mime_type;
    int is_directory;
    std::string parent_directory;
    std::string last_scanned_time;
    int scan_count;
};

class FileDB {
public:
    FileDB(const std::string& db_path = "file_scanner.db");
    ~FileDB();
    
    // 禁止拷贝
    FileDB(const FileDB&) = delete;
    FileDB& operator=(const FileDB&) = delete;
    
    bool init_database();
    bool insert_file(const FileInfo& file_info);
    bool update_file(const std::string& file_path, const FileInfo& file_info);
    bool delete_file(const std::string& file_path);
    bool delete_files_by_directory(const std::string& directory_path);
    bool delete_files_by_path_prefix(const std::string& path_prefix);

    bool begin_transaction();
    bool commit_transaction();
    bool rollback_transaction();
    
    std::unique_ptr<FileInfo> get_file(const std::string& file_path);
    bool file_exists(const std::string& file_path);
    
    std::vector<FileInfo> search_files(const std::string& search_term, 
                                      const std::string& search_field = "file_name",
                                      int limit = -1);
    
    std::vector<FileInfo> get_files_by_parent_directory(const std::string& parent_directory);
    bool batch_delete_files(const std::vector<std::string>& file_paths);
    
    std::unordered_map<std::string, int> get_database_stats();
    bool clear_database();
    void close();
    
    // 工具函数
    static std::string get_current_time();

    std::string start_search_task(const std::string& search_term,
                                 const std::string& search_field,
                                 int& max_file_count,
                                 int limit = -1,
                                 bool include_hidden = false);

    SearchStatus get_task_status(const std::string& task_id);

    bool cancel_search_task(const std::string& task_id);
    
    void cleanup_task(const std::string& task_id);

    std::vector<FileInfo> get_search_batch(const std::string& task_id, 
                                          int batch_size = 100000);

private:
    // 批量操作结构
    struct BatchOperation {
        std::string sql;
        std::vector<std::string> params;
    };
    
    sqlite3_stmt* get_prepared_statement(const std::string& sql);
    void cleanup_prepared_statements();

    bool execute_sql(const std::string& sql);
    bool execute_sql_with_params(const std::string& sql, 
                                const std::vector<std::string>& params);

    int get_max_id();

    DBConnection* db_conn_;
    std::string db_path_;
    mutable std::mutex operation_mutex_; // 用于操作级别的线程安全
    bool is_connected_;
    
    int transaction_depth_ = 0;

    std::unordered_map<std::string, sqlite3_stmt*> prepared_statements_;

    std::unordered_map<std::string, std::unique_ptr<SearchTask>> search_tasks_;
    std::mutex task_mutex_;
    std::atomic<int> next_task_id_{0};
};

#endif // FILEDB_H