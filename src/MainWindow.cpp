#include "MainWindow.hpp"
#include <qapplication.h>
#include <qnamespace.h>
#include <qfiledialog.h>
#include <qaudiooutput.h>
#include <qtimer.h>
#include <kjob.h>
#include <qmessagebox.h>

#define ICON_PLAY QIcon::fromTheme("media-playback-start")
#define ICON_PAUSE QIcon::fromTheme("media-playback-pause")

namespace Kalorite
{
    MainWindow::MainWindow() {
        resize(500, 150);
        setWindowTitle("Kalorite");

        centralWidget = new QWidget(this);

        this->currentMenuBar = menuBar();
        this->fileMenu = this->currentMenuBar->addMenu(tr("&File"));

        QAction* openSongAction = new QAction(tr("&OpenSong"), this);
        this->fileMenu->addAction(openSongAction);
        openSongAction->setShortcut(QKeySequence::fromString("Ctrl+O"));

        QAction *exitAction = new QAction(tr("&Exit"), this);
        this->fileMenu->addAction(exitAction);
        exitAction->setShortcut(QKeySequence::fromString("Ctrl+Esc"));

        openSongAction->setIcon(QIcon::fromTheme("document-open"));
        exitAction->setIcon(QIcon::fromTheme("application-exit"));

        this->mixer = new QMediaPlayer();
        this->output = new QAudioOutput();
        connect(this->mixer, &QMediaPlayer::playbackStateChanged, this, &MainWindow::onMediaPlayerStatusChanged);

        this->mixer->setAudioOutput(this->output);
        this->output->setVolume(volume);

        connect(exitAction, &QAction::triggered, this, &QApplication::quit);

        connect(openSongAction, &QAction::triggered, this, &MainWindow::openButtonTriggered);

        setMenuBar(this->currentMenuBar);

        this->mainLayout = new QVBoxLayout(centralWidget);
        this->playerLayout = new QHBoxLayout();

        soundList = new QListWidget();
        connect(soundList, &QListWidget::itemDoubleClicked, this, &MainWindow::onListSelection);

        playbackButton = new QPushButton();
        playbackButton->setIcon(ICON_PLAY);
        playbackButton->setToolTip(tr("PlayTooltip"));
        playbackButton->setEnabled(false);
        connect(playbackButton, &QPushButton::clicked, this, &MainWindow::onPlayTriggered);

        playbackSlider = new QSlider(Qt::Horizontal);
        playbackSlider->setEnabled(false);
        connect(playbackSlider, &QSlider::sliderPressed, this, &MainWindow::playbackSliderPressed);
        connect(playbackSlider, &QSlider::sliderReleased, this, &MainWindow::playbackSliderReleased);

        timeLabel = new QLabel("00:00:00 / 00:00:00");

        skipBackButton = new QPushButton();
        skipBackButton->setIcon(QIcon::fromTheme("media-skip-backward"));
        skipBackButton->setEnabled(false);
        skipBackButton->setToolTip(tr("SkipBackward"));
        connect(skipBackButton, &QPushButton::clicked, this, &MainWindow::onSkipBack);

        skipForwardButton = new QPushButton();
        skipForwardButton->setIcon(QIcon::fromTheme("media-skip-forward"));
        skipForwardButton->setEnabled(false);
        skipForwardButton->setToolTip(tr("SkipForward"));

        volumeBox = new QSpinBox();
        volumeBox->setMaximum(100);
        volumeBox->setMinimum(0);
        volumeBox->setValue(100);
        connect(volumeBox, &QSpinBox::valueChanged, this, &MainWindow::onSpinTriggered);
        
        connect(skipForwardButton, &QPushButton::clicked, this, &MainWindow::onSkipNext);
        setCentralWidget(this->centralWidget);

        this->playerLayout->addWidget(skipBackButton);
        this->playerLayout->addWidget(playbackButton);
        this->playerLayout->addWidget(skipForwardButton);
        this->playerLayout->addWidget(playbackSlider);
        this->playerLayout->addWidget(timeLabel);
        this->playerLayout->addWidget(volumeBox);

        this->mainLayout->addWidget(soundList);
        this->mainLayout->addLayout(playerLayout);

        this->playbackTimer = new QTimer();
        connect(this->playbackTimer, &QTimer::timeout, this, &MainWindow::onPlayback);

        show();
    }

    void MainWindow::onSpinTriggered(const int value) {
        this->volume = float(value) / 100;
        this->output->setVolume(this->volume);
    }

    void MainWindow::onListSelection(QListWidgetItem *item) {
        int id = this->soundList->row(item);
        seekToTrack(id);
    }

