#include "FileResultTable.h"
#include <QDebug>
#include <QContextMenuEvent>

FileResultTable::FileResultTable(QWidget *parent)
    : QTableWidget(parent)
{
    // 设置表头
    setColumnCount(4);
    QStringList headers;
    headers << "名称" << "路径" << "大小" << "修改时间";
    setHorizontalHeaderLabels(headers);
    
    // 设置表格属性
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSortingEnabled(true);
    setAlternatingRowColors(true);

    verticalHeader()->setVisible(false);
    
    // 设置列宽
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    
    // 连接双击信号
    connect(this, &QTableWidget::itemDoubleClicked, this, &FileResultTable::onItemDoubleClicked);
}

void FileResultTable::contextMenuEvent(QContextMenuEvent *event)
{
    QTableWidgetItem *item = itemAt(event->pos());
    if (!item) return;
    
    setCurrentItem(item);
    QString filePath = getSelectedFilePath();
    if (filePath.isEmpty()) return;
    
    // 使用 FileContextMenu
    FileContextMenu *menu = new FileContextMenu(filePath, this);
    //connect(menu, &FileContextMenu::fileRenamed, this, &FileResultTable::onFileRenamed);
    //connect(menu, &FileContextMenu::fileDeleted, this, &FileResultTable::onFileDeleted);
    
    menu->exec(event->globalPos());
    menu->deleteLater();
}

void FileResultTable::setSearchResults(const QList<QVariantMap>& results)
{
    clearResults();
    
    setRowCount(results.size());
    
    for (int i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        QString filePath = result["file_path"].toString();
        QString fileName = result["file_name"].toString();
        bool isDirectory = result["is_directory"].toBool();
        
        QFileInfo fileInfo(filePath);
        QIcon fileIcon = getFileIcon(filePath, isDirectory);
        
        // 名称列（带图标）
        auto nameItem = new QTableWidgetItem(fileIcon, fileName);
        nameItem->setData(Qt::UserRole, filePath); // 存储完整路径
        nameItem->setToolTip(fileName);
        
        // 路径列
        auto pathItem = new QTableWidgetItem(fileInfo.absolutePath());
        pathItem->setToolTip(fileInfo.absolutePath());
        
        // 大小列
        QString sizeText = isDirectory ? QString() : formatFileSize(fileInfo.size());
        auto sizeItem = new QTableWidgetItem(sizeText);
        sizeItem->setTextAlignment(Qt::AlignRight);
        
        // 修改时间列
        auto timeItem = new QTableWidgetItem(formatDateTime(fileInfo.lastModified()));
        
        // 设置所有项的数据，用于排序
        nameItem->setData(Qt::UserRole + 1, fileName);
        pathItem->setData(Qt::UserRole + 1, fileInfo.absolutePath());
        sizeItem->setData(Qt::UserRole + 1, isDirectory ? -1 : fileInfo.size());
        timeItem->setData(Qt::UserRole + 1, fileInfo.lastModified());
        
        setItem(i, 0, nameItem);
        setItem(i, 1, pathItem);
        setItem(i, 2, sizeItem);
        setItem(i, 3, timeItem);
    }
    
    // 自动调整列宽
    resizeColumnsToContents();
}

void FileResultTable::clearResults()
{
    setRowCount(0);
    // 确保表头保持正确
    QStringList headers;
    headers << "名称" << "路径" << "大小" << "修改时间";
    setHorizontalHeaderLabels(headers);
}

void FileResultTable::onItemDoubleClicked(QTableWidgetItem *item)
{
    if (!item) return;
    
    // 获取完整文件路径
    int row = item->row();
    QTableWidgetItem* nameItem = this->item(row, 0);
    QString filePath = nameItem->data(Qt::UserRole).toString();
    
    if (filePath.isEmpty()) return;
    
    QFileInfo fileInfo(filePath);
    
    if (fileInfo.isDir()) {
        // 如果是目录，在文件管理器中打开
        QUrl url = QUrl::fromLocalFile(filePath);
        QDesktopServices::openUrl(url);
    } else {
        // 如果是文件，用系统默认程序打开
        QUrl url = QUrl::fromLocalFile(filePath);
        QDesktopServices::openUrl(url);
    }
}

QIcon FileResultTable::getFileIcon(const QString& filePath, bool isDirectory) const
{
    if (isDirectory) {
        // 文件夹图标
        return QIcon::fromTheme("folder", QIcon(":/icons/folder.png"));
    } else {
        // 文件图标 - 根据MIME类型
        QMimeDatabase mimeDb;
        QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
        QString iconName = mimeType.iconName();
        QString mimeTypeName = mimeType.name();

        if (mimeTypeName == "application/x-object" || 
            mimeTypeName == "application/x-executable" ||
            mimeTypeName == "application/x-sharedlib") {
            // 二进制文件使用特定的图标
            iconName = "application-x-executable";
        } else if (mimeTypeName.startsWith("application/octet-stream")) {
            // 未知二进制文件
            iconName = "application-octet-stream";
        }
        
        if (iconName.isEmpty()) {
            iconName = "text-x-generic";
        }
        
        return QIcon::fromTheme(iconName, QIcon(":/icons/file.png"));
    }
}

QString FileResultTable::formatFileSize(qint64 sizeBytes) const
{
    if (sizeBytes == 0) {
        return "0 B";
    }
    
    static const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = sizeBytes;
    
    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        unitIndex++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', unitIndex > 0 ? 1 : 0).arg(units[unitIndex]);
}

QString FileResultTable::formatDateTime(const QDateTime& dateTime) const
{
    return dateTime.toString("yyyy-MM-dd hh:mm:ss");
}

QString FileResultTable::getSelectedFilePath() const
{
    QTableWidgetItem *item = currentItem();
    if (!item) return QString();
    
    int row = item->row();
    QTableWidgetItem* nameItem = this->item(row, 0);
    return nameItem->data(Qt::UserRole).toString();
}