#include "MainWindow.hpp"
#include <fstream>
#include <qapplication.h>
#include <qnamespace.h>
#include <qfiledialog.h>
#include <qaudiooutput.h>
#include <qtimer.h>
#include <kjob.h>
#include <qmessagebox.h>
#include <random>
#include <algorithm>
#include <nlohmann/json.hpp>

#define ICON_PLAY QIcon::fromTheme("media-playback-start")
#define ICON_PAUSE QIcon::fromTheme("media-playback-pause")
#define ICON_REPEAT QIcon::fromTheme("media-playlist-repeat")
#define ICON_REPEAT_CURRENT QIcon::fromTheme("media-playlist-repeat-song")
#define ICON_REPEAT_THE_LIST QIcon::fromTheme("media-playlist-repeat-amarok")
#define ICON_REPEAT_SHUFFLE QIcon::fromTheme("media-playlist-shuffle")

namespace Kalorite
{
    MainWindow::MainWindow() {
        resize(500, 150);
        setWindowTitle("Kalorite");
        setWindowIcon(QIcon("kalorite"));

        centralWidget = new QWidget(this);

        this->currentMenuBar = menuBar();
        this->fileMenu = this->currentMenuBar->addMenu(tr("&File"));

        QAction* openSongAction = new QAction(tr("&OpenSong"), this);
        openSongAction->setShortcut(QKeySequence::fromString("Ctrl+O"));

        QAction* savePlaylistAction = new QAction(tr("&SavePlaylistAs"));
        savePlaylistAction->setShortcut(QKeySequence::fromString("Ctrl+S"));

        QAction* loadPlaylistAction = new QAction(tr("&LoadPlaylistFrom"));
        loadPlaylistAction->setShortcut(QKeySequence::fromString("Ctrl+V"));

        this->fileMenu->addAction(openSongAction);
        this->fileMenu->addAction(savePlaylistAction);
        this->fileMenu->addAction(loadPlaylistAction);


        QAction *exitAction = new QAction(tr("&Exit"), this);
        this->fileMenu->addAction(exitAction);
        exitAction->setShortcut(QKeySequence::fromString("Ctrl+Esc"));

        openSongAction->setIcon(QIcon::fromTheme("list-add"));
        exitAction->setIcon(QIcon::fromTheme("application-exit"));
        savePlaylistAction->setIcon(QIcon::fromTheme("document-save-as"));
        loadPlaylistAction->setIcon(QIcon::fromTheme("document-open"));

        connect(openSongAction, &QAction::triggered, this, &MainWindow::openButtonTriggered);
        connect(savePlaylistAction, &QAction::triggered, this, &MainWindow::savePlaylistTriggered);
        connect(loadPlaylistAction, &QAction::triggered, this, &MainWindow::loadPlaylistTriggered);

        this->mixer = new QMediaPlayer();
        this->output = new QAudioOutput();
        connect(this->mixer, &QMediaPlayer::playbackStateChanged, this, &MainWindow::onMediaPlayerStatusChanged);

        this->mixer->setAudioOutput(this->output);
        this->output->setVolume(volume);

        connect(exitAction, &QAction::triggered, this, &QApplication::quit);

        setMenuBar(this->currentMenuBar);

        this->mainLayout = new QVBoxLayout(centralWidget);
        this->playerLayout = new QHBoxLayout();

        soundList = new QListWidget();
        soundList->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(soundList, &QListWidget::itemDoubleClicked, this, &MainWindow::onListSelection);
        connect(soundList, &QListWidget::customContextMenuRequested, this, &MainWindow::onContextMenuSoundList);

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

        repeatButton = new QPushButton();
        repeatButton->setIcon(ICON_REPEAT);
        repeatButton->setEnabled(false);
        connect(repeatButton, &QPushButton::clicked, this, &MainWindow::onRepeatButtonTriggered);
        repeatButton->setToolTip(tr("TooltipNoRepeat"));


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
        this->playerLayout->addWidget(repeatButton);
        this->playerLayout->addWidget(playbackSlider);
        this->playerLayout->addWidget(timeLabel);
        this->playerLayout->addWidget(volumeBox);

        this->mainLayout->addWidget(soundList);
        this->mainLayout->addLayout(playerLayout);

        this->playbackTimer = new QTimer();
        connect(this->playbackTimer, &QTimer::timeout, this, &MainWindow::onPlayback);

        show();
    }

    void MainWindow::savePlaylistTriggered() {
        QString saveFilePath = QFileDialog::getSaveFileName(this, tr("&SavePlaylistAs"), QDir::homePath(), tr("PlaylistFileFilters"));
        if (!saveFilePath.isEmpty()) {
            nlohmann::json playlist = {};
            for (int i = 0; i < this->soundList->count(); i++) {
                auto item = this->soundList->item(i);
                playlist[i] = item->text().toStdString();
            }

            std::ofstream file(saveFilePath.toStdString());
            file << playlist;
            file.close();
        }
    }

