#include "FileScanner.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fnmatch.h>
#include <sys/stat.h>
#include <ctime>

const std::unordered_set<std::string> FileScanner::DEFAULT_EXCLUDED_DIRS = {
    ".git", ".svn", ".hg", ".idea", ".vscode", "__pycache__", "node_modules", ".repo", ".cache"
};

FileScanner::FileScanner(const std::string& directory_path,
                         const std::string& db_path,
                         const std::unordered_set<std::string>& excluded_patterns) : 
    directory_path_(std::filesystem::absolute(directory_path).string()),
    db_path_(db_path),
    excluded_patterns_(excluded_patterns.empty() ? DEFAULT_EXCLUDED_DIRS : excluded_patterns),
    is_watching_(false),
    total_file_count_(0),
    file_watcher_(nullptr) {
    
    file_db_ = std::make_unique<FileDB>(db_path_);
    scan_obj_ = std::make_unique<ScanObject>(db_path_);
    
    std::cout << "文件扫描器初始化: " << directory_path_ << std::endl;
    std::cout << "排除模式: " << std::endl;
    for (const auto& pattern : excluded_patterns_) {
        std::cout << pattern.c_str() << " " << std::endl;
    }
}

FileScanner::~FileScanner() {
    std::cout << "文件扫描器析构: " << directory_path_ << std::endl;
    close();
}

bool FileScanner::directory_match(const std::string &file_path) {
    return file_path.find(directory_path_) == 0;
}

bool FileScanner::should_exclude_directory(const std::filesystem::path& dir_path) {
    std::string dir_name = dir_path.filename().string();
    
    // 检查精确匹配
    if (excluded_patterns_.find(dir_name) != excluded_patterns_.end()) {
        return true;
    }
    
    // 检查通配符匹配
    for (const auto& pattern : excluded_patterns_) {
        if (pattern.find('*') != std::string::npos) {
            if (fnmatch(pattern.c_str(), dir_name.c_str(), 0) == 0) {
                return true;
            }
        }
    }
    
    return false;
}

