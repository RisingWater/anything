#include "stdafx.h"
#include "MainApp.h"
#include <QApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QCloseEvent>
#include <QFont>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDebug>
#include "ScanObjDialog.h"

FileSearchApp::FileSearchApp(QWidget* parent) :
    QMainWindow(parent),
    search_timer_(new QTimer(this))
{
    setupUI();
    setupMenu();
    setupTrayIcon();

    search_timer_->setSingleShot(true);  // 单次触发
    search_timer_->setInterval(500);     // 500毫秒延迟
    connect(search_timer_, &QTimer::timeout, this, &FileSearchApp::performSearch);
}

FileSearchApp::~FileSearchApp() {
    // 停止定时器
    if (search_timer_->isActive()) {
        search_timer_->stop();
    }
}

void FileSearchApp::setupUI() {
    setWindowTitle("Anything");
    setMinimumSize(1366, 768);
    setWindowIcon(QIcon(":/res/anything.png"));
    
    // 中央部件
    auto central_widget = new QWidget(this);
    setCentralWidget(central_widget);
    
    auto layout = new QVBoxLayout(central_widget);
    
    // 搜索栏
    auto search_layout = new QHBoxLayout();
    search_input_ = new QLineEdit(this);
    search_input_->setPlaceholderText("输入文件名、路径或扩展名进行搜索...");
    connect(search_input_, &QLineEdit::textChanged, this, &FileSearchApp::onSearchTextChanged);
    connect(search_input_, &QLineEdit::returnPressed, this, &FileSearchApp::performSearch);
    
    search_layout->addWidget(search_input_);
    
    // 搜索结果列表
    result_table_ = new FileResultTable(this);
    
    // 主内容区域
    auto main_content = new QWidget(this);
    auto main_layout = new QVBoxLayout(main_content);
    main_layout->addWidget(result_table_);
    
    // 状态栏
    status_label_ = new QLabel("就绪", this);
    
    // 添加到主布局
    layout->addLayout(search_layout);
    layout->addWidget(main_content);
    layout->addWidget(status_label_);
}

void FileSearchApp::setupMenu() {
    auto menubar = menuBar();
    
    // 文件菜单
    auto file_menu = menubar->addMenu("文件");
    
    auto add_dir_action = new QAction("配置扫描目录", this);
    connect(add_dir_action, &QAction::triggered, this, &FileSearchApp::showScanObjDialog);
    file_menu->addAction(add_dir_action);
}

void FileSearchApp::showScanObjDialog() {
    ScanObjDialog* dialog = new ScanObjDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->exec();
}

void FileSearchApp::onSearchTextChanged(const QString& text) {
    // 停止之前的定时器（如果正在运行）
    if (search_timer_->isActive()) {
        search_timer_->stop();
    }
    
    if (text.length() >= 2) {
        // 重新启动定时器，500毫秒后执行搜索
        search_timer_->start();
    } else if (text.isEmpty()) {
        // 如果搜索框为空，立即清除结果
        search_timer_->stop();
        result_table_->clearResults();
        status_label_->setText("就绪");
    }
}

void FileSearchApp::performSearch() {
    QString search_text = search_input_->text().trimmed();
    if (search_text.isEmpty()) {
        result_table_->clearResults();
        status_label_->setText("就绪");
        return;
    }

    qDebug() << "searchText: " << search_text;
    
    try {
        // 使用 webapi
        QString uid = QString::number(getuid());
        QString encoded_search_text = QUrl::toPercentEncoding(search_text);
        
        QUrl url(QString("%1/api/filedb/%2/%3").arg(SERVER_URL).arg(uid).arg(encoded_search_text));
        QNetworkRequest request(url);
        
        QNetworkAccessManager* manager = new QNetworkAccessManager(this);
        QNetworkReply* reply = manager->get(request);
        
        // 连接完成信号
        connect(reply, &QNetworkReply::finished, this, [this, reply, manager]() {
            onSearchFinished(reply);
            manager->deleteLater();
        });
        
        status_label_->setText("搜索中...");
        
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", QString("搜索失败: %1").arg(e.what()));
    }
}

void FileSearchApp::onSearchFinished(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response_data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response_data);
        
        qDebug() << "搜索响应:" << response_data;
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString result = obj["result"].toString();
            
            if (result == "ok") {
                int count = obj["count"].toInt();
                QJsonArray files_array = obj["filedb_objs"].toArray();
                
                QList<QVariantMap> search_results;
                
                for (const QJsonValue& value : files_array) {
                    if (value.isObject()) {
                        QJsonObject file_obj = value.toObject();
                        QVariantMap result;
                        result["file_path"] = file_obj["file_path"].toString();
                        result["file_name"] = file_obj["file_name"].toString();
                        result["file_extension"] = file_obj["file_extension"].toString();
                        result["mime_type"] = file_obj["mime_type"].toString();
                        result["is_directory"] = file_obj["is_directory"].toBool();
                        result["id"] = file_obj["id"].toInt();
                        
                        search_results.append(result);
                    }
                }
                
                displaySearchResults(search_results);
                status_label_->setText(QString("找到 %1 个结果").arg(count));
                
            } else {
                QString errorMsg = obj["message"].toString();
                QMessageBox::warning(this, "搜索失败", errorMsg);
                status_label_->setText("搜索失败");
            }
        } else {
            QMessageBox::warning(this, "错误", "服务器返回的数据格式不正确");
            status_label_->setText("数据格式错误");
        }
    } else {
        QMessageBox::warning(this, "网络错误", 
                           QString("网络请求失败: %1").arg(reply->errorString()));
        status_label_->setText("网络错误");
    }
    
    reply->deleteLater();
}

void FileSearchApp::displaySearchResults(const QList<QVariantMap>& results) {
    result_table_->setSearchResults(results);
}

void FileSearchApp::closeEvent(QCloseEvent* event) {
    hide();
    event->accept();
}

void FileSearchApp::setupTrayIcon() {
    // 创建系统托盘图标
    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setIcon(QIcon(":/res/anything.png")); // 设置程序图标
    trayIcon_->setToolTip("Anything");
    
    // 创建托盘右键菜单
    QMenu* trayMenu = new QMenu(this);
    
    // 显示/隐藏动作
    QAction* showHideAction = new QAction("显示/隐藏", this);
    connect(showHideAction, &QAction::triggered, this, [this]() {
        if (isHidden()) {
            show();
            activateWindow();
        } else {
            hide();
        }
    });
    
    // 完全退出动作
    QAction* quitAction = new QAction("完全退出", this);
    connect(quitAction, &QAction::triggered, this, []() {
        qApp->quit();
    });
    
    // 添加到菜单
    trayMenu->addAction(showHideAction);
    trayMenu->addSeparator(); // 添加分隔线
    trayMenu->addAction(quitAction);
    
    // 设置托盘菜单
    trayIcon_->setContextMenu(trayMenu);
    trayIcon_->show();
    
    // 连接托盘图标点击事件
    connect(trayIcon_, &QSystemTrayIcon::activated, this, &FileSearchApp::onTrayIconActivated);
}

void FileSearchApp::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        if (isHidden()) {
            show();
            activateWindow();
        } else {
            hide();
        }
    }
}
