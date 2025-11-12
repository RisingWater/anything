#include "MainApp.h"
#include "FileDB.h"
#include "ScanObject.h"
#include "FileScanner.h"
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

#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_MAC)
#include <ApplicationServices/ApplicationServices.h>
#else
#include <QProcess>
#endif

AddDirectoryDialog::AddDirectoryDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("添加扫描目录");
    setFixedSize(400, 150);
    
    auto layout = new QVBoxLayout(this);
    
    // 目录选择
    auto dir_layout = new QHBoxLayout();
    auto dir_label = new QLabel("目录:", this);
    dir_input_ = new QLineEdit(this);
    dir_input_->setPlaceholderText("选择或输入目录路径...");
    auto browse_btn = new QPushButton("浏览", this);
    connect(browse_btn, &QPushButton::clicked, this, &AddDirectoryDialog::browseDirectory);
    
    dir_layout->addWidget(dir_label);
    dir_layout->addWidget(dir_input_);
    dir_layout->addWidget(browse_btn);
    
    // 按钮
    auto btn_layout = new QHBoxLayout();
    auto add_btn = new QPushButton("添加", this);
    auto cancel_btn = new QPushButton("取消", this);
    connect(add_btn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    
    btn_layout->addStretch();
    btn_layout->addWidget(add_btn);
    btn_layout->addWidget(cancel_btn);
    
    layout->addLayout(dir_layout);
    layout->addStretch();
    layout->addLayout(btn_layout);
}

void AddDirectoryDialog::browseDirectory() {
    QString directory = QFileDialog::getExistingDirectory(this, "选择目录");
    if (!directory.isEmpty()) {
        dir_input_->setText(directory);
    }
}

QString AddDirectoryDialog::getDirectory() const {
    return dir_input_->text().trimmed();
}

FileSearchApp::FileSearchApp(QWidget* parent) 
    : QMainWindow(parent)
{
    // 获取home目录
    QString homeDir = QDir::homePath();
    
    // 构建完整路径
    QString configPath = homeDir + "/.config/anything";
    QDir dir(configPath);
    if (!dir.exists()) {
        dir.mkpath(".");  // 递归创建目录
    }
    
    //db_path_ = configPath + "/file_scanner.db";
    db_path_ = "file_scanner.db";
    
    qDebug() << "数据库路径:" << db_path_;

    setupUI();
    checkScanObjects();
    setupMenu();
}

FileSearchApp::~FileSearchApp() {
    for (auto thread : scan_threads_) {
        if (thread->isRunning()) {
            thread->wait(5000);
        }
        delete thread;
    }
}

void FileSearchApp::setupUI() {
    setWindowTitle("Anything");
    setMinimumSize(1366, 768);
    
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
    
    auto add_dir_action = new QAction("添加扫描目录", this);
    connect(add_dir_action, &QAction::triggered, this, &FileSearchApp::showAddDirectoryDialog);
    file_menu->addAction(add_dir_action);
    
    auto refresh_action = new QAction("刷新所有扫描", this);
    connect(refresh_action, &QAction::triggered, this, &FileSearchApp::refreshAllScans);
    file_menu->addAction(refresh_action);
    
    file_menu->addSeparator();
    
    auto exit_action = new QAction("退出", this);
    connect(exit_action, &QAction::triggered, this, &QMainWindow::close);
    file_menu->addAction(exit_action);
}

void FileSearchApp::checkScanObjects() {
    try {
        ScanObject scan_obj(db_path_.toStdString());
        auto scan_objects = scan_obj.get_all_scan_objects(true);
        
        if (scan_objects.empty()) {
            QString homeDir = QDir::homePath();
            search_input_->setPlaceholderText("正在创建搜索索引，请稍后...");
            search_input_->setEnabled(false);
            addScanDirectory(homeDir);
        } else {
            //for (const auto& obj : scan_objects) {
            //    startScan(QString::fromStdString(obj.directory_path));
            //}
        }
        
        scan_obj.close();
    } catch (...) {
        QMessageBox::warning(this, "错误", "检查已有目录异常");
    }
}

void FileSearchApp::showAddDirectoryDialog() {
    AddDirectoryDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString directory = dialog.getDirectory();
        if (!directory.isEmpty() && QFileInfo(directory).exists()) {
            addScanDirectory(directory);
        } else {
            QMessageBox::warning(this, "错误", "目录不存在或路径无效");
        }
    }
}

