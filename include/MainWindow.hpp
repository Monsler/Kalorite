#pragma once

#include <QQuickView>
#include <QString>
#include <QPointF>
#include "Settings.hpp"
#include "PlasmaPercent.hpp"
#include "MenuNode.hpp"

namespace Kalorite {

    class Mixer;
    class PlaybackController;
    class Playlist;
    class EqualizerBackend;
    class PlaylistBackend;
    class PluginApi;
    class PluginManager;
    class ClipboardWatcher;
    class SongDownloader;
    class AboutDialog;
    class WinampDisplay;
    class PatternVisualizer;


    class MainWindow : public QQuickView {
        Q_OBJECT
        Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playStateChanged)
        Q_PROPERTY(int loopType READ loopType NOTIFY loopTypeChanged)
        Q_PROPERTY(QString repeatIcon READ repeatIcon NOTIFY loopTypeChanged)
        Q_PROPERTY(QString repeatTooltip READ repeatTooltip NOTIFY loopTypeChanged)
        Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
        Q_PROPERTY(bool controlsEnabled READ controlsEnabled NOTIFY controlsEnabledChanged)
        Q_PROPERTY(bool patternVizVisible READ patternVizVisible WRITE setPatternVizVisible NOTIFY patternVizVisibleChanged)
        Q_PROPERTY(bool eqVisible READ eqVisible WRITE setEqVisible NOTIFY eqVisibleChanged)
        Q_PROPERTY(bool playlistVisible READ playlistVisible WRITE setPlaylistVisible NOTIFY playlistVisibleChanged)
        Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
        Q_PROPERTY(Kalorite::MenuNode* rootMenu READ rootMenu CONSTANT)
        Q_PROPERTY(Kalorite::MenuNode* contextMenu READ contextMenu CONSTANT)

    public:
        MainWindow();
        ~MainWindow() override;

        // Called by SongDownloader once a download completes.
        void addSoundFile(const QString& filePath, const QString& displayName = QString(), qint64 durationMs = -1);

        bool isPlaying() const;
        int loopType() const;
        QString repeatIcon() const;
        QString repeatTooltip() const;
        int progress() const { return m_progress; }
        bool controlsEnabled() const { return m_controlsEnabled; }
        bool patternVizVisible() const { return m_patternVizVisible; }
        void setPatternVizVisible(bool visible);
        bool eqVisible() const { return m_eqVisible; }
        void setEqVisible(bool visible);
        bool playlistVisible() const { return m_playlistVisible; }
        void setPlaylistVisible(bool visible);
        QString statusMessage() const { return m_statusMessage; }

        MenuNode* rootMenu() const { return m_rootMenu; }
        MenuNode* contextMenu() const { return m_contextMenu; }

    public slots:
        void onPlayClicked();
        void onSkipNext();
        void onSkipBack();
        void onRepeatClicked();
        void onSliderPressed();
        void onSliderReleased();
        void onSeek(int value);
        void onSavePlaylist();
        void onLoadPlaylist();
        void openButtonTriggered();
        void addFolderTriggered();
        void openAudioCdTriggered();
        void openSoundFileDownloadDialog();
        void openAddPluginDialog();
        void openAboutDialog();
        void onVolumeChanged(int value);

    signals:
        void playStateChanged();
        void loopTypeChanged();
        void progressChanged();
        void controlsEnabledChanged();
        void patternVizVisibleChanged();
        void eqVisibleChanged();
        void playlistVisibleChanged();
        void statusMessageChanged();

    protected:
        void closeEvent(QCloseEvent* event) override;

    private slots:
        void onPlayStateChanged(bool playing);
        void onTrackLoaded(const QString& path);
        void onTick();
        void onLoopTypeChangedSlot(int loopType);
        void enableControls();
        void onTrackRemoved();
        void setModernMode(bool modern);

    private:
        void applyLoadedSettings();
        void updateProgress();
        void playFirstRunGreeting();
        void setupMenus();
        void rebuildContextMenu();
        void showStatusMessage(const QString& text, int timeoutMs = 5000);

        Settings m_settings;
        Mixer* mixer = nullptr;
        Playlist* playlist = nullptr;
        PlaybackController* playback = nullptr;
        EqualizerBackend* eqBackend = nullptr;
        PlaylistBackend* playlistBackend = nullptr;
        PluginApi* pluginApi = nullptr;
        PluginManager* pluginManager = nullptr;
        ClipboardWatcher* clipboardWatcher = nullptr;
        SongDownloader* songDownloader = nullptr;
        AboutDialog* aboutDialog = nullptr;

        WinampDisplay* winampDisplay = nullptr;
        PatternVisualizer* patternVisualizer = nullptr;

        MenuNode* m_rootMenu = nullptr;
        MenuNode* m_contextMenu = nullptr;
        MenuNode* m_fileMenu = nullptr;
        MenuNode* m_viewMenu = nullptr;
        MenuNode* m_pluginsMenu = nullptr;
        MenuNode* m_helpMenu = nullptr;
        MenuNode* m_retroAction = nullptr;
        MenuNode* m_modernAction = nullptr;
        MenuNode* m_showPatternVizAction = nullptr;

        PlasmaPercent percent;

        int m_progress = 0;
        bool m_controlsEnabled = false;
        bool m_patternVizVisible = true;
        bool m_eqVisible = false;
        bool m_playlistVisible = true;
        QString m_statusMessage;
    };

} // namespace Kalorite
