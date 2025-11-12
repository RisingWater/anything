#ifndef SCANTHREAD_H
#define SCANTHREAD_H

#include <QThread>
#include <QString>
#include <QList>
#include "FileScanner.h"

// 扫描线程
class ScanThread : public QThread
{
    Q_OBJECT
    
public:
    ScanThread(const QString& directory_path, const QString& db_path, QObject* parent = nullptr);
    ~ScanThread();
    
signals:
    void progressSignal(const QString& message);
    void finishedSignal(bool success, const QString& message);
    
protected:
    void run() override;
    
private:
    QString directory_path_;
    QString db_path_;

    std::shared_ptr<FileScanner> scanner_;
};

#endif