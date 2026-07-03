#include <SongDownloader.hpp>
#include "MainWindow.hpp"
#include <qwidget.h>
#include <QString>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QStandardPaths>

namespace Kalorite {
    SongDownloader::SongDownloader(MainWindow* parent) {
        this->parent = parent;

        manager = new QNetworkAccessManager(this);

        frame = new QWidget(nullptr);
        frame->setWindowTitle(tr("&DownloadSound"));
        frame->setFixedSize(340, 190);

        layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(10);

        titleLabel = new QLabel(tr("&DownloadSound"), frame);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        layout->addWidget(titleLabel);

        urlInput = new QLineEdit(frame);
        urlInput->setPlaceholderText(tr("&DownloadSoundPlaceholder"));
        urlInput->setClearButtonEnabled(true);
        layout->addWidget(urlInput);

        downloadProgress = new QProgressBar(frame);
        downloadProgress->setTextVisible(false);
        downloadProgress->setRange(0, 100);
        downloadProgress->setValue(0);
        layout->addWidget(downloadProgress);

        downloadStatus = new QLabel(frame);
        layout->addWidget(downloadStatus);
        downloadStatus->hide();

        layout->addStretch();

        buttonLayout = new QHBoxLayout();
        buttonLayout->setSpacing(8);
        buttonLayout->addStretch();

        cancelButton = new QPushButton(tr("&DownloadSoundCancel"), frame);
        cancelButton->setCursor(Qt::PointingHandCursor);
        cancelButton->hide();
        buttonLayout->addWidget(cancelButton);

        downloadButton = new QPushButton(tr("&DownloadSoundStart"), frame);
        downloadButton->setCursor(Qt::PointingHandCursor);
        downloadButton->setDefault(true);
        buttonLayout->addWidget(downloadButton);

        layout->addLayout(buttonLayout);

        connect(downloadButton, &QPushButton::clicked, this, &SongDownloader::onDownloadButtonClicked);
        connect(urlInput, &QLineEdit::returnPressed, this, &SongDownloader::onDownloadButtonClicked);
        connect(downloadProgress, &QProgressBar::valueChanged, this, &SongDownloader::onDownloadProgressChanged);
        connect(cancelButton, &QPushButton::clicked, this, &SongDownloader::onCancelButtonClicked);
    }

    void SongDownloader::resetUi() {
        downloadButton->show();
        downloadButton->setEnabled(true);
        cancelButton->hide();
        urlInput->setEnabled(true);
        urlInput->clear();
        downloadProgress->setRange(0, 100);
        downloadProgress->setValue(0);
        downloadStatus->clear();
        downloadStatus->hide();
    }

    void SongDownloader::onDownloadButtonClicked() {
        QUrl url(urlInput->text().trimmed());
        if (!url.isValid() || url.scheme().isEmpty()) {
            downloadStatus->show();
            downloadStatus->setText(tr("&DownloadSoundInvalidUrl"));
            return;
        }

        QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QString remoteName = url.fileName();
        if (remoteName.isEmpty()) remoteName = "track.mp3";
        QString fileName = downloadPath + "/" + remoteName;

        file = new QFile(fileName);
        if (!file->open(QIODevice::WriteOnly)) {
            downloadStatus->show();
            downloadStatus->setText(tr("&DownloadSoundWriteError"));
            delete file;
            file = nullptr;
            return;
        }

        downloadButton->hide();
        cancelButton->show();
        urlInput->setEnabled(false);

        downloadProgress->setRange(0, 0);
        downloadProgress->setValue(0);

        downloadStatus->show();
        downloadStatus->setText(tr("&SongDlIndicator").arg(0));

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
        if (!currentReply) return;

        bool ok = currentReply->error() == QNetworkReply::NoError;
        let fileName = file ? file->fileName() : QString();

        if (file) {
            file->close();
            if (!ok) file->remove();
            delete file;
            file = nullptr;
        }

        currentReply->deleteLater();
        currentReply = nullptr;

        if (ok) {
            if (parent && !containsItem(parent->soundList, fileName)) {
                parent->soundList->addItem(fileName);
            }
            resetUi();
            frame->hide();
        } else {
            resetUi();
            downloadStatus->show();
            downloadStatus->setText(tr("&DownloadSoundFailed"));
        }
    }

    void SongDownloader::onCancelButtonClicked() {
        if (currentReply) {
            currentReply->disconnect(this);
            currentReply->abort();
            currentReply->deleteLater();
            currentReply = nullptr;
        }

        if (file) {
            let fileName = file->fileName();
            file->close();
            file->remove();
            delete file;
            file = nullptr;

            if (parent && containsItem(parent->soundList, fileName)) {
               delete parent->soundList->takeItem(parent->soundList->row(parent->soundList->findItems(fileName, Qt::MatchExactly).first()));
            }
        }

        resetUi();
        frame->hide();
    }

    void SongDownloader::onDownloadProgressChanged(int value) {
        downloadProgress->setValue(value);
    }

    void SongDownloader::show() {
        if (!currentReply) resetUi();
        frame->show();
        frame->raise();
        frame->activateWindow();
        urlInput->setFocus();
    }
} // namespace Kalorite
