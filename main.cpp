#include "cronometerwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    CronometerWindow w;
    w.show();
    return a.exec();
}
