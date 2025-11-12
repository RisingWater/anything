#include "ScanThread.h"

ScanThread::ScanThread(const QString& directory_path, const QString& db_path, QObject* parent)
    : QThread(parent), directory_path_(directory_path), db_path_(db_path) {
    scanner_ = std::make_shared<FileScanner>(directory_path_.toStdString(), db_path_.toStdString());

    // 使用lambda表达式设置进度回调
    scanner_->setProgressCallback([this](const std::string& message) {
            // 将std::string转换为QString并发射信号
            emit progressSignal(QString::fromStdString(message));
    });
}

ScanThread::~ScanThread()
{
    scanner_->close();
}

void ScanThread::run() {
    try {
        emit progressSignal(QString("开始扫描: %1").arg(directory_path_));
        
        bool success = scanner_->scan_directory();
        
        if (success) {
            emit finishedSignal(true, QString("扫描完成: %1").arg(directory_path_));
        } else {
            emit finishedSignal(false, QString("扫描失败: %1").arg(directory_path_));
        }
        
    } catch (const std::exception& e) {
        emit finishedSignal(false, QString("扫描错误: %1").arg(e.what()));
    }
}
