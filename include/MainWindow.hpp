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
#include <QMediaDevices>
#include <QAudioDevice>
#include <qspinbox.h>
#include <qcheckbox.h>
#include <qframe.h>
#include <QClipboard>
#include "SongDownloader.hpp"
#include "WinampDisplay.hpp"
#include "PatternVisualizer.hpp"
#include "VolumeSignalWidget.hpp"
#include <nlohmann/json.hpp>

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
        void    addSoundFile(const QString& filePath, const QString& displayName = QString());

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
        void onRowsReordered();
        void savePlaylistTriggered();
        void loadPlaylistTriggered();
        void openSoundFileDownloadDialog();
        void addFolderTriggered();
        void openAudioCdTriggered();
        void onDurationChanged(qint64 durationMs);
        void onContextMenuWinampDisplay(const QPoint &pos);
        void openAddPluginDialog();
        void openAboutDialog();
        void openAddSkinDialog();
        void onClipboardChanged();

        private:

        // Persistent app settings (general-purpose JSON store for the future).
        QString settingsFilePath() const;
        void loadSettings();
        void saveSettings();
        // Apply the persisted context-menu settings to the mixer / displays once
        // all the widgets have been constructed.
        void applyLoadedSettings();

        // Plays the embedded greeting jingle once, on the very first launch.
        void playFirstRunGreeting();

        void startPlayback();
        void stopPlayback();
        void updateTimeLabel();
        void setPlaybackPos(const int percent);
        void seekToTrack(const int id);
        void genShuffle();

        // Playlist item widget + priority queue helpers.
        void buildItemWidget(class QListWidgetItem* item);
        void refreshQueueLabels();
        void toggleQueueForRow(int row);
        void clearQueueEntryForRow(int row);
        int  takeQueueHead();
        int  takeNextQueuedTrack();

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

        // Live audio output device list: filled at startup and kept in sync
        // via QMediaDevices::audioOutputsChanged.
        QMediaDevices* mediaDevices = nullptr;
        QList<QAudioDevice> audioDevices;

        PlasmaPercent percent;
        WinampDisplay* winampDisplay;
        PatternVisualizer* patternVisualizer;
        QAction* showPatternVizAction = nullptr;

        QString m_currentSkinName = "system";
        QPalette m_defaultPalette;
        void applySkin(const QString& skinName);
        void applyDarkPalette(const QColor& accentColor, const QColor& bgColor, const QColor& surfaceColor);
        void populateSkinsMenu(QMenu* menu);

        // General-purpose persisted settings (loaded at startup, saved on change
        // and on close). Keep new options here so everything lands in one file.
        nlohmann::json m_settings;

        QString m_lastClipboardUrl;
    };
} // namespace Kalorite
