#ifndef FILERESULTTABLE_H
#define FILERESULTTABLE_H

#include <QTableWidget>
#include <QString>
#include <QList>
#include <QVariantMap>

class FileContextMenu;
class HighlightDelegate;

class FileResultTable : public QTableWidget
{
    Q_OBJECT

public:
    explicit FileResultTable(QWidget *parent = nullptr);
    void setSearchResults(const std::string& keyword, const QList<QVariantMap>& results);
    void clearResults();
    QString getSelectedFilePath() const;

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onItemDoubleClicked(QTableWidgetItem *item);

private:
    QIcon getFileIcon(const QString& filePath, bool isDirectory) const;
    QString formatFileSize(qint64 sizeBytes) const;
    QString formatDateTime(const QDateTime& dateTime) const;
    
    QString keyword_;
    HighlightDelegate* nameDelegate_ = nullptr;
    //HighlightDelegate* pathDelegate_ = nullptr;
    
    // 高亮颜色配置
    QColor highlightColor_ = Qt::red;
    bool highlightBold_ = true;
};

#endif // FILERESULTTABLE_H