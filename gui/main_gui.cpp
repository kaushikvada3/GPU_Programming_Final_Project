#include <QApplication>

#include "mainwindow.h"
#include "renderworker.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    qRegisterMetaType<OrbitCamera>("OrbitCamera");

    MainWindow w;
    w.show();
    return app.exec();
}
