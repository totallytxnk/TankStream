#include "MainWindow.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("TankStream");
    QApplication::setApplicationVersion("0.4.0");
    QApplication::setOrganizationName("TankStream");

    MainWindow window;
    window.show();
    return app.exec();
}
