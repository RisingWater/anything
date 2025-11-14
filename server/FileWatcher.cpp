#include "FileWatcher.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

FileWatcher::FileWatcher() {
}

FileWatcher::~FileWatcher() {
    stopWatching();
}

bool FileWatcher::setupAudit() {
    audit_fd_ = audit_open();
    if (audit_fd_ < 0) {
        std::cerr << "Failed to open audit socket: " << strerror(errno) << std::endl;
        return false;
    }

    return true;
}

void FileWatcher::cleanupAudit() {
    if (rule_id_ != -1) {
        deleteAuditRule();
    }
    
    if (audit_fd_ != -1) {
        audit_close(audit_fd_);
        audit_fd_ = -1;
    }
}

bool FileWatcher::addAuditRule(const std::string& directory_path) {
    // 创建审计规则
    struct audit_rule_data *rule = NULL;
    
    // 添加目录监控规则
    if (audit_add_dir(&rule, directory_path.c_str()) != 0) {
        std::cerr << "Failed to create audit rule for directory: " << directory_path << std::endl;
        return false;
    }
    
    // 设置监控权限：写、属性更改
    if (audit_update_watch_perms(rule, AUDIT_PERM_WRITE | AUDIT_PERM_ATTR) != 0) {
        std::cerr << "Failed to update watch permissions" << std::endl;
        free(rule);
        return false;
    }
    
    // 添加规则到内核
    rule_id_ = audit_add_rule_data(audit_fd_, rule, AUDIT_FILTER_EXIT, AUDIT_ALWAYS);
    if (rule_id_ <= 0) {
        if (errno == EEXIST) {
            // 规则已存在，这不算错误
            std::cout << "Audit rule already exists, continuing..." << std::endl;
            free(rule);
            return true;
        } else {
            std::cerr << "Failed to add audit rule: " << strerror(errno) << std::endl;
            free(rule);
            return false;
        }
    }
    
    free(rule);
    return true;
}

bool FileWatcher::deleteAuditRule() {
    if (rule_id_ <= 0) return true;
    
    // 创建相同的规则用于删除
    struct audit_rule_data *rule = NULL;
    if (audit_add_dir(&rule, directory_path_.c_str()) != 0) {
        return false;
    }
    
    // 删除规则
    int ret = audit_delete_rule_data(audit_fd_, rule, AUDIT_FILTER_EXIT, AUDIT_ALWAYS);
    free(rule);
    
    if (ret != 0) {
        std::cerr << "Failed to delete audit rule: " << strerror(errno) << std::endl;
        return false;
    }
    
    rule_id_ = -1;
    return true;
}

bool FileWatcher::startWatching(const std::string& directory_path) {
    directory_path_ = directory_path;

    // 设置 audit
    if (!setupAudit()) {
        return false;
    }

    // 添加审计规则
    if (!addAuditRule(directory_path_)) {
        cleanupAudit();
        return false;
    }

    std::cout << "Started watching directory: " << directory_path_ << std::endl;

    return true;
}

void FileWatcher::stopWatching() {
    cleanupAudit();
    std::cout << "Stopped watching directory: " << directory_path_ << std::endl;
}
