#include "stdafx.h"
#include "ScanObjDialog.h"
#include "Defines.h"
#include <unistd.h>

ScanObjDialog::ScanObjDialog(QWidget* parent) 
    : QDialog(parent)
    , network_manager_(new QNetworkAccessManager(this))
{
    setWindowTitle("管理扫描目录");
    setFixedSize(700, 400);
    
    setupUI();
    loadScanObjects();
    
    // 连接网络请求的信号槽
    connect(network_manager_, &QNetworkAccessManager::finished, 
            this, &ScanObjDialog::onScanObjectsLoaded);
}

ScanObjDialog::~ScanObjDialog()
{
}

void ScanObjDialog::setupUI()
{
    auto layout = new QVBoxLayout(this);
    
    // 表格显示区域
    table_widget_ = new QTableWidget(this);
    table_widget_->setColumnCount(2);
    table_widget_->setHorizontalHeaderLabels(QStringList() << "描述" << "路径");
    table_widget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_widget_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_widget_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    table_widget_->verticalHeader()->setVisible(false);
    
    // 设置表格样式
    table_widget_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    table_widget_->horizontalHeader()->setStretchLastSection(true);
    table_widget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    table_widget_->setColumnWidth(0, 250);
    
    // 添加新的扫描对象区域
    auto add_group_layout = new QVBoxLayout();
    auto add_label = new QLabel("添加新的扫描目录:", this);
    
    // 目录选择
    auto dir_layout = new QHBoxLayout();
    auto dir_label = new QLabel("目录路径:", this);
    dir_input_ = new QLineEdit(this);
    dir_input_->setPlaceholderText("选择或输入目录路径...");
    auto browse_btn = new QPushButton("浏览", this);
    connect(browse_btn, &QPushButton::clicked, this, &ScanObjDialog::browseDirectory);
    
    dir_layout->addWidget(dir_label);
    dir_layout->addWidget(dir_input_);
    dir_layout->addWidget(browse_btn);
    
    // 描述输入
    auto desc_layout = new QHBoxLayout();
    auto desc_label = new QLabel("描述:", this);
    desc_input_ = new QLineEdit(this);
    desc_input_->setPlaceholderText("输入目录描述...");
    
    desc_layout->addWidget(desc_label);
    desc_layout->addWidget(desc_input_);
    
    // 按钮区域
    auto btn_layout = new QHBoxLayout();
    add_btn_ = new QPushButton("添加", this);
    delete_btn_ = new QPushButton("删除选中", this);
    refresh_btn_ = new QPushButton("刷新", this);
    cancel_btn_ = new QPushButton("关闭", this);
    
    connect(add_btn_, &QPushButton::clicked, this, &ScanObjDialog::addScanObject);
    connect(delete_btn_, &QPushButton::clicked, this, &ScanObjDialog::deleteScanObject);
    connect(refresh_btn_, &QPushButton::clicked, this, &ScanObjDialog::loadScanObjects);
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    
    btn_layout->addWidget(add_btn_);
    btn_layout->addWidget(delete_btn_);
    btn_layout->addWidget(refresh_btn_);
    btn_layout->addStretch();
    btn_layout->addWidget(cancel_btn_);
    
    // 组装布局
    add_group_layout->addWidget(add_label);
    add_group_layout->addLayout(dir_layout);
    add_group_layout->addLayout(desc_layout);
    
    layout->addWidget(table_widget_);
    layout->addLayout(add_group_layout);
    layout->addLayout(btn_layout);
}

void ScanObjDialog::browseDirectory()
{
    QString directory = QFileDialog::getExistingDirectory(this, "选择目录");
    if (!directory.isEmpty()) {
        dir_input_->setText(directory);
    }
}

void ScanObjDialog::addScanObject()
{
    QString directory = dir_input_->text().trimmed();
    QString description = desc_input_->text().trimmed();
    
    if (directory.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入目录路径");
        return;
    }
    
    addScanObjectInternal(directory, description);
}

void ScanObjDialog::addScanObjectInternal(const QString& path, const QString& desc)
{
    // 准备JSON数据
    QJsonObject scanObj;
    scanObj["directory_path"] = path;
    scanObj["description"] = desc;
    scanObj["is_active"] = 1;
    scanObj["is_recursive"] = 1;
    
    QJsonDocument doc(scanObj);
    QByteArray data = doc.toJson();
    
    qDebug() << "发送添加请求，数据:" << data;
    
    // 发送POST请求
    QString uid = getCurrentUserUid();
    QUrl url(QString("%1/api/scan_obj/%2").arg(SERVER_URL).arg(uid));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 临时断开通用finished信号，连接特定的添加完成信号
    disconnect(network_manager_, &QNetworkAccessManager::finished, 
               this, &ScanObjDialog::onScanObjectsLoaded);
    connect(network_manager_, &QNetworkAccessManager::finished, 
            this, &ScanObjDialog::onScanObjectAdded);
    
    network_manager_->post(request, data);
}