    void MainWindow::loadPlaylistTriggered() {
        QString openPath = QFileDialog::getOpenFileName(this, tr("&LoadPlaylistFrom"), QDir::homePath(), tr("PlaylistFileFilters"));

        if (!openPath.isEmpty()) {
            this->soundList->clear();
            std::ifstream file(openPath.toStdString());

            nlohmann::json playlist;
            file >> playlist;
            file.close();

            for (int i = 0; i < playlist.size(); i++) {
                this->soundList->addItem(QString::fromStdString(playlist[i]));
            }
            this->soundList->setCurrentRow(0);
            setCurrentSong(this->soundList->item(0)->text().toStdString());

            genShuffle();
        }
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

        skipBackButton->setEnabled(true);
        playbackButton->setEnabled(true);
        playbackSlider->setEnabled(true);
        skipForwardButton->setEnabled(true);
        repeatButton->setEnabled(true);
    }

    bool containsItem(QListWidget *list, const QString &text) {
        QList<QListWidgetItem*> found = list->findItems(text, Qt::MatchExactly);
        return !found.isEmpty();
    }

    void MainWindow::onContextMenuSoundList(const QPoint &pos) {
        auto item = this->soundList->itemAt(pos);

        if (item) {
            QMenu menu;

            QAction* actionRemove = menu.addAction(tr("RemoveFromList"));
            actionRemove->setIcon(QIcon::fromTheme("document-close"));
            connect(actionRemove, &QAction::triggered, [item, this, pos] () {
                this->soundList->takeItem(this->soundList->currentRow());
                this->genShuffle();
            });

            menu.exec(this->soundList->viewport()->mapToGlobal(pos));
        }
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
                repeatButton->setEnabled(true);
                genShuffle();
            } else {
                QMessageBox::critical(nullptr, "Kalorite", tr("ErrorAlreadyHasElementInList"), QMessageBox::Ok);
            }
        }
    }

    void MainWindow::startPlayback() {
        this->isPlaying = true;
        this->playbackButton->setIcon(ICON_PAUSE);

        this->mixer->play();
        this->playbackTimer->start(1);
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

    void MainWindow::onRepeatButtonTriggered() {
        loopType++;
        if (loopType > 3) {
            loopType = 0;
        }

        if (loopType == 0) {
            repeatButton->setIcon(ICON_REPEAT);
            repeatButton->setToolTip(tr("TooltipNoRepeat"));
        } else if (loopType == 1) {
            repeatButton->setIcon(ICON_REPEAT_CURRENT);
            repeatButton->setToolTip(tr("TooltipRepeatSingleTrack"));
        } else if (loopType == 2) {
            repeatButton->setToolTip(tr("TooltipRepeatList"));
            repeatButton->setIcon(ICON_REPEAT_THE_LIST);
        } else if (loopType == 3) {
            repeatButton->setToolTip(tr("TooltipRepeatShuffle"));
            genShuffle();
            shufflePos = 0;
            repeatButton->setIcon(ICON_REPEAT_SHUFFLE);
        }
    }

    void MainWindow::onSkipBack() {
        if (this->playbackSlider->value() > 5) {
            this->playbackSlider->setValue(0);
            this->mixer->setPosition(0);
        } else {
            int id = this->soundList->currentRow() - 1;
            seekToTrack(id);
        }

    }

    void MainWindow::onSkipNext() {
        if (this->playbackSlider->value() < 95) {
            this->playbackSlider->setValue(95);
            this->mixer->setPosition(95 * this->mixer->duration() / 100);
        } else {
             int id = this->soundList->currentRow() + 1;
            seekToTrack(id);
        }
    }

    void MainWindow::genShuffle() {
        int size = this->soundList->count();
        shufflePos = 0;

        shuffle.resize(size);

        for (int i = 0; i < size; i++) {
            shuffle[i] = i;
        }

        std::random_device rd;
        std::mt19937_64 g(rd());

        std::shuffle(shuffle.begin(), shuffle.end(), g);
    }

    void MainWindow::onMediaPlayerStatusChanged(QMediaPlayer::PlaybackState status) {
        if (status == QMediaPlayer::PlaybackState::StoppedState && isPlaying) {
            if (loopType == 0) {
                stopPlayback();
                this->mixer->setPosition(0);
            } else if(loopType == 1) {
                this->mixer->setPosition(0);
                startPlayback();
            } else if (loopType == 2) {
                int newTrack = this->currentId + 1;

                if (newTrack < this->soundList->count()) {
                    this->currentId = newTrack;
                    this->soundList->setCurrentRow(this->currentId);
                    setCurrentSong(this->soundList->item(this->currentId)->text().toStdString());
                } else {
                    this->currentId = 0;
                    this->soundList->setCurrentRow(this->currentId);
                    setCurrentSong(this->soundList->item(this->currentId)->text().toStdString());
                }

                startPlayback();
            } else if (loopType == 3) {
                if (shufflePos == shuffle.size() - 1) {
                    qDebug() << "Reached shuffle end; Generating a new one";
                    genShuffle();
                }

                this->currentId = shuffle[shufflePos];
                this->soundList->setCurrentRow(this->currentId);
                setCurrentSong(this->soundList->item(this->currentId)->text().toStdString());
                startPlayback();

                shufflePos++;
            }
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
