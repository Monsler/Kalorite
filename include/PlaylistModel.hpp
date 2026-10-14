#pragma once

#include <QAbstractListModel>
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
        int queueAt(int row) const;
        void setQueue(int row, int queue);
        void setDuration(int row, const QString& duration);

        void append(const QString& path, const QString& name, const QString& duration = QStringLiteral("--:--"));
        void removeAt(int row);
        void clear();

        Q_INVOKABLE void moveTrack(int from, int to);

    signals:
        void currentRowChanged();
        void filterChanged();

    private:
        struct Track {
            QString path;
            QString name;
            QString duration = QStringLiteral("--:--");
            int queue = 0;
        };

        bool matchesFilter(const Track& track) const;
        void updateVisibleRows();

        QList<Track> m_tracks;
        QList<int> m_visibleRows;
        QString m_filter;
        int m_currentRow = -1;
    };

} // namespace Kalorite
