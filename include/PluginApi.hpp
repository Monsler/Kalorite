#pragma once

#include <QObject>
#include <QString>

namespace Kalorite {

    class Mixer;
    class PlaybackController;
    class Playlist;

    // Player surface exposed to LuaJIT plugins. Everything here runs on the GUI thread.
    class PluginApi : public QObject {
        Q_OBJECT

    public:
        PluginApi(PlaybackController* playback, Playlist* playlist, Mixer* mixer,
                  QObject* parent = nullptr);

        Mixer* mixer() const { return m_mixer; }

        void play();
        void pause();
        void stop();
        void next();
        void prev();
        bool isPlaying() const;
        int  positionMs() const;
        int  durationMs() const;
        void seekMs(int ms);

        int     playlistCount() const;
        void    playlistAdd(const QString& path);
        void    playlistRemove(int index);
        void    playlistClear();
        QString playlistPath(int index) const;
        QString playlistTitle(int index) const;
        int     currentIndex() const;
        void    playIndex(int index);

        void notify(const QString& text);

    signals:
        void pluginTrackChanged(QString path, int index);
        void pluginPlaybackStateChanged(QString state);
        void pluginTrackFinished(QString path);

        // Transport "next"/"prev" go through the same logic as the on-screen buttons,
        // including the progress-bar shortcut, so the window handles them.
        void nextRequested();
        void prevRequested();

        void statusMessage(QString text);

    private:
        PlaybackController* m_playback;
        Playlist* m_playlist;
        Mixer* m_mixer;
    };

} // namespace Kalorite
