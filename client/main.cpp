#include "SingleInstanceApp.h"
#include "MainApp.h"

#include <QApplication>
#include <QMessageBox>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("Anything");
    app.setOrganizationName("Anything");
    app.setOrganizationDomain("anything.com");
    app.setQuitOnLastWindowClosed(false);
    
    // 创建单例检查器
    SingleInstanceApp singleApp;
    
    // 检查是否已有实例运行
    if (singleApp.isAnotherInstanceRunning()) {
        // 尝试激活已运行的实例
        if (singleApp.tryActivateExistingInstance()) {
            qDebug() << "Activated existing instance, exiting...";
            return 0;
        } else {
            // 激活失败，可能是服务器创建失败，尝试继续运行
            QMessageBox::warning(nullptr, 
                "警告", 
                "无法激活已运行的实例，程序将继续运行。\n"
                "可能会有多个实例同时运行。");
        }
    }
    
    // 这是第一个实例，创建主窗口
    FileSearchApp window;
    
    // 连接激活信号
    QObject::connect(&singleApp, &SingleInstanceApp::activateRequested,
                     &window, &FileSearchApp::onActivateRequested);
    
    // 显示窗口
    window.show();
    
    return app.exec();
}