bool FileScanner::is_path_contains_excluded_directory(const std::filesystem::path& file_path) {
    std::filesystem::path current_path = file_path.parent_path(); // 从父目录开始检查
    
    // 逐级向上检查每个目录组件
    for (const auto& component : current_path) {
        std::string dir_name = component.string();
        
        // 跳过空组件和根目录
        if (dir_name.empty() || dir_name == "/") {
            continue;
        }
        
        // 检查精确匹配
        if (excluded_patterns_.find(dir_name) != excluded_patterns_.end()) {
            return true;
        }
        
        // 检查通配符匹配
        for (const auto& pattern : excluded_patterns_) {
            if (pattern.find('*') != std::string::npos) {
                if (fnmatch(pattern.c_str(), dir_name.c_str(), 0) == 0) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool FileScanner::should_rescan()
{
    auto scan_object = scan_obj_->get_scan_object(directory_path_);
    
    // 如果扫描对象不存在，需要扫描
    if (!scan_object) {
        std::cout << "扫描对象不存在，需要扫描: " << directory_path_ << std::endl;
        return true;
    }
    
    // 如果扫描对象存在但未激活，不需要扫描
    if (!scan_object->is_active) {
        std::cout << "扫描对象未激活，跳过扫描: " << directory_path_ << std::endl;
        return false;
    }
    
    // 检查最后扫描时间
    if (scan_object->last_successful_scan_time.empty()) {
        std::cout << "从未扫描过，需要扫描: " << directory_path_ << std::endl;
        return true;
    }

    return true;
}

bool FileScanner::scan_directory() {
    // 全局符号链接检测集合
    static std::unordered_set<std::string> global_visited_paths;
    global_visited_paths.clear(); // 每次扫描前清空
    
    try {
        // 检查是否需要扫描
        if (!should_rescan()) {
            return true;
        }

        total_file_count_ = 0;
        
        std::cout << "开始扫描目录:" << directory_path_ << std::endl;
        double start_time = get_current_timestamp();
        
        // 确保扫描对象存在
        if (!scan_obj_->scan_object_exists(directory_path_)) {
            scan_obj_->add_scan_object(directory_path_, 
                                     std::filesystem::path(directory_path_).filename().string(),
                                     "", true);
        }

        if (!file_db_->begin_transaction()) {
            return false;
        }
        
        // 递归扫描，传入全局路径集合
        bool success = scan_directory_recursive(directory_path_, global_visited_paths);
        
        // 更新扫描时间
        if (success) {
            file_db_->commit_transaction();
            scan_obj_->update_last_scan_time(directory_path_);
            double scan_duration = get_current_timestamp() - start_time;
            std::cout << "扫描完成:" << directory_path_ << "耗时:" << scan_duration << "秒, 对象数量:" << total_file_count_ << std::endl;
        } else {
            file_db_->rollback_transaction();
            std::cerr << "扫描失败:" << directory_path_ << std::endl;
        }

        return success;
        
    } catch (const std::exception& e) {
        file_db_->rollback_transaction();
        std::cerr << "扫描目录异常:" << directory_path_.c_str() << "错误:" << e.what() << std::endl;
        return false;
    }
}

bool FileScanner::scan_directory_recursive(const std::string& current_dir, std::unordered_set<std::string>& visited_paths) {
    try {
        std::filesystem::path current_path(current_dir);
        
        // 符号链接循环检测
        try {
            auto canonical_path = std::filesystem::canonical(current_path);
            std::string canonical_str = canonical_path.string();
            
            if (visited_paths.count(canonical_str)) {
                std::cerr << "检测到符号链接循环，跳过目录:" << current_dir.c_str() << std::endl;
                return true; // 返回true继续扫描其他目录
            }
            
            visited_paths.insert(canonical_str);
            
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "无法解析规范路径:" << current_dir.c_str() << "错误:" << e.what() << std::endl;
            return false;
        }
        
        // 扫描当前目录的文件
        if (!scan_single_directory(current_dir)) {
            std::cerr << "当前目录扫描失败:" << current_dir.c_str() << std::endl;
            // 不立即返回false，继续尝试其他目录
        }
        
        // 递归扫描子目录
        for (const auto& entry : std::filesystem::directory_iterator(current_path, 
             std::filesystem::directory_options::skip_permission_denied)) {
            
            try {
                if (entry.is_directory()) {
                    // 检查是否应该排除该目录
                    if (should_exclude_directory(entry.path())) {
                        std::cout << "跳过排除目录:" << entry.path().string() << std::endl;
                        continue;
                    }
                    
                    // 对于符号链接目录，进行额外的循环检测
                    if (entry.is_symlink()) {
                        try {
                            auto target_canonical = std::filesystem::canonical(entry.path());
                            if (visited_paths.count(target_canonical.string())) {
                                std::cerr << "跳过符号链接循环:" << entry.path().string() << std::endl;
                                continue;
                            }
                        } catch (const std::filesystem::filesystem_error& e) {
                            std::cerr << "无法解析符号链接:" << entry.path().string() << "错误:" << e.what() << std::endl;
                            continue;
                        }
                    }
                    
                    if (!scan_directory_recursive(entry.path().string(), visited_paths)) {
                        std::cerr << "子目录扫描失败:" << entry.path().string() << std::endl;
                        // 继续扫描其他目录，不立即返回
                    }
                }
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "无法访问目录条目:" << entry.path().string() << "错误:" << e.what() << std::endl;
                continue;
            } catch (const std::exception& e) {
                std::cerr << "处理目录条目异常:" << entry.path().string() << "错误:" << e.what() << std::endl;
                continue;
            }
        }
        
        return true;
        
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "文件系统错误扫描目录:" << current_dir << "错误:" << e.what() << "代码:" << e.code().value() << std::endl;;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "递归扫描异常:" << current_dir << "错误:" << e.what() << std::endl;;
        return false;
    }
}

bool FileScanner::scan_single_directory(const std::string& directory_path) {
    try {
        std::filesystem::path dir_path(directory_path);
        std::vector<FileInfo> existing_files;
        std::unordered_map<std::string, FileInfo> existing_paths;
        
        // 获取数据库中该目录的所有现有文件
        existing_files = file_db_->get_files_by_parent_directory(directory_path);
        for (const auto& file : existing_files) {
            existing_paths[file.file_path] = file;
        }

        // 扫描实际文件
        std::unordered_set<std::string> actual_paths;
        // 记录跳过的目录
        std::unordered_set<std::string> skipped_directories;  

        // 处理目录本身
        auto dir_info = get_directory_info(dir_path);
        if (dir_info) {
            file_db_->insert_file(*dir_info);
            total_file_count_++;
            actual_paths.insert(dir_path.string());
        }
        
        // 处理文件
        for (const auto& entry : std::filesystem::directory_iterator(dir_path, 
             std::filesystem::directory_options::skip_permission_denied)) {
            
            try {
                if (entry.is_regular_file()) {
                    auto file_info = get_file_info(entry.path());
                    if (file_info) {
                        file_db_->insert_file(*file_info);
                        total_file_count_++;
                        actual_paths.insert(entry.path().string());
                    }
                } else if (entry.is_directory()) {
                    // 检查是否应该排除该目录
                    if (should_exclude_directory(entry.path())) {
                        std::cout << "跳过排除目录:" << entry.path().string() << std::endl;
                        skipped_directories.insert(entry.path().string());  // 记录跳过的目录
                        
                        // 立即删除这个跳过目录及其所有子内容
                        if (file_db_->get_file(entry.path().string())) {
                            file_db_->delete_files_by_path_prefix(entry.path().string());
                            file_db_->delete_file(entry.path().string());
                        }

                        continue;
                    }
                    // 目录会在递归中处理，这里只记录路径
                    actual_paths.insert(entry.path().string());
                }
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "无法访问文件条目:" << entry.path().string() << "错误:" << e.what() << std::endl;
                continue;
            } catch (const std::exception& e) {
                std::cerr << "处理文件条目异常:" << entry.path().string() << "错误:" << e.what() << std::endl;
                continue;
            }
        }
        
        // 删除数据库中不存在于实际文件中的记录
        if (!existing_files.empty()) {
            std::vector<std::string> paths_to_delete;
            for (const auto& existing_pair : existing_paths) {
                if (actual_paths.find(existing_pair.first) == actual_paths.end()) {
                    paths_to_delete.push_back(existing_pair.first);
                }
            }
            
            for (const auto& file_path : paths_to_delete) {
                file_db_->delete_file(file_path);
            }
            
            if (paths_to_delete.size() > 0) {
                std::cout << "清理了" << paths_to_delete.size() << "个不存在的文件记录，目录:" << directory_path << std::endl;
            }
        }
        
        return true;
        
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "文件系统错误扫描单个目录:" << directory_path << "错误:" << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "扫描单个目录异常:" << directory_path << "错误:" << e.what() << std::endl;
        return false;
    }
}

std::unique_ptr<FileInfo> FileScanner::get_file_info(const std::filesystem::path& file_path) {
    try {
        //获取最后修改时间，来确定下次扫描的时候是否要跳过这个目录
        auto stat = std::filesystem::status(file_path);
        auto modify_time = std::filesystem::last_write_time(file_path);

        // 跨编译器的文件时间转换
        auto sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            modify_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        
        auto time_t = std::chrono::system_clock::to_time_t(sys_time);

        std::stringstream time_ss;
        time_ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
        
        auto file_info = std::make_unique<FileInfo>();
        file_info->file_path = file_path.string();
        file_info->file_name = file_path.filename().string();
        file_info->modified_time = time_ss.str();
        file_info->created_time = time_ss.str();
        file_info->file_extension = file_path.extension().string();
        file_info->mime_type = get_mime_type(file_path);
        file_info->is_directory = 0;
        file_info->parent_directory = file_path.parent_path().string();
        
        return file_info;
        
    } catch (const std::exception& e) {
        std::cerr << "获取文件信息失败 " << file_path.string().c_str() << ": " << e.what();
        return nullptr;
    }
}

std::unique_ptr<FileInfo> FileScanner::get_directory_info(const std::filesystem::path& dir_path) {
    try {
        //获取最后修改时间，来确定下次扫描的时候是否要跳过这个目录
        auto stat = std::filesystem::status(dir_path);
        auto modify_time = std::filesystem::last_write_time(dir_path);

        // 跨编译器的文件时间转换
        auto sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            modify_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        
        auto time_t = std::chrono::system_clock::to_time_t(sys_time);
        
        std::stringstream time_ss;
        time_ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S");

        auto file_info = std::make_unique<FileInfo>();
        file_info->file_path = dir_path.string();
        file_info->file_name = dir_path.filename().string();
        file_info->modified_time = time_ss.str();
        file_info->created_time = time_ss.str();
        file_info->file_extension = "";
        file_info->mime_type = "inode/directory";
        file_info->is_directory = 1;
        file_info->parent_directory = dir_path.parent_path().string();
        
        return file_info;
        
    } catch (const std::exception& e) {
        std::cerr << "获取目录信息失败 " << dir_path.string().c_str() << ": " << e.what();
        return nullptr;
    }
}

std::string FileScanner::get_mime_type(const std::filesystem::path& file_path) {
    // 简化的MIME类型检测，基于文件扩展名
    std::string extension = file_path.extension().string();
    
    if (extension == ".txt" || extension == ".md") return "text/plain";
    if (extension == ".html" || extension == ".htm") return "text/html";
    if (extension == ".css") return "text/css";
    if (extension == ".js") return "application/javascript";
    if (extension == ".json") return "application/json";
    if (extension == ".xml") return "application/xml";
    if (extension == ".pdf") return "application/pdf";
    if (extension == ".zip") return "application/zip";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".png") return "image/png";
    if (extension == ".gif") return "image/gif";
    
    return "application/octet-stream";
}

void FileScanner::start_file_watcher() {
    if (!file_watcher_) {
        file_watcher_ = std::make_unique<FileWatcher>();
        
        // 启动文件监控
        if (file_watcher_->startWatching(directory_path_)) {
            std::cout << "启动文件监听器: " << directory_path_ << std::endl;
            is_watching_ = true;
        } else {
            std::cerr << "无法启动文件监听器: " << directory_path_ << std::endl;
            file_watcher_.reset();
        }
    }
}

void FileScanner::stop_file_watcher() {
    if (file_watcher_) {
        is_watching_ = false;
        file_watcher_->stopWatching();
        file_watcher_.reset();
        std::cout << "文件监听器已停止" << std::endl;
    }
}

bool FileScanner::on_file_changed(const std::string& path, const std::string& event_type) {
    std::cout << "文件变化: [" << event_type << "] " << path << std::endl;

    if (!is_watching_) {
        return false;
    }

    try {
        // 根据事件类型处理文件变化
        if (event_type == "CREATE") {
            if (!is_path_contains_excluded_directory(path)) {
                auto file_info = get_file_info(path);
                if (file_info) {
                    file_db_->insert_file(*file_info);
                }
            }
        } else if (event_type == "CREATE_DIR") {
            if (!is_path_contains_excluded_directory(path) && !should_exclude_directory(path)) {
                auto dir_info = get_directory_info(path);
                if (dir_info) {
                    file_db_->insert_file(*dir_info);
                }
            }
        } else if (event_type == "DELETE") {
            file_db_->delete_file(path);
        } else if (event_type == "DELETE_DIR") {
            file_db_->delete_files_by_path_prefix(path);
        }
    } catch (const std::exception& e) {
        std::cerr << "处理文件变化异常:" << path << "错误:" << e.what() << std::endl;
    }

    return true;
}

bool FileScanner::run() {
    try {
        // 执行初始扫描
        if (scan_directory()) {
            // 启动文件监听
            start_file_watcher();
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "扫描器运行异常: " << e.what() << std::endl;
        return false;
    }
}

void FileScanner::close() {
    stop_file_watcher();
    if (file_db_) {
        file_db_->close();
    }
    if (scan_obj_) {
        scan_obj_->close();
    }
    std::cout << "文件扫描器已关闭" << std::endl;
}

std::string FileScanner::get_current_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

double FileScanner::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::duration<double>>(now.time_since_epoch()).count();
}
