#include "PlaylistModel.hpp"
#include <QDebug>

namespace Kalorite {

PlaylistModel::PlaylistModel(QObject* parent) : QAbstractListModel(parent) {}

int PlaylistModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_visibleRows.size();
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleRows.size()) return {};

    int trackIdx = m_visibleRows.at(index.row());
    if (trackIdx < 0 || trackIdx >= m_tracks.size()) return {};

    const Track& track = m_tracks.at(trackIdx);
    switch (role) {
    case PathRole:          return track.path;
    case NameRole:          return track.name;
    case DurationRole:      return track.duration;
    case QueueRole:         return track.queue;
    case OriginalIndexRole: return trackIdx;
    case TitleRole:         return track.title.isEmpty() ? track.name : track.title;
    case ArtistRole:        return track.artist;
    case HasCoverRole:      return track.hasCover;
    case CoverUrlRole:      return track.hasCover ? (QStringLiteral("image://cover/") + QString::number(track.id) + QStringLiteral("?v=") + QString::number(track.coverVersion)) : QString();
    }
    return {};
}

QHash<int, QByteArray> PlaylistModel::roleNames() const {
    return {
        { PathRole,          "path" },
        { NameRole,          "name" },
        { DurationRole,      "duration" },
        { QueueRole,         "queue" },
        { OriginalIndexRole, "originalIndex" },
        { TitleRole,         "title" },
        { ArtistRole,        "artist" },
        { HasCoverRole,      "hasCover" },
        { CoverUrlRole,      "coverUrl" },
    };
}

void PlaylistModel::setCurrentRow(int row) {
    if (row < 0 || row >= count()) row = -1;
    if (row == m_currentRow) return;

    m_currentRow = row;
    emit currentRowChanged();
}

void PlaylistModel::setFilter(const QString& filter) {
    if (m_filter == filter) return;
    m_filter = filter;
    updateVisibleRows();
    emit filterChanged();
}

bool PlaylistModel::matchesFilter(const Track& track) const {
    if (m_filter.trimmed().isEmpty()) return true;
    return track.title.contains(m_filter, Qt::CaseInsensitive)
        || track.artist.contains(m_filter, Qt::CaseInsensitive)
        || track.name.contains(m_filter, Qt::CaseInsensitive)
        || track.path.contains(m_filter, Qt::CaseInsensitive);
}

void PlaylistModel::updateVisibleRows() {
    beginResetModel();
    m_visibleRows.clear();
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (matchesFilter(m_tracks.at(i))) {
            m_visibleRows.append(i);
        }
    }
    endResetModel();
}

QString PlaylistModel::pathAt(int row) const {
    if (row < 0 || row >= count()) return QString();
    return m_tracks.at(row).path;
}

QString PlaylistModel::durationAt(int row) const {
    if (row < 0 || row >= count()) return QString();
    return m_tracks.at(row).duration;
}

QString PlaylistModel::titleAt(int row) const {
    if (row < 0 || row >= count()) return QString();
    const Track& t = m_tracks.at(row);
    return t.title.isEmpty() ? t.name : t.title;
}

QString PlaylistModel::artistAt(int row) const {
    if (row < 0 || row >= count()) return QString();
    return m_tracks.at(row).artist;
}

bool PlaylistModel::hasCoverAt(int row) const {
    if (row < 0 || row >= count()) return false;
    return m_tracks.at(row).hasCover;
}

QImage PlaylistModel::coverAt(int row) const {
    if (row < 0 || row >= count()) return {};
    return m_tracks.at(row).cover;
}

QImage PlaylistModel::coverById(quint64 id) const {
    for (const Track& t : m_tracks) {
        if (t.id == id) return t.cover;
    }
    return {};
}

int PlaylistModel::queueAt(int row) const {
    if (row < 0 || row >= count()) return 0;
    return m_tracks.at(row).queue;
}

void PlaylistModel::setQueue(int row, int queue) {
    if (row < 0 || row >= count()) return;

    m_tracks[row].queue = queue;
    int vRow = m_visibleRows.indexOf(row);
    if (vRow >= 0) {
        emit dataChanged(index(vRow), index(vRow), { QueueRole });
    }
}