void FileSearchApp::addScanDirectory(const QString& directory_path) {
    try {
        ScanObject scan_obj(db_path_.toStdString());
        bool success = scan_obj.add_scan_object(
            directory_path.toStdString(),
            QFileInfo(directory_path).fileName().toStdString()
        );
        
        if (success) {
            checkScanObjects();  // 刷新界面状态
            startScan(directory_path);
        } else {
            QMessageBox::warning(this, "错误", "添加扫描目录失败，可能已存在");
        }
        
        scan_obj.close();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", QString("添加扫描目录失败: %1").arg(e.what()));
    }
}

void FileSearchApp::startScan(const QString& directory_path) {
    auto thread = new ScanThread(directory_path, db_path_, this);
    connect(thread, &ScanThread::progressSignal, this, &FileSearchApp::updateScanProgress);
    connect(thread, &ScanThread::finishedSignal, this, &FileSearchApp::onScanFinished);
    thread->start();
    
    scan_threads_.append(thread);
}

void FileSearchApp::refreshAllScans() {
    try {
        ScanObject scan_obj(db_path_.toStdString());
        auto scan_objects = scan_obj.get_all_scan_objects(true);
        
        if (scan_objects.empty()) {
            QMessageBox::information(this, "提示", "没有可扫描的目录");
            return;
        }
        
        for (const auto& obj : scan_objects) {
            startScan(QString::fromStdString(obj.directory_path));
        }
        
        scan_obj.close();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", QString("刷新扫描失败: %1").arg(e.what()));
    }
}

void FileSearchApp::updateScanProgress(const QString& message) {
    status_label_->setText(message);
}

void FileSearchApp::onSearchTextChanged(const QString& text) {
    if (text.length() >= 2) {
        QTimer::singleShot(300, this, &FileSearchApp::performSearch);
    } else if (text.isEmpty()) {
        result_table_->clear();
    }
}

void FileSearchApp::performSearch() {
    QString search_text = search_input_->text().trimmed();
    if (search_text.isEmpty()) {
        result_table_->clear();
        status_label_->setText("就绪");
        return;
    }

    qDebug() << "searchText: " << search_text;
    
    try {
        FileDB db(db_path_.toStdString());
        
        // 搜索不同字段
        QList<QVariantMap> results;
        QStringList search_fields = {"file_name", "file_path", "file_extension"};
        
        for (const auto& field : search_fields) {
            auto db_results = db.search_files(
                search_text.toStdString(), 
                field.toStdString(), 
                200
            );
            
            for (const auto& db_result : db_results) {
                QVariantMap result;
                result["file_path"] = QString::fromStdString(db_result.file_path);
                result["file_name"] = QString::fromStdString(db_result.file_name);
                result["is_directory"] = db_result.is_directory;
                results.append(result);
            }
        }
        
        // 去重
        QSet<QString> seen_paths;
        QList<QVariantMap> unique_results;
        
        for (const auto& result : results) {
            QString file_path = result["file_path"].toString();
            if (!seen_paths.contains(file_path)) {
                seen_paths.insert(file_path);
                unique_results.append(result);
            }
        }
        
        displaySearchResults(unique_results);
        status_label_->setText(QString("找到 %1 个结果").arg(unique_results.size()));
        
        db.close();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", QString("搜索失败: %1").arg(e.what()));
    }
}

void FileSearchApp::displaySearchResults(const QList<QVariantMap>& results) {
    result_table_->setSearchResults(results);
}

void FileSearchApp::onScanFinished(bool success, const QString& message) {
    if (success) {
        status_label_->setText(message);
        // 扫描成功后如果有搜索内容，刷新搜索结果
        if (!search_input_->text().trimmed().isEmpty()) {
            performSearch();
        }
    } else {
        status_label_->setText(QString("扫描失败: %1").arg(message));
    }

    search_input_->setPlaceholderText("输入文件名、路径或扩展名进行搜索...");
    search_input_->setEnabled(true);
}

QString FileSearchApp::formatSize(qint64 size_bytes) const {
    if (size_bytes == 0) {
        return "0B";
    }
    
    QStringList size_names = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double size = size_bytes;
    
    while (size >= 1024 && i < size_names.size() - 1) {
        size /= 1024.0;
        i++;
    }
    
    return QString("%1%2").arg(size, 0, 'f', 1).arg(size_names[i]);
}

void FileSearchApp::closeEvent(QCloseEvent* event) {
    // 等待所有扫描线程完成
    for (auto thread : scan_threads_) {
        if (thread->isRunning()) {
            thread->wait(5000);
        }
    }
    event->accept();
}

// main函数
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("文件搜索器");
    
    FileSearchApp window;
    window.show();
    
    return app.exec();
}