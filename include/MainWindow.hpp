#pragma once
#include "PlasmaPercent.hpp"
#include <qtmetamacros.h>
#include <qmainwindow.h>
#include <qmenubar.h>
#include <qpushbutton.h>
#include <QVBoxLayout>
#include <qslider.h>
#include <qlabel.h>
#include <qlistwidget.h>
#include <qmediaplayer.h>
#include <qspinbox.h>

namespace Kalorite
{
    class MainWindow : public QMainWindow {
        Q_OBJECT

        public:
        MainWindow();

        private slots:
        void onPlayTriggered();
        void openButtonTriggered();
        void onPlayback();
        void onSkipNext();
        void onSkipBack();
        void onMediaPlayerStatusChanged(QMediaPlayer::PlaybackState status);
        void playbackSliderPressed();
        void playbackSliderReleased();
        void onListSelection(QListWidgetItem *item);
        void onSpinTriggered(const int value);

        private:
        void setCurrentSong(const std::string path);
        void startPlayback();
        void stopPlayback();
        void updateTimeLabel();
        void setPlaybackPos(const int percent);
        void seekToTrack(const int id);

        protected:
        void closeEvent(QCloseEvent* event) override;

        QMenuBar* currentMenuBar;
        QMenu* fileMenu;

        QPushButton* playbackButton;
        QPushButton* skipBackButton;
        QPushButton* skipForwardButton;

        QVBoxLayout* mainLayout;
        QHBoxLayout* playerLayout;
        QHBoxLayout* songsControlLayout;
        QSlider* playbackSlider;
        QWidget* centralWidget;
        QLabel* timeLabel;
        QListWidget* soundList;
        QMediaPlayer* mixer;
        QAudioOutput* output;
        QTimer* playbackTimer;

        QSpinBox* volumeBox;
        
        std::string currentAudio;
        bool isPlaying = false;
        int trackLengthSeconds = 0;
        int currentId;
        float volume = 1.0f;

        PlasmaPercent percent;
    };
} // namespace Kalorite