void ScanObjDialog::deleteScanObject()
{
    int currentRow = table_widget_->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "选择错误", "请先选择要删除的目录");
        return;
    }
    
    // 从item data中获取ID
    int id = table_widget_->item(currentRow, 0)->data(Qt::UserRole).toInt();
    QString directory = table_widget_->item(currentRow, 0)->text();
    
    int ret = QMessageBox::question(this, "确认删除", 
                                   QString("确定要删除目录:\n%1\n吗？").arg(directory),
                                   QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        QString uid = getCurrentUserUid();
        QUrl url(QString("%1/api/scan_obj/%2/%3").arg(SERVER_URL).arg(uid).arg(id));
        QNetworkRequest request(url);
        
        // 临时断开通用finished信号，连接特定的删除完成信号
        disconnect(network_manager_, &QNetworkAccessManager::finished, 
                   this, &ScanObjDialog::onScanObjectsLoaded);
        connect(network_manager_, &QNetworkAccessManager::finished, 
                this, &ScanObjDialog::onScanObjectDeleted);
        
        network_manager_->deleteResource(request);
    }
}

void ScanObjDialog::loadScanObjects()
{
    QString uid = getCurrentUserUid();
    QUrl url(QString("%1/api/scan_obj/%2").arg(SERVER_URL).arg(uid));
    QNetworkRequest request(url);
    
    // 确保连接到正确的槽函数
    disconnect(network_manager_, &QNetworkAccessManager::finished, 
               this, &ScanObjDialog::onScanObjectAdded);
    disconnect(network_manager_, &QNetworkAccessManager::finished, 
               this, &ScanObjDialog::onScanObjectDeleted);
    connect(network_manager_, &QNetworkAccessManager::finished, 
            this, &ScanObjDialog::onScanObjectsLoaded);
    
    network_manager_->get(request);
}

void ScanObjDialog::onScanObjectsLoaded(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response_data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response_data);
        
        qDebug() << "获取扫描对象响应:" << response_data;
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString result = obj["result"].toString();
            
            if (result == "ok") {
                // 清空表格
                table_widget_->setRowCount(0);

                int count = obj["count"].toInt();

                if (count == 0) {
                    if (QMessageBox::question(this, "提示", "没有扫描目录，是否添加默认目录？", 
                                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                        addScanObjectInternal(QDir::homePath(), "主目录");
                    }
                } else if (obj.contains("scan_objs") && obj["scan_objs"].isArray()) {
                    QJsonArray array = obj["scan_objs"].toArray();
                    for (const QJsonValue& value : array) {
                        if (value.isObject()) {
                            QJsonObject scanObj = value.toObject();
                            int id = scanObj["id"].toInt();
                            QString directory = scanObj["directory_path"].toString();
                            QString description = scanObj["description"].toString();
                            
                            int row = table_widget_->rowCount();
                            table_widget_->insertRow(row);
                            
                            // 描述列
                            QTableWidgetItem* desc_item = new QTableWidgetItem(description);
                            table_widget_->setItem(row, 0, desc_item);

                            // 目录路径列
                            QTableWidgetItem* path_item = new QTableWidgetItem(directory);
                            path_item->setData(Qt::UserRole, id);
                            table_widget_->setItem(row, 1, path_item);
                            

                        }
                    }
                }
                
                if (table_widget_->rowCount() == 0) {
                    table_widget_->setRowCount(1);
                    table_widget_->setItem(0, 0, new QTableWidgetItem("暂无扫描目录"));
                    table_widget_->setItem(0, 1, new QTableWidgetItem(""));
                }
                
            } else {
                // 业务逻辑错误
                QString errorMsg = obj["message"].toString();
                QMessageBox::warning(this, "加载失败", 
                                   QString("加载扫描目录列表失败: %1").arg(errorMsg));
            }
        } else {
            //QMessageBox::warning(this, "响应格式错误", "服务器返回的数据格式不正确");
        }
    } else {
        // 网络错误
        QMessageBox::warning(this, "网络错误", 
                           QString("网络请求失败: %1").arg(reply->errorString()));
    }
    
    reply->deleteLater();
}

void ScanObjDialog::onScanObjectAdded(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response_data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response_data);
        
        qDebug() << "添加扫描对象响应:" << response_data;
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString result = obj["result"].toString();
            
            if (result == "ok") {
                QMessageBox::information(this, "成功", "扫描目录添加成功");
                dir_input_->clear();
                desc_input_->clear();
                loadScanObjects(); // 重新加载列表
            } else {
                QString errorMsg = obj["message"].toString();
                QMessageBox::warning(this, "添加失败", 
                                   QString("添加扫描目录失败: %1").arg(errorMsg));
            }
        } else {
            QMessageBox::warning(this, "响应格式错误", "服务器返回的数据格式不正确");
        }
    } else {
        QMessageBox::warning(this, "网络错误", 
                           QString("网络请求失败: %1").arg(reply->errorString()));
    }
    
    reply->deleteLater();
}

void ScanObjDialog::onScanObjectDeleted(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response_data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response_data);
        
        qDebug() << "删除扫描对象响应:" << response_data;
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString result = obj["result"].toString();
            
            if (result == "ok") {
                QMessageBox::information(this, "成功", "扫描目录删除成功");
                loadScanObjects(); // 重新加载列表
            } else {
                QString errorMsg = obj["message"].toString();
                QMessageBox::warning(this, "删除失败", 
                                   QString("删除扫描目录失败: %1").arg(errorMsg));
            }
        } else {
            QMessageBox::warning(this, "响应格式错误", "服务器返回的数据格式不正确");
        }
    } else {
        QMessageBox::warning(this, "网络错误", 
                           QString("网络请求失败: %1").arg(reply->errorString()));
    }
    
    reply->deleteLater();
}

QString ScanObjDialog::getCurrentUserUid() const
{
    return QString::number(getuid());
}