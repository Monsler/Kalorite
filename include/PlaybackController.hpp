#pragma once

#include <QObject>
#include <QString>
#include <string>
#include <vector>

class QTimer;

namespace Kalorite {

    class Mixer;
    class Playlist;

    // Transport state machine: play/pause, track selection, loop modes, shuffle, the
    // priority queue and end-of-track handling. Holds no widgets; the window listens
    // to the signals below to update the UI.
    class PlaybackController : public QObject {
        Q_OBJECT

    public:
        PlaybackController(Mixer* mixer, Playlist* playlist, QObject* parent = nullptr);

        Mixer* mixer() const { return m_mixer; }
        Playlist* playlist() const { return m_playlist; }
        bool isPlaying() const { return m_isPlaying; }
        int loopType() const { return m_loopType; }
        const std::string& currentAudio() const { return m_currentAudio; }

        bool crossfadeEnabled() const { return m_crossfadeEnabled; }
        bool smartGainEnabled() const { return m_smartGainEnabled; }
        bool bitPerfectEnabled() const { return m_bitPerfectEnabled; }
        void setCrossfadeEnabled(bool enabled) { m_crossfadeEnabled = enabled; }
        void setSmartGainEnabled(bool enabled) { m_smartGainEnabled = enabled; }
        void setBitPerfectEnabled(bool enabled) { m_bitPerfectEnabled = enabled; }

        void play();
        void pause();
        void stop();
        void togglePlay();
        void seekMs(int ms);

        void selectTrack(int row, bool restoreSavedPosition = false);
        void skipNext();
        void skipPrevious();
        void cycleLoopType();
        void toggleQueue(int row);

        // Adds a track to the playlist. The first track ever added becomes the loaded one.
        void addTrack(const QString& path, const QString& displayName = QString(), qint64 durationMs = -1, double progress = 0.0, qint64 positionMs = 0);
        void loadTrack(const std::string& path);
        // Plays a file outside the playlist (startup greeting); plugins are not notified.
        void playDetached(const QString& path);

    signals:
        void playStateChanged(bool playing);
        void pluginStateChanged(QString state);
        void trackLoaded(QString path);
        void trackFinished(QString path);
        void loopTypeChanged(int loopType);
        void tick();
        void controlsAvailable();

    private slots:
        void onTimer();
        void onCrossfadeAdvance();
        void onRowsReordered();

    private:
        int takeNextQueuedTrack();
        void genShuffle();
        int getNextShuffleTrack();

        Mixer* m_mixer;
        Playlist* m_playlist;
        QTimer* m_timer;

        std::string m_currentAudio;
        bool m_isPlaying = false;
        int m_currentId = 0;
        int m_loopType = 0;
        std::vector<int> m_shuffle;
        int m_shufflePos = 0;
        bool m_crossfadeEnabled = false;
        bool m_smartGainEnabled = false;
        bool m_bitPerfectEnabled = false;
    };

} // namespace Kalorite
