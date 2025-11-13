#ifndef SCANOBJDIALOG_H
#define SCANOBJDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QHeaderView>

class ScanObjDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScanObjDialog(QWidget* parent = nullptr);
    ~ScanObjDialog();

private slots:
    void browseDirectory();
    void addScanObject();
    void deleteScanObject();
    void loadScanObjects();
    void onScanObjectsLoaded(QNetworkReply* reply);
    void onScanObjectAdded(QNetworkReply* reply);
    void onScanObjectDeleted(QNetworkReply* reply);

private:
    void setupUI();
    QString getCurrentUserUid() const;
    void addScanObjectInternal(const QString& path, const QString& desc);
    
    QTableWidget* table_widget_;
    QLineEdit* dir_input_;
    QLineEdit* desc_input_;
    QPushButton* add_btn_;
    QPushButton* delete_btn_;
    QPushButton* refresh_btn_;
    QPushButton* cancel_btn_;
    
    QNetworkAccessManager* network_manager_;
};

#endif // SCANOBJDIALOG_H