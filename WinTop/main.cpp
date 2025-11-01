#include "WinTop.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    WinTop window;
    window.show();
    window.resize(1000, 600);

    return app.exec();
}
