#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication aplikace(argc, argv);
    
    MainWindow okno;
    okno.show();
    
    return aplikace.exec();
}