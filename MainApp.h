#ifndef MAINGUI_H
#define MAINGUI_H

#include <QMainWindow>
#include <QThread>
#include <QAtomicInt>
#include <QString>
#include <QDialog>
#include <QList>
#include <QTimer>
#include "ScanThread.h"
#include "FileResultTable.h"
#include <QSystemTrayIcon>

// 前向声明
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class QProgressBar;
class QStackedLayout;
class QVBoxLayout;
class QHBoxLayout;
class QDialog;


// 添加目录对话框
class AddDirectoryDialog : public QDialog
{
    Q_OBJECT
    
public:
    AddDirectoryDialog(QWidget* parent = nullptr);
    QString getDirectory() const;
    
private slots:
    void browseDirectory();
    
private:
    QLineEdit* dir_input_;
};

// 主窗口
class FileSearchApp : public QMainWindow
{
    Q_OBJECT
    
public:
    FileSearchApp(QWidget* parent = nullptr);
    ~FileSearchApp();
    
protected:
    void closeEvent(QCloseEvent* event) override;
    
private slots:
    void showAddDirectoryDialog();
    void addScanDirectory(const QString& directory_path);
    void startScan(const QString& directory_path);
    void refreshAllScans();
    void updateScanProgress(const QString& message);
    void onScanFinished(bool success, const QString& message);
    void onSearchTextChanged(const QString& text);
    void performSearch();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    
private:
    void setupUI();
    void setupMenu();
    void setupTrayIcon();
    void checkScanObjects();
    void displaySearchResults(const QList<QVariantMap>& results);
    QString formatSize(qint64 size_bytes) const;
    
    QString db_path_;
    QList<ScanThread*> scan_threads_;
    
    // UI组件
    QLineEdit* search_input_;
    FileResultTable* result_table_;
    QLabel* status_label_;
    QWidget* empty_widget_;
    QTimer* search_timer_;

    QSystemTrayIcon* trayIcon_;
};

#endif // MAINGUI_H