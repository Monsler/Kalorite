#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QList>
#include <QColor>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace Kalorite {

    class PlaybackController;
    class Mixer;
    class Playlist;

    struct LyricsLine {
        qint64 timeMs = -1;
        QString text;
        QString timeStr;
    };

    class LyricsBackend : public QAbstractListModel {
        Q_OBJECT
        Q_PROPERTY(bool hasLyrics READ hasLyrics NOTIFY stateChanged)
        Q_PROPERTY(bool isSynced READ isSynced NOTIFY stateChanged)
        Q_PROPERTY(bool isLoading READ isLoading NOTIFY stateChanged)
        Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeIndexChanged)
        Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
        Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY trackInfoChanged)
        Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY trackInfoChanged)
        Q_PROPERTY(QString currentPath READ currentPath NOTIFY trackInfoChanged)
        Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY backgroundColorChanged)

    public:
        enum Roles {
            TextRole = Qt::UserRole + 1,
            TimeMsRole,
            TimeStrRole,
            IsActiveRole
        };

        explicit LyricsBackend(PlaybackController* playback, Mixer* mixer, Playlist* playlist, QObject* parent = nullptr);
        ~LyricsBackend() override;

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;

        bool hasLyrics() const { return m_hasLyrics; }
        bool isSynced() const { return m_isSynced; }
        bool isLoading() const { return m_isLoading; }
        int activeIndex() const { return m_activeIndex; }
        QString statusText() const { return m_statusText; }
        QString trackTitle() const { return m_trackTitle; }
        QString trackArtist() const { return m_trackArtist; }
        QString currentPath() const { return m_currentPath; }
        QColor backgroundColor() const { return m_backgroundColor; }

        Q_INVOKABLE void seekToLine(int index);
        Q_INVOKABLE void reload();
        Q_INVOKABLE void openLrcFile();
        Q_INVOKABLE void loadLrcFromPath(const QString& filePath);

    public slots:
        void onTrackLoaded(const QString& path);
        void onPlaybackPositionChanged(int posMs);
        void updateCoverColor();

    signals:
        void stateChanged();
        void activeIndexChanged(int index);
        void trackInfoChanged();
        void backgroundColorChanged();

    private slots:
        void handleLrclibGetReply(QNetworkReply* reply, const QString& path, const QString& title, const QString& artist, int durationSec);
        void handleLrclibSearchReply(QNetworkReply* reply, const QString& path);

    private:
        void loadForTrack(const QString& path);
        bool tryLoadLocalLrc(const QString& path);
        void queryLrclib(const QString& path, const QString& title, const QString& artist, int durationSec);
        void queryLrclibSearch(const QString& path, const QString& query);
        void parseLrcContent(const QString& content, bool saveToLocal = false, const QString& targetLrcPath = QString());
        void setLyricsLines(const QList<LyricsLine>& lines, bool synced);

        PlaybackController* m_playback = nullptr;
        Mixer* m_mixer = nullptr;
        Playlist* m_playlist = nullptr;
        QNetworkAccessManager* m_net = nullptr;

        QList<LyricsLine> m_lines;
        QString m_currentPath;
        QString m_trackTitle;
        QString m_trackArtist;
        QString m_statusText;
        QColor m_backgroundColor{26, 26, 30};

        int m_activeIndex = -1;
        bool m_hasLyrics = false;
        bool m_isSynced = false;
        bool m_isLoading = false;
    };

} // namespace Kalorite
