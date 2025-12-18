#include <SongDownloader.hpp>
#include "MainWindow.hpp"
#include <qwidget.h>
#include <QString>
#include <QPushButton>
#include <QLabel>
#include <QStandardPaths>

namespace Kalorite {
    SongDownloader::SongDownloader(MainWindow* parent) {
        this->parent = parent;

        manager = new QNetworkAccessManager(this);

        frame = new QWidget(nullptr);
        frame->setWindowTitle(tr("&DownloadSound"));
        frame->setFixedSize(245, 140);
        layout = new QVBoxLayout(frame);
        frame->setLayout(layout);

        urlInput = new QLineEdit(frame);
        urlInput->setPlaceholderText(tr("&DownloadSoundPlaceholder"));

        layout->addWidget(urlInput);

        buttonLayout = new QHBoxLayout();
        downloadButton = new QPushButton(tr("&DownloadSoundStart"), frame);
        buttonLayout->addWidget(downloadButton);

        cancelButton = new QPushButton(tr("&DownloadSoundCancel"), frame);
        buttonLayout->addWidget(cancelButton);

        cancelButton->hide();

        layout->addLayout(buttonLayout);

        downloadProgress = new QProgressBar(frame);
        downloadProgress->setTextVisible(false);
        layout->addWidget(downloadProgress);

        downloadStatus = new QLabel(frame);
        layout->addWidget(downloadStatus);
        downloadStatus->hide();

        connect(downloadButton, &QPushButton::clicked, this, &SongDownloader::onDownloadButtonClicked);
        connect(downloadProgress, &QProgressBar::valueChanged, this, &SongDownloader::onDownloadProgressChanged);
        connect(cancelButton, &QPushButton::clicked, this, &SongDownloader::onCancelButtonClicked);
    }

    void SongDownloader::onDownloadButtonClicked() {
        QUrl url(urlInput->text());
        if (!url.isValid()) {
            return;
        }

        downloadButton->hide();
        cancelButton->show();

        downloadProgress->setRange(0, 100);
        downloadProgress->setValue(0);

        downloadStatus->show();

        QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QString fileName = downloadPath + "/" + url.fileName();
        if (fileName.isEmpty()) fileName = downloadPath + "/track.mp3";

        file = new QFile(fileName);
        if (!file->open(QIODevice::WriteOnly)) {
            delete file;
            return;
        }

        currentReply = manager->get(QNetworkRequest(url));

        connect(currentReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
            if (total > 0) {
                downloadProgress->setRange(0, 100);
                downloadProgress->setValue(static_cast<int>((received * 100) / total));
            } else {
                downloadProgress->setRange(0, 0);
            }
            downloadStatus->setText(tr("&SongDlIndicator").arg(received / (1024 * 1024)));
        });

        connect(currentReply, &QNetworkReply::readyRead, this, [this]() {
            if (file) file->write(currentReply->readAll());
        });

        connect(currentReply, &QNetworkReply::finished, this, [this]() {
            onDownloadFinished();
        });
    }

    void SongDownloader::onDownloadFinished() {
        file->close();
        downloadButton->show();
        cancelButton->hide();
        currentReply->deleteLater();
        currentReply = nullptr;
        downloadProgress->setValue(0);
        downloadStatus->setText("");
        downloadStatus->hide();
        urlInput->setText(QString::fromStdString(""));

        let fileName = file->fileName();

        if (parent && !containsItem(parent->soundList, fileName)) {
            parent->soundList->addItem(fileName);
        }

        frame->hide();
    }

    void SongDownloader::onCancelButtonClicked() {
        currentReply->abort();

        let fileName = file->fileName();

        if (parent && containsItem(parent->soundList, fileName)) {
           delete parent->soundList->takeItem(parent->soundList->row(parent->soundList->findItems(fileName, Qt::MatchExactly).first()));
        }

        file->remove();
        frame->hide();
    }

    void SongDownloader::onDownloadProgressChanged(int value) {
        downloadProgress->setValue(value);
    }

    void SongDownloader::show() {
        frame->show();
    }
} // namespace Kalorite
