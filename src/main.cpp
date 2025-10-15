#include "MainWindow.hpp"
#include <qlogging.h>
#include <qtranslator.h>
#include <qapplication.h>
#include <qlocale.h>

int main(int argc, char** argv) {
    QGuiApplication::setDesktopFileName("kalorite");

    QApplication app(argc, argv);

    QTranslator translator;
    QLocale systemLocale = QLocale::system();

    qDebug() << "===Kalorite===";
    qDebug() << "OS Locale: " << systemLocale.name();
    qDebug() << "==============";

    if (translator.load(systemLocale, "kalorite", "_", "")) {
        if (app.installTranslator(&translator)) {
            qDebug() << "Installed translator";
        } else {
            qWarning() << "Error installing translator";
        }
    } else {
        qWarning() << "Failed to load translator for: " << systemLocale.name();
    }

    Kalorite::MainWindow window;

    return app.exec();
}