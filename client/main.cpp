#include <QApplication>
#include "MainApp.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Anything");
    
    QApplication::setQuitOnLastWindowClosed(false);

    FileSearchApp window;
    window.show();
    
    return app.exec();
}