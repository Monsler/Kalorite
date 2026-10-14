#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace Kalorite {

    class PlaylistModel;
    class MusicBrainzService;

    // Playlist logic on top of PlaylistModel: adding, queueing, durations and file I/O.
    class Playlist : public QObject {
        Q_OBJECT

    public:
        explicit Playlist(QObject* parent = nullptr);

        PlaylistModel* model() const { return m_model; }
        MusicBrainzService* musicBrainz() const { return m_musicBrainz; }
        int count() const;
        int currentRow() const;
        void setCurrentRow(int row);
        QString pathAt(int row) const;
        QString titleAt(int row) const;
        QString artistAt(int row) const;
        bool hasCoverAt(int row) const;
        int rowForPath(const QString& path) const;

        // Returns false (and changes nothing) for an empty or already-listed path.
        bool addTrack(const QString& path, const QString& displayName = QString(), qint64 durationMs = -1);
        void removeRow(int row);
        void clear();
        void setRowDuration(int row, qint64 durationMs);

        static QString formatDuration(qint64 durationMs);
        static qint64 getAudioDurationMs(const QString& path);

        void toggleQueue(int row);
        void clearQueueEntry(int row);
        void clearAllQueues();
        int takeQueueHead();

        void saveToFile(const QString& filePath) const;
        static QStringList readPathsFromFile(const QString& filePath);

    signals:
        // Emitted whenever the set or order of tracks changes; shuffle order depends on it.
        void contentsChanged();
        // Emitted after a drag reorder.
        void rowsReordered();

    private:
        void onRowsMoved();
        void closeQueueGap(int slot);

        PlaylistModel* m_model;
        MusicBrainzService* m_musicBrainz;
    };

} // namespace Kalorite
