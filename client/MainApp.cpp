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

    networkManager_ = new QNetworkAccessManager(this);

    search_timer_->setSingleShot(true);  // 单次触发
    search_timer_->setInterval(500);     // 500毫秒延迟
    connect(search_timer_, &QTimer::timeout, this, &FileSearchApp::performSearch);

    currentId_ = 0;
    currentMaxId_ = 0;
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

    connect(this, &FileSearchApp::refreshSearchResults, this, &FileSearchApp::refreshSearchResultsSlot);
    
    search_layout->addWidget(search_input_);
    search_layout->setContentsMargins(5, 0, 5, 0); // 移除边距
    
    // 搜索结果列表
    result_table_ = new FileResultTable(this);
    
    // 主内容区域
    auto main_layout = new QVBoxLayout();
    main_layout->addWidget(result_table_);
    main_layout->setContentsMargins(5, 0, 5, 0); // 移除边距
    
    auto status_layout = new QHBoxLayout();
    // 状态栏
    status_label_ = new QLabel("就绪", this);
    // 进度条
    progress_bar_ = new QProgressBar(this);
    progress_bar_->setFixedWidth(400);  // 设置固定宽度
    progress_bar_->setFixedHeight(20);
    progress_bar_->setVisible(false);   // 默认隐藏，搜索时显示
    progress_bar_->setRange(0, 100);    // 设置范围0-100
    progress_bar_->setTextVisible(true); // 显示百分比文本
    progress_bar_->setAlignment(Qt::AlignCenter); // 文本居中
    progress_bar_->setVisible(false);

    status_layout->addWidget(status_label_);
    status_layout->addStretch();
    status_layout->addWidget(progress_bar_);
    status_layout->setContentsMargins(5, 0, 5, 0); // 移除边距
    
    // 添加到主布局
    layout->addLayout(search_layout);
    layout->addLayout(main_layout);
    layout->addLayout(status_layout);
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

void FileSearchApp::performSearch()
{
    QString search_text = search_input_->text().trimmed();
    if (search_text.isEmpty()) {
        result_table_->clearResults();
        status_label_->setText("就绪");
        return;
    }

    // 如果已经有搜索在进行，先取消
    if (isSearching_ && !currentTaskId_.isEmpty()) {
        cancelCurrentSearch();
    }

    qDebug() << "开始新的搜索: " << search_text;
    
    try {
        // 使用新的分批次搜索API
        QString uid = QString::number(getuid());
        QString encoded_search_text = QUrl::toPercentEncoding(search_text);

        // 清空表格并设置搜索关键词
        result_table_->setSearchKeyword(search_text.toStdString());
        currentSearchText_ = search_text;
        
        // 创建搜索任务
        QUrl url(QString("%1/api/filedb/%2/task/%3")
                 .arg(SERVER_URL)
                 .arg(uid)
                 .arg(encoded_search_text));
        
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        
        QNetworkReply* reply = networkManager_->post(request, QByteArray());
        
        // 连接完成信号
        connect(reply, &QNetworkReply::finished, this, 
                [this, reply]() {
            onCreateTaskFinished(reply);
        });

        isSearching_ = true;        
        status_label_->setText("正在创建搜索任务...");        
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", QString("搜索失败: %1").arg(e.what()));
        status_label_->setText("搜索失败");
    }
}

void FileSearchApp::clearSearchStatus()
{
    isSearching_ = false;
    currentTaskId_.clear();
    currentId_ = 0;
    currentMaxId_ = 0;
    progress_bar_->setVisible(false);
}

void FileSearchApp::showSearchStatus(const QString& task_id, int max_file_count)
{
    currentTaskId_ = task_id;
    currentId_ = 0;
    currentMaxId_ = max_file_count;
    progress_bar_->setVisible(true);
}

void FileSearchApp::updateSearchStatus(int add_file_count)
{
    currentId_ += add_file_count;
    if (currentId_ > currentMaxId_)
        currentId_ = currentMaxId_;

    progress_bar_->setValue(currentId_ * 100 / currentMaxId_);
}

void FileSearchApp::cancelCurrentSearch()
{    
    if (!currentTaskId_.isEmpty() && isSearching_) {
        // 发送DELETE请求取消任务
        QString uid = QString::number(getuid());
        QString encoded_task_id = QUrl::toPercentEncoding(currentTaskId_);
        
        QUrl url(QString("%1/api/filedb/%2/task/%3")
                 .arg(SERVER_URL)
                 .arg(uid)
                 .arg(encoded_task_id));
        
        QNetworkRequest request(url);
        QNetworkAccessManager manager;
        QNetworkReply* reply = manager.deleteResource(request);
        
        // 可以异步处理取消，不需要等待结果
        reply->deleteLater();
    }
    
    clearSearchStatus();
    status_label_->setText("搜索已取消");
}