    void MainWindow::playbackSliderPressed() {
        stopPlayback();
    }

    void MainWindow::playbackSliderReleased() {
        startPlayback();
        int pos = playbackSlider->value();

        int sec = pos * this->mixer->duration() / 100;
        this->mixer->setPosition(sec);
        updateTimeLabel();
    }

    void MainWindow::closeEvent(QCloseEvent* event) {
        QApplication::exit();
    }
    
    void MainWindow::seekToTrack(const int id) {
        int listSize = this->soundList->count();

        if (id >= 0 && id < listSize) {
            this->soundList->setCurrentRow(id);
            this->currentId = id;
            stopPlayback();
            setCurrentSong(this->soundList->item(id)->text().toStdString());
            startPlayback();
        }
    }

    void MainWindow::setCurrentSong(const std::string path) {
        this->currentAudio = path;
        setWindowTitle(QString::fromStdString(std::format("Kalorite - {}", this->currentAudio)));
        this->mixer->setSource(QUrl::fromLocalFile(QString::fromStdString(path)));
        this->trackLengthSeconds = this->mixer->duration() / 1000;
        updateTimeLabel();
    }

    bool containsItem(QListWidget *list, const QString &text) {
        QList<QListWidgetItem*> found = list->findItems(text, Qt::MatchExactly);
        return !found.isEmpty();
    }

    void MainWindow::openButtonTriggered() {
        QString openPath = QFileDialog::getOpenFileName(this, tr("OpenSound"), QDir::homePath(), tr("FileFilters"));

        if (!openPath.isEmpty()) {
            if(!containsItem(this->soundList, openPath)) {

                if (this->currentAudio == "") {
                    this->currentAudio = openPath.toStdString();
                    this->setCurrentSong(this->currentAudio);
                    this->currentId = 0;
                }
            
                soundList->addItem(openPath);
                this->soundList->setCurrentRow(this->currentId);

                skipBackButton->setEnabled(true);
                playbackButton->setEnabled(true);
                playbackSlider->setEnabled(true);
                skipForwardButton->setEnabled(true);
            } else {
                QMessageBox::critical(nullptr, "Kalorite", tr("ErrorAlreadyHasElementInList"), QMessageBox::Ok);
            }
        }
    }

    void MainWindow::startPlayback() {
        this->isPlaying = true;
        this->playbackButton->setIcon(ICON_PAUSE);

        this->mixer->play();
        this->playbackTimer->start(1000);
    }

    void MainWindow::onPlayback() {
        updateTimeLabel();
    }

    void MainWindow::setPlaybackPos(const int percent) {

    }

    void MainWindow::updateTimeLabel() {
        this->trackLengthSeconds = this->mixer->duration() / 1000;
        int totalMinutes = (this->trackLengthSeconds % 3600) / 60;
        int totalHours = this->trackLengthSeconds / 3600;
        int totalSecondsLength = this->trackLengthSeconds % 60;

        int totalSeconds = mixer->position() / 1000;
        int minutes = (totalSeconds % 3600) / 60;
        int hours = totalSeconds / 3600;
        int seconds = totalSeconds % 60;
        float played = std::round((float(totalSeconds) / float(trackLengthSeconds)) * 100.0f);

        QString textValue = QString("%1:%2:%3 / %4:%5:%6")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(totalHours, 2, 10, QChar('0'))
            .arg(totalMinutes, 2, 10, QChar('0'))
            .arg(totalSecondsLength, 2, 10, QChar('0'));

        this->timeLabel->setText(textValue);
        this->playbackSlider->setValue(played);

        this->percent.emitPercent(played);
    }

    void MainWindow::onSkipBack() {
        int id = this->soundList->currentRow() - 1;
        seekToTrack(id);
    }

    void MainWindow::onSkipNext() {
        int id = this->soundList->currentRow() + 1;
        seekToTrack(id);
    }

    void MainWindow::onMediaPlayerStatusChanged(QMediaPlayer::PlaybackState status) {
        if (status == QMediaPlayer::PlaybackState::StoppedState) {
            stopPlayback();
            this->mixer->setPosition(0);
        }
    }

    void MainWindow::stopPlayback() {
        this->isPlaying = false;
        this->playbackButton->setIcon(ICON_PLAY);

        this->mixer->pause();
        this->playbackTimer->stop();
    }

    void MainWindow::onPlayTriggered() {
        this->isPlaying = !this->isPlaying;

        if (this->isPlaying) {
            startPlayback();
        } else {
            stopPlayback();
        }
    }
} // namespace Kalorite
