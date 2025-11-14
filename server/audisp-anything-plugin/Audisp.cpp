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
#include <QtConcurrent>
#include <QByteArray>

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

// 解码十六进制字符串
QString decodeHexPath(const QString &hexPath)
{
    // 检查是否是纯十六进制字符串（只包含0-9, A-F）
    QRegExp hexRegex("^[0-9A-F]+$");
    if (hexRegex.exactMatch(hexPath)) {
        QByteArray byteArray = QByteArray::fromHex(hexPath.toLatin1());
        return QString::fromUtf8(byteArray);
    }
    return hexPath; // 如果不是纯十六进制，返回原字符串
}

// 实际的网络请求函数
void performNetworkRequest(const QString &path, const QString &type)
{
    QNetworkAccessManager localManager;
    QEventLoop eventLoop;
    
    QJsonObject obj;
    obj["path"] = path;
    obj["type"] = type;
    obj["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QByteArray jsonData = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    
    QNetworkRequest request;
    request.setUrl(QUrl(WEB_URL));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = localManager.post(request, jsonData);
    
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    
    // 设置超时
    QTimer::singleShot(2000, &eventLoop, &QEventLoop::quit);
    
    eventLoop.exec();
    
    reply->deleteLater();
}

void sendEventToWebService(const QString &path, const QString &type)
{
    QtConcurrent::run(performNetworkRequest, path, type);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    logMessage("Audisp Qt plugin started");

    QTextStream in(stdin);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.contains("type=PATH"))
            continue;

        QString filePath;
        QString eventType;

        #if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        #else
        QStringList parts = line.split(' ', QString::SkipEmptyParts);
        #endif
        
        for (const QString &p : parts) {
            if (p.startsWith("name=")) {
                filePath = p.mid(5).remove('"');
                // 解码十六进制路径
                filePath = decodeHexPath(filePath);
            }
            else if (p.startsWith("nametype=")) {
                QString nt = p.mid(9);
                if (nt == "CREATE") eventType = "CREATE";
                else if (nt == "DELETE") eventType = "DELETE";
            }
        }

        if (filePath.isEmpty() || eventType.isEmpty())
            continue;

        QFileInfo fi(filePath);
        QString pathType;
        if (eventType == "CREATE") {
            pathType = fi.isDir() ? "MKDIR" : "CREATE";
        } else if (eventType == "DELETE") {
            pathType = fi.isDir() ? "RMDIR" : "DELETE";
        }

        // 记录解码后的路径
        logMessage(pathType + " " + filePath);

        sendEventToWebService(filePath, pathType);
        
        QCoreApplication::processEvents();
    }

    logMessage("Audisp Qt plugin exited");

    return 0;
}