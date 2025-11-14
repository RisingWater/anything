#include <QCoreApplication>
#include <QTextStream>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStringList>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>

QNetworkAccessManager *g_netManager = nullptr;
const QString WEB_URL = "http://localhost:5071/api/audit/events";
const QString LOG_FILE = "/tmp/audisp-qt-plugin.log";

// 写日志到文件
void logMessage(const QString &msg)
{
    QFile f(LOG_FILE);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss ") << msg << "\n";
        f.close();
    }
}

// 发送事件到 WebService
void sendEventToWebService(const QString &path, const QString &type)
{
    if (!g_netManager) return;

    QJsonObject obj;
    obj["path"] = path;
    obj["type"] = type;
    obj["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // 修正：正确的 QNetworkRequest 声明方式
    QNetworkRequest request;
    request.setUrl(QUrl(WEB_URL));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    g_netManager->post(request, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    g_netManager = new QNetworkAccessManager(&app);

    QTextStream in(stdin);

    logMessage("Audisp Qt plugin started");

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.contains("type=PATH"))
            continue;

        QString filePath;
        QString eventType;  // CREATE / DELETE

        // 解析 PATH 事件
        // 修正：恢复使用 QString::SkipEmptyParts
        QStringList parts = line.split(' ', QString::SkipEmptyParts);
        for (const QString &p : parts) {
            if (p.startsWith("name="))
                filePath = p.mid(5).remove('"');
            else if (p.startsWith("nametype=")) {
                QString nt = p.mid(9);
                if (nt == "CREATE") eventType = "CREATE";
                else if (nt == "DELETE") eventType = "DELETE";
            }
        }

        if (filePath.isEmpty() || eventType.isEmpty())
            continue;

        // 判断是文件还是目录
        QFileInfo fi(filePath);
        QString pathType;
        if (eventType == "CREATE") {
            pathType = fi.isDir() ? "MKDIR" : "CREATE";
        } else if (eventType == "DELETE") {
            pathType = fi.isDir() ? "RMDIR" : "DELETE";
        }

        // 写日志
        logMessage(pathType + " " + filePath);

        // 发送到 WebService
        sendEventToWebService(filePath, pathType);
    }

    logMessage("Audisp Qt plugin exited");

    return 0;
}