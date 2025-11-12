#ifndef FILE_RESULT_TABLE_H
#define FILE_RESULT_TABLE_H

#include <QTableWidget>
#include <QHeaderView>
#include <QFileInfo>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QIcon>
#include <QMimeData>
#include <QMimeDatabase>
#include "FileContextMenu.h"

class FileResultTable : public QTableWidget
{
    Q_OBJECT

public:
    explicit FileResultTable(QWidget *parent = nullptr);
    void setSearchResults(const QList<QVariantMap>& results);
    void clearResults();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onItemDoubleClicked(QTableWidgetItem *item);

private:
    QString getSelectedFilePath() const;
    QIcon getFileIcon(const QString& filePath, bool isDirectory) const;
    QString formatFileSize(qint64 sizeBytes) const;
    QString formatDateTime(const QDateTime& dateTime) const;

    FileContextMenu *contextMenu_;
};

#endif // FILE_RESULT_TABLE_H
