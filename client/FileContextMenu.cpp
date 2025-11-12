#include "FileContextMenu.h"
#include <QDebug>
#include <QMimeData>

// 静态成员初始化
QString FileContextMenu::clipboardSourcePath_ = "";
bool FileContextMenu::clipboardIsCut_ = false;

FileContextMenu::FileContextMenu(const QString &filePath, QWidget *parent)
    : QMenu(parent), filePath_(filePath), fileInfo_(filePath)
{
    setupActions();
}

void FileContextMenu::setupActions()
{
    // 打开
    openAction_ = addAction("打开", this, &FileContextMenu::openFile);

    // 打开所在文件夹
    openDirAction_ = addAction("打开所在文件夹", this, &FileContextMenu::openFilePath);

    // 复制完整路径
    copyPathAction_ = addAction("复制完整路径", this, &FileContextMenu::copyFullPath);        

    addSeparator();
    
    // 剪切
    cutAction_ = addAction("剪切", this, &FileContextMenu::cutFile);
    cutAction_->setShortcut(QKeySequence::Cut);
    
    // 复制
    copyAction_ = addAction("复制", this, &FileContextMenu::copyFile);
    copyAction_->setShortcut(QKeySequence::Copy);
    
    addSeparator();
    
    // 删除
    deleteAction_ = addAction("删除", this, &FileContextMenu::deleteFile);
    deleteAction_->setShortcut(QKeySequence::Delete);
    
    // 重命名
    renameAction_ = addAction("重命名", this, &FileContextMenu::renameFile);
    renameAction_->setShortcut(Qt::Key_F2);
}

bool FileContextMenu::isClipboardFromThisApp() const
{
    return !clipboardSourcePath_.isEmpty();
}

void FileContextMenu::openFile()
{
    if (fileInfo_.exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath_));
    } else {
        QMessageBox::warning(nullptr, "错误", "文件不存在");
    }
}

void FileContextMenu::openFilePath()
{
    if (!fileInfo_.exists()) {
        QMessageBox::warning(nullptr, "错误", "文件或文件夹不存在");
        return;
    }
    
    if (fileInfo_.isDir()) {
        // 如果是文件夹，直接打开自己
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath_));
    } else {
        // 如果是文件，打开所在文件夹
        QString folderPath = fileInfo_.absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
    }
}

void FileContextMenu::copyFullPath()
{
    QApplication::clipboard()->setText(filePath_);
}

void FileContextMenu::cutFile()
{
    clipboardSourcePath_ = filePath_;
    clipboardIsCut_ = true;
    
    // 设置剪贴板内容
    QMimeData *mimeData = new QMimeData;
    mimeData->setUrls({QUrl::fromLocalFile(filePath_)});
    mimeData->setText("cut");
    QApplication::clipboard()->setMimeData(mimeData);
}

void FileContextMenu::copyFile()
{
    clipboardSourcePath_ = filePath_;
    clipboardIsCut_ = false;
    
    // 设置剪贴板内容
    QMimeData *mimeData = new QMimeData;
    mimeData->setUrls({QUrl::fromLocalFile(filePath_)});
    QApplication::clipboard()->setMimeData(mimeData);
}

void FileContextMenu::deleteFile()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr,
        "确认删除",
        QString("确定要删除 \"%1\" 吗？").arg(fileInfo_.fileName()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        bool success = false;
        if (fileInfo_.isDir()) {
            QDir dir(filePath_);
            success = dir.removeRecursively();
        } else {
            success = QFile::remove(filePath_);
        }
        
        if (success) {
            emit fileDeleted(filePath_);
        } else {
            QMessageBox::warning(nullptr, "错误", "删除失败");
        }
    }
}

void FileContextMenu::renameFile()
{
    bool ok;
    QString newName = QInputDialog::getText(
        nullptr,
        "重命名",
        "请输入新名称:",
        QLineEdit::Normal,
        fileInfo_.fileName(),
        &ok
    );
    
    if (ok && !newName.trimmed().isEmpty()) {
        QString newPath = fileInfo_.absolutePath() + "/" + newName.trimmed();
        
        if (newPath == filePath_) {
            return; // 名称未改变
        }
        
        if (QFile::exists(newPath)) {
            QMessageBox::warning(nullptr, "错误", "文件已存在");
            return;
        }
        
        if (QFile::rename(filePath_, newPath)) {
            emit fileRenamed(filePath_, newPath);
        } else {
            QMessageBox::warning(nullptr, "错误", "重命名失败");
        }
    }
}

QString FileContextMenu::getUniqueName(const QString &basePath, const QString &fileName)
{
    QFileInfo fileInfo(basePath + "/" + fileName);
    if (!fileInfo.exists()) {
        return fileInfo.absoluteFilePath();
    }
    
    QString baseName = fileInfo.completeBaseName();
    QString suffix = fileInfo.suffix();
    int counter = 1;
    
    while (true) {
        QString newName;
        if (suffix.isEmpty()) {
            newName = QString("%1 (%2)").arg(baseName).arg(counter);
        } else {
            newName = QString("%1 (%2).%3").arg(baseName).arg(counter).arg(suffix);
        }
        
        QString newPath = basePath + "/" + newName;
        if (!QFile::exists(newPath)) {
            return newPath;
        }
        counter++;
    }
}