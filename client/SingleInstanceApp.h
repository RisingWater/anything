#ifndef SINGLEINSTANCEAPP_H
#define SINGLEINSTANCEAPP_H

#include <QObject>
#include <QString>

class QLocalServer;

/**
 * @brief 单例应用控制类
 * 
 * 确保应用程序只有一个实例运行，第二次启动时会激活第一个实例
 * 使用 QLocalServer 实现进程间通信
 */
class SingleInstanceApp : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * @param serverName 服务器名称，用于进程间通信，必须唯一
     * @param parent 父对象
     */
    explicit SingleInstanceApp(const QString& serverName = QString(), QObject* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~SingleInstanceApp() override;
    
    /**
     * @brief 检查是否已有实例运行
     * @return true-已有实例运行，false-这是第一个实例
     */
    bool isAnotherInstanceRunning();
    
    /**
     * @brief 尝试激活已运行的实例
     * @return true-成功激活已有实例，false-激活失败
     */
    bool tryActivateExistingInstance();
    
    /**
     * @brief 设置服务器名称
     * @param serverName 服务器名称
     */
    void setServerName(const QString& serverName);
    
    /**
     * @brief 获取服务器名称
     * @return 服务器名称
     */
    QString serverName() const;
    
    /**
     * @brief 获取是否正在运行（已成功创建服务器）
     * @return true-正在运行，false-未运行
     */
    bool isRunning() const;
    
signals:
    /**
     * @brief 收到激活请求信号
     * 
     * 当第二个实例启动并尝试激活时，会触发此信号
     * 主窗口应连接此信号并执行激活操作
     */
    void activateRequested();
    
    /**
     * @brief 实例启动失败信号
     * @param errorMessage 错误信息
     */
    void instanceStartFailed(const QString& errorMessage);
    
private slots:
    /**
     * @brief 处理新连接
     */
    void handleNewConnection();
    
private:
    /**
     * @brief 初始化本地服务器
     * @return true-初始化成功，false-初始化失败
     */
    bool initLocalServer();
    
    /**
     * @brief 清理本地服务器
     */
    void cleanupLocalServer();
    
    QLocalServer* m_server;    ///< 本地服务器
    QString m_serverName;      ///< 服务器名称
    bool m_isRunning;          ///< 是否正在运行
};

#endif // SINGLEINSTANCEAPP_H