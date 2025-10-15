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
        void onRepeatButtonTriggered();
        void onContextMenuSoundList(const QPoint &pos);

        private:
        void setCurrentSong(const std::string path);
        void startPlayback();
        void stopPlayback();
        void updateTimeLabel();
        void setPlaybackPos(const int percent);
        void seekToTrack(const int id);
        void genShuffle();

        protected:
        void closeEvent(QCloseEvent* event) override;

        QMenuBar* currentMenuBar;
        QMenu* fileMenu;

        QPushButton* playbackButton;
        QPushButton* skipBackButton;
        QPushButton* skipForwardButton;
        QPushButton* repeatButton;

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
        int loopType = 0;
        std::vector<int> shuffle;
        int shufflePos = 0;

        PlasmaPercent percent;
    };
} // namespace Kalorite
