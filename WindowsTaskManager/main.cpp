#include "WinTaskManager.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    WinTaskManager window;
    window.show();
    window.resize(1000, 600);

    return app.exec();
}
