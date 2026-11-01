#include "ui/MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    // Init application
    QApplication app(argc, argv);

    // Create and show Main Window
    MainWindow window;
    window.show();

    // Start Qt action loop
    return QApplication::exec();
}