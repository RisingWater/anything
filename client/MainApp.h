#ifndef MAINGUI_H
#define MAINGUI_H

#include <QMainWindow>
#include <QThread>
#include <QAtomicInt>
#include <QString>
#include <QDialog>
#include <QList>
#include <QTimer>
#include <QProgressBar>
#include "FileResultTable.h"
#include <QSystemTrayIcon>
#include <QNetworkAccessManager>

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

// 主窗口
class FileSearchApp : public QMainWindow
{
    Q_OBJECT
    
public:
    FileSearchApp(QWidget* parent = nullptr);
    ~FileSearchApp();
    
protected:
    void closeEvent(QCloseEvent* event) override;

signals:
    void refreshSearchResults(QString task_id);

public slots:
    void onActivateRequested();

private slots:
    void showScanObjDialog();
    void onSearchTextChanged(const QString& text);
    void performSearch();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onCreateTaskFinished(QNetworkReply* reply);
    void onFetchBatchFinished(QNetworkReply* reply);
    void cancelCurrentSearch();
    void refreshSearchResultsSlot(QString task_id);
    void onScanObjectsLoaded(QNetworkReply* reply);

private:
    void setupUI();
    void setupMenu();
    void setupTrayIcon();
    void checkScanObjects();
    void setSearchKeyword(const std::string& keyword);
    void addSearchResults(const QList<QVariantMap>& results);
    void processBatchResults(const QJsonArray& files_array);

    void loadScanObjects();
    void addScanObjectInternal(const QString& path, const QString& desc);

    void clearSearchStatus();    
    void showSearchStatus(const QString& task_id, int max_file_count);
    void updateSearchStatus(int current_file_count);
    QString api_url_;
    bool isSearching_;
    
    // UI组件
    QLineEdit* search_input_;
    FileResultTable* result_table_;
    QLabel* status_label_;
    QProgressBar* progress_bar_;
    QTimer* search_timer_;

    QSystemTrayIcon* trayIcon_;
    QString currentSearchText_;
    QString currentTaskId_;

    int currentId_;
    int currentMaxId_;

    QNetworkAccessManager* networkManager_;
};

#endif // MAINGUI_H