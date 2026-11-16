#include <QApplication>
#include <QMessageBox>
#include "SystemServices.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("PlaceBridge");
    QCoreApplication::setApplicationName("PlaceBridge");

    Theme::install(app);          // palette, fonts and style sheet (the web look, in Qt)

    try {
        MainWindow window;
        window.show();
        return app.exec();
    } catch (const std::exception &error) {
        AppLogger::write("ERROR", QString("Application startup failed: %1").arg(error.what()));
        QMessageBox::critical(nullptr, "PlaceBridge could not start",
                              QString("The application could not open its database.\n\n%1")
                                  .arg(QString::fromLocal8Bit(error.what())));
        return 1;
    }
}
