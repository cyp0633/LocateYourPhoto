#include "ui/main_window.h"
#include <QApplication>
#include <QDebug>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("LocateYourPhoto");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LocateYourPhoto");
    
    lyp::MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}
