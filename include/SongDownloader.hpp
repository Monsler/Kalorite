#pragma once

#include <qabstractspinbox.h>
#include <qboxlayout.h>
#include <qobject.h>
#include <qprogressbar.h>
#include <qwidget.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <QProgressBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

namespace Kalorite {
    class MainWindow;

    class SongDownloader : public QObject {
    Q_OBJECT

    public:
        SongDownloader(MainWindow* parent=nullptr);

        void show();
        void setParent(MainWindow* parent);

    public slots:
        void onDownloadButtonClicked();
        void onDownloadProgressChanged(int value);
        void onCancelButtonClicked();
        void onDownloadFinished();

    private:
        QWidget* frame;
        QVBoxLayout* layout;
        QHBoxLayout* buttonLayout;
        QLineEdit* urlInput;
        QPushButton* downloadButton;
        QPushButton* cancelButton;
        QProgressBar* downloadProgress;

        MainWindow* parent;

        QNetworkAccessManager *manager;
        QNetworkReply *currentReply = nullptr;
        QFile *file = nullptr;

    };

} // namespace Kalorite