void FileSearchApp::onCreateTaskFinished(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response_data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response_data);
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString result = obj["result"].toString();
            
            if (result == "ok") {
                // 获取任务ID
                currentTaskId_ = obj["task_id"].toString();
                
                qDebug() << "创建搜索任务成功, task_id:" << currentTaskId_;

                currentMaxId_ = obj["max_file_count"].toInt();
                
                //启动定时器获取下一批
                showSearchStatus(currentTaskId_, currentMaxId_);
                status_label_->setText("搜索中...");
                //开始获取
                emit refreshSearchResults(currentTaskId_);
                
            } else {
                QString errorMsg = obj["message"].toString();
                QMessageBox::warning(this, "创建任务失败", errorMsg);
                status_label_->setText("创建任务失败");
                clearSearchStatus();
            }
        } else {
            QMessageBox::warning(this, "错误", "服务器返回的数据格式不正确");
            status_label_->setText("数据格式错误");
            clearSearchStatus();
        }
    } else {
        QMessageBox::warning(this, "网络错误", 
                           QString("网络请求失败: %1").arg(reply->errorString()));
        status_label_->setText("网络错误");
        clearSearchStatus();
    }
    
    reply->deleteLater();
}

void FileSearchApp::refreshSearchResultsSlot(QString task_id)
{
    if (currentTaskId_.isEmpty() || !isSearching_ || currentTaskId_ != task_id)
    {
        qDebug() << "任务已取消或已切换任务";
        return;
    }
    
    QString uid = QString::number(getuid());
    QString encoded_task_id = QUrl::toPercentEncoding(currentTaskId_);
    
    QUrl url(QString("%1/api/filedb/%2/task/%3")
             .arg(SERVER_URL)
             .arg(uid)
             .arg(encoded_task_id));
    
    QNetworkRequest request(url);
    
    QNetworkReply* reply = networkManager_->get(request);

    qDebug() << "开始获取下一批结果, task_id:" << currentTaskId_;
    
    connect(reply, &QNetworkReply::finished, this, 
            [this, reply]() {
        onFetchBatchFinished(reply);
        reply->deleteLater();
    });
}

void FileSearchApp::onFetchBatchFinished(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError)
    {
        qDebug() << "获取下一批结果成功, task_id:" << currentTaskId_;

        QByteArray response_data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response_data);
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString result = obj["result"].toString();
            QString task_id = obj["task_id"].toString();

            if (task_id != currentTaskId_) {
                //不是当前查找的task
                qDebug() << "任务已取消或已切换任务";
                return;
            }
            
            if (result == "ok") {
                updateSearchStatus(100000);
                bool is_finished = obj["is_finished"].toBool();
                // 处理批数据
                QJsonArray files_array = obj["filedb_objs"].toArray();
                if (!files_array.isEmpty()) {
                    processBatchResults(files_array);
                    
                    // 更新状态显示
                    int current_count = result_table_->rowCount();
                    status_label_->setText(
                        QString("已找到 %1 个文件")
                        .arg(current_count)
                    );
                }
                
                // 如果搜索完成，停止定时器
                if (is_finished) {
                    qDebug() << "搜索完成, task_id:" << task_id;
                    clearSearchStatus();
                    int displayed_count = result_table_->rowCount();
                    
                    if (displayed_count == 0) {
                        status_label_->setText("没有找到匹配的文件");
                    } else {
                        status_label_->setText(QString("搜索完成，共找到 %1 个文件").arg(displayed_count));
                    }
                }
                else
                {
                    qDebug() << "搜索未完成, task_id:" << task_id;
                    //开启下一个批次
                    emit refreshSearchResults(task_id);
                }
            } else {
                QString errorMsg = obj["message"].toString();
                qDebug() << "获取批次失败:" << errorMsg;
                clearSearchStatus();
                status_label_->setText("获取数据失败");
            }
        }
    } else {
        qDebug() << "网络错误:" << reply->errorString();
        clearSearchStatus();
        status_label_->setText("网络错误");
    }
}

void FileSearchApp::processBatchResults(const QJsonArray& files_array)
{
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

            qDebug() << "文件路径:" << result["file_path"];
                       
            search_results.append(result);
        }
    }
    
    // 增量添加到表格
    if (!search_results.isEmpty()) {
        result_table_->addSearchResults(search_results);
    }
}

void FileSearchApp::setSearchKeyword(const std::string& keyword)
{
    result_table_->setSearchKeyword(keyword);
}

void FileSearchApp::addSearchResults(const QList<QVariantMap>& results)
{
    result_table_->addSearchResults(results);
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
