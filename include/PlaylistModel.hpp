#pragma once

#include <QAbstractListModel>
#include <QImage>
#include <QList>
#include <QString>

namespace Kalorite {

    // Track rows for the playlist, exposed to QML as `playlistModel`.
    class PlaylistModel : public QAbstractListModel {
        Q_OBJECT
        Q_PROPERTY(int currentRow READ currentRow NOTIFY currentRowChanged)
        Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

    public:
        enum Roles {
            PathRole = Qt::UserRole,
            NameRole,
            DurationRole,
            QueueRole,
            OriginalIndexRole,
            TitleRole,
            ArtistRole,
            HasCoverRole,
            CoverUrlRole,
        };

        explicit PlaylistModel(QObject* parent = nullptr);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        QHash<int, QByteArray> roleNames() const override;

        int count() const { return m_tracks.size(); }
        int visibleCount() const { return m_visibleRows.size(); }
        int currentRow() const { return m_currentRow; }
        void setCurrentRow(int row);

        QString filter() const { return m_filter; }
        void setFilter(const QString& filter);

        QString pathAt(int row) const;
        QString durationAt(int row) const;
        QString titleAt(int row) const;
        QString artistAt(int row) const;
        bool hasCoverAt(int row) const;
        QImage coverAt(int row) const;
        QImage coverById(quint64 id) const;
        int queueAt(int row) const;
        void setQueue(int row, int queue);
        void setDuration(int row, const QString& duration);

        quint64 append(const QString& path,
                       const QString& name,
                       const QString& title,
                       const QString& artist,
                       const QString& duration = QStringLiteral("--:--"),
                       const QImage& cover = QImage(),
                       bool hasCover = false);
        void updateMetadataById(quint64 trackId,
                                const QString& title,
                                const QString& artist,
                                const QImage& cover,
                                bool hasCover);
        void removeAt(int row);
        void clear();

        Q_INVOKABLE void moveTrack(int from, int to);
        Q_INVOKABLE int originalIndex(int visibleRow) const;

    signals:
        void currentRowChanged();
        void filterChanged();

    private:
        struct Track {
            quint64 id = 0;
            QString path;
            QString name;
            QString title;
            QString artist;
            QString duration = QStringLiteral("--:--");
            int queue = 0;
            QImage cover;
            bool hasCover = false;
            int coverVersion = 1;
        };

        bool matchesFilter(const Track& track) const;
        void updateVisibleRows();

        QList<Track> m_tracks;
        QList<int> m_visibleRows;
        QString m_filter;
        int m_currentRow = -1;
        quint64 m_nextTrackId = 1;
    };

} // namespace Kalorite
