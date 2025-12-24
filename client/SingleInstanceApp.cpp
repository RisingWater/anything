#include "SingleInstanceApp.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QCoreApplication>
#include <QDebug>

SingleInstanceApp::SingleInstanceApp(const QString& serverName, QObject* parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_serverName(serverName.isEmpty() ? QCoreApplication::applicationName() : serverName)
    , m_isRunning(false) {
    
    // 确保服务器名称合法
    if (m_serverName.isEmpty()) {
        m_serverName = "anything_single_instance";
    }
    
    // 移除可能存在的非法字符
    m_serverName.replace(' ', '_');
    m_serverName.replace('/', '_');
    m_serverName.replace('\\', '_');
    m_serverName.replace(':', '_');
    m_serverName.replace('|', '_');
    m_serverName.replace('?', '_');
    m_serverName.replace('*', '_');
    m_serverName.replace('<', '_');
    m_serverName.replace('>', '_');
    m_serverName.replace('"', '_');
}

SingleInstanceApp::~SingleInstanceApp() {
    cleanupLocalServer();
}

bool SingleInstanceApp::isAnotherInstanceRunning() {
    // 尝试连接到已有的实例
    QLocalSocket socket;
    socket.connectToServer(m_serverName);
    
    // 等待连接，超时时间500ms
    if (socket.waitForConnected(500)) {
        qDebug() << "Another instance is already running";
        socket.close();
        return true;
    }
    
    // 没有实例运行，尝试创建服务器
    return !initLocalServer();
}

bool SingleInstanceApp::tryActivateExistingInstance() {
    // 尝试连接到已有的实例
    QLocalSocket socket;
    socket.connectToServer(m_serverName);
    
    if (!socket.waitForConnected(500)) {
        qDebug() << "No existing instance found to activate";
        return false;
    }
    
    // 发送激活请求
    const char activateCmd[] = "ACTIVATE";
    qint64 bytesWritten = socket.write(activateCmd, sizeof(activateCmd) - 1);
    
    if (bytesWritten == -1) {
        qWarning() << "Failed to write activate command:" << socket.errorString();
        socket.close();
        return false;
    }
    
    // 等待数据写入
    if (!socket.waitForBytesWritten(500)) {
        qWarning() << "Failed to wait for bytes written:" << socket.errorString();
        socket.close();
        return false;
    }
    
    socket.close();
    qDebug() << "Successfully sent activate request to existing instance";
    return true;
}

void SingleInstanceApp::setServerName(const QString& serverName) {
    if (m_serverName == serverName || serverName.isEmpty()) {
        return;
    }
    
    // 清理旧的服务器
    cleanupLocalServer();
    
    // 设置新的服务器名称
    m_serverName = serverName;
    m_isRunning = false;
}

QString SingleInstanceApp::serverName() const {
    return m_serverName;
}

bool SingleInstanceApp::isRunning() const {
    return m_isRunning;
}

bool SingleInstanceApp::initLocalServer() {
    // 清理可能存在的旧服务器
    QLocalServer::removeServer(m_serverName);
    
    // 创建新的服务器
    m_server = new QLocalServer(this);
    
    // 设置服务器名称
    if (!m_server->listen(m_serverName)) {
        QString errorMsg = QString("Failed to create local server '%1': %2")
                            .arg(m_serverName)
                            .arg(m_server->errorString());
        qWarning() << errorMsg;
        emit instanceStartFailed(errorMsg);
        
        delete m_server;
        m_server = nullptr;
        return false;
    }
    
    // 连接新客户端信号
    connect(m_server, &QLocalServer::newConnection, 
            this, &SingleInstanceApp::handleNewConnection);
    
    m_isRunning = true;
    qDebug() << "Local server created successfully:" << m_serverName;
    return true;
}

void SingleInstanceApp::cleanupLocalServer() {
    if (m_server) {
        if (m_server->isListening()) {
            m_server->close();
        }
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_isRunning = false;
}

void SingleInstanceApp::handleNewConnection() {
    if (!m_server) {
        return;
    }
    
    // 获取新的客户端连接
    QLocalSocket* clientSocket = m_server->nextPendingConnection();
    if (!clientSocket) {
        return;
    }
    
    // 连接断开信号，自动清理
    connect(clientSocket, &QLocalSocket::disconnected,
            clientSocket, &QLocalSocket::deleteLater);
    
    // 等待读取数据
    if (clientSocket->waitForReadyRead(500)) {
        QByteArray data = clientSocket->readAll();
        QString cmd = QString::fromUtf8(data).trimmed();
        
        if (cmd == "ACTIVATE") {
            qDebug() << "Received activate request from another instance";
            emit activateRequested();
        } else {
            qDebug() << "Received unknown command:" << cmd;
        }
    } else {
        // 如果没有数据，仍然认为需要激活（兼容性处理）
        qDebug() << "No data received, but emitting activate request";
        emit activateRequested();
    }
    
    // 关闭连接
    clientSocket->close();
}