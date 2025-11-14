#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <linux/audit.h>
#include <libaudit.h>

class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    bool startWatching(const std::string& directory_path);
    void stopWatching();
    
private:
    bool setupAudit();
    void cleanupAudit();
    
    bool addAuditRule(const std::string& directory_path);
    bool deleteAuditRule();
    
    std::string directory_path_;
    
    int audit_fd_{-1};
    int rule_id_{-1};
};

#endif // FILEWATCHER_H