#ifndef FILECONTEXTMENU_H
#define FILECONTEXTMENU_H

#include <QMenu>
#include <QFileInfo>
#include <QUrl>
#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QDir>

class FileContextMenu : public QMenu
{
    Q_OBJECT

public:
    explicit FileContextMenu(const QString &filePath, QWidget *parent = nullptr);
    
signals:
    void fileRenamed(const QString &oldPath, const QString &newPath);
    void fileDeleted(const QString &filePath);

private slots:
    void openFile();
    void openFilePath();
    void copyFullPath();
    void cutFile();
    void copyFile();
    void deleteFile();
    void renameFile();

private:
    void setupActions();
    bool isClipboardFromThisApp() const;
    QString getUniqueName(const QString &basePath, const QString &fileName);
    
    QString filePath_;
    QFileInfo fileInfo_;
    
    QAction *openAction_;
    QAction *openDirAction_;
    QAction *copyPathAction_;
    QAction *cutAction_;
    QAction *copyAction_;
    QAction *deleteAction_;
    QAction *renameAction_;
    
    static QString clipboardSourcePath_;
    static bool clipboardIsCut_;
};

#endif // FILECONTEXTMENU_H