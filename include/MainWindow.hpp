#include <qobject.h>
#define let auto

#pragma once
#include "Mixer.hpp"
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
#include <qcheckbox.h>
#include <qframe.h>
#include "SongDownloader.hpp"
#include "WinampDisplay.hpp"
#include "VolumeSignalWidget.hpp"

namespace Kalorite
{

    class PluginManager;

    bool containsItem(QListWidget *list, const QString& text);

    class MainWindow : public QMainWindow {
        Q_OBJECT

        public:
        MainWindow();
        void setCurrentSong(const std::string path);
        QListWidget* soundList;

        // ---- Plugin API surface -------------------------------------------
        // These wrap the private transport/playlist logic so the LuaJIT plugin
        // layer never has to touch internals directly. All are GUI-thread only.
        Mixer* pluginMixer() { return mixer; }

        void pluginPlay();
        void pluginPause();
        void pluginStop();
        void pluginNext();
        void pluginPrev();
        bool pluginIsPlaying() const { return isPlaying; }
        int  pluginPositionMs();
        int  pluginDurationMs();
        void pluginSeekMs(int ms);

        int     pluginPlaylistCount() const;
        void    pluginPlaylistAdd(const QString& path);
        void    pluginPlaylistRemove(int index);
        void    pluginPlaylistClear();
        QString pluginPlaylistPath(int index) const;
        QString pluginPlaylistTitle(int index) const;
        int     pluginCurrentIndex() const;
        void    pluginPlayIndex(int index);

    signals:
        // Emitted on the GUI thread; PluginManager fans these out to plugins.
        void pluginTrackChanged(QString path, int index);
        void pluginPlaybackStateChanged(QString state); // "playing"/"paused"/"stopped"
        void pluginTrackFinished(QString path);

        private slots:
        void onPlayTriggered();
        void openButtonTriggered();
        void onPlayback();
        void onSkipNext();
        void onCrossfadeAdvance();
        void onSkipBack();
        void onMediaPlayerStatusChanged(QMediaPlayer::PlaybackState status);
        void playbackSliderPressed();
        void playbackSliderReleased();
        void onListSelection(QListWidgetItem *item);
        void onSpinTriggered(const int value);
        void onRepeatButtonTriggered();
        void onContextMenuSoundList(const QPoint &pos);
        void savePlaylistTriggered();
        void loadPlaylistTriggered();
        void openSoundFileDownloadDialog();
        void addFolderTriggered();
        void onDurationChanged(qint64 durationMs);
        void addSoundFile(const QString& filePath);
        void onContextMenuWinampDisplay(const QPoint &pos);
        void openAddPluginDialog();
        void openAboutDialog();

        private:

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


        Mixer* mixer;
        SongDownloader* songDownloader;
        PluginManager* pluginManager = nullptr;
        QMenu* pluginsMenu = nullptr;

        QTimer* playbackTimer;

        VolumeSignalWidget* volumeSignal;
        QWidget* playlistContainer;
        QPushButton* playlistToggleBtn;

        QPushButton* eqToggleBtn;
        QWidget* eqContainer;
        QCheckBox* eqEnabledCheckbox;
        QFrame* eqSeparator;
        QList<QSlider*> eqSliders;

        std::string currentAudio;
        bool isPlaying = false;
        int trackLengthSeconds = 0;
        int currentId;
        int loopType = 0;
        std::vector<int> shuffle;
        int shufflePos = 0;
        bool m_crossfadeEnabled = false;
        bool m_bitPerfectEnabled = false;
        bool m_smartGainEnabled = false;

        PlasmaPercent percent;
        WinampDisplay* winampDisplay;
    };
} // namespace Kalorite
