#ifndef MAINGUI_H
#define MAINGUI_H

#include <QMainWindow>
#include <QThread>
#include <QAtomicInt>
#include <QString>
#include <QDialog>
#include <QList>
#include <QTimer>
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
    
private slots:
    void showScanObjDialog();
    void onSearchTextChanged(const QString& text);
    void performSearch();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onSearchFinished(QNetworkReply* reply);
    
private:
    void setupUI();
    void setupMenu();
    void setupTrayIcon();
    void checkScanObjects();
    void displaySearchResults(const QList<QVariantMap>& results);
    
    QString api_url_;
    
    // UI组件
    QLineEdit* search_input_;
    FileResultTable* result_table_;
    QLabel* status_label_;
    QTimer* search_timer_;

    QSystemTrayIcon* trayIcon_;
};

#endif // MAINGUI_H