void PlaylistModel::setDuration(int row, const QString& duration) {
    if (row < 0 || row >= count()) return;

    m_tracks[row].duration = duration;
    int vRow = m_visibleRows.indexOf(row);
    if (vRow >= 0) {
        emit dataChanged(index(vRow), index(vRow), { DurationRole });
    }
}

quint64 PlaylistModel::append(const QString& path,
                              const QString& name,
                              const QString& title,
                              const QString& artist,
                              const QString& duration,
                              const QImage& cover,
                              bool hasCover) {
    const int origIdx = count();
    const quint64 trackId = m_nextTrackId++;
    Track track{ trackId, path, name, title, artist, duration, 0, cover, hasCover, 1 };
    m_tracks.append(track);

    if (matchesFilter(track)) {
        const int vRow = m_visibleRows.size();
        beginInsertRows(QModelIndex(), vRow, vRow);
        m_visibleRows.append(origIdx);
        endInsertRows();
    }
    return trackId;
}

void PlaylistModel::updateMetadataById(quint64 trackId,
                                      const QString& title,
                                      const QString& artist,
                                      const QImage& cover,
                                      bool hasCover) {
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].id == trackId) {
            bool changed = false;
            if (!title.isEmpty() && m_tracks[i].title != title) {
                m_tracks[i].title = title;
                changed = true;
            }
            if (!artist.isEmpty() && m_tracks[i].artist != artist) {
                m_tracks[i].artist = artist;
                changed = true;
            }
            if (changed) {
                m_tracks[i].name = !m_tracks[i].artist.isEmpty()
                    ? (m_tracks[i].artist + QStringLiteral(" - ") + m_tracks[i].title)
                    : m_tracks[i].title;
            }
            if (hasCover && !cover.isNull()) {
                m_tracks[i].cover = cover;
                m_tracks[i].hasCover = true;
                m_tracks[i].coverVersion++;
                changed = true;
            }
            if (changed) {
                int vRow = m_visibleRows.indexOf(i);
                qDebug() << "[PlaylistModel] Updated track metadata: row=" << vRow
                         << "title=" << m_tracks[i].title
                         << "artist=" << m_tracks[i].artist
                         << "hasCover=" << m_tracks[i].hasCover
                         << "coverVersion=" << m_tracks[i].coverVersion;
                if (vRow >= 0) {
                    emit dataChanged(index(vRow), index(vRow),
                                     { TitleRole, ArtistRole, NameRole, HasCoverRole, CoverUrlRole });
                }
            }
            break;
        }
    }
}

void PlaylistModel::removeAt(int row) {
    if (row < 0 || row >= count()) return;

    m_tracks.removeAt(row);
    if (row < m_currentRow) --m_currentRow;
    if (m_currentRow >= count()) m_currentRow = count() - 1;

    updateVisibleRows();
    emit currentRowChanged();
}

void PlaylistModel::clear() {
    beginResetModel();
    m_tracks.clear();
    m_visibleRows.clear();
    m_currentRow = -1;
    endResetModel();
    emit currentRowChanged();
}

// Qt expects the insertion point for a downward move to be one past the target row.
// The current row follows the moved track, or shifts when it sits between the two positions.
void PlaylistModel::moveTrack(int from, int to) {
    if (!m_filter.trimmed().isEmpty()) return;
    if (from == to || from < 0 || from >= count() || to < 0 || to >= count()) return;
    if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to)) return;

    const int previousRow = m_currentRow;
    if (m_currentRow == from)                              m_currentRow = to;
    else if (from < m_currentRow && m_currentRow <= to)   --m_currentRow;
    else if (to <= m_currentRow && m_currentRow < from)   ++m_currentRow;

    m_tracks.move(from, to);
    for (int i = 0; i < m_visibleRows.size(); ++i) {
        m_visibleRows[i] = i;
    }
    endMoveRows();

    const int low = qMin(from, to);
    const int high = qMax(from, to);
    emit dataChanged(index(low), index(high), { OriginalIndexRole });

    if (m_currentRow != previousRow) emit currentRowChanged();
}

int PlaylistModel::originalIndex(int visibleRow) const {
    if (visibleRow < 0 || visibleRow >= m_visibleRows.size()) return -1;
    return m_visibleRows.at(visibleRow);
}

} // namespace Kalorite
