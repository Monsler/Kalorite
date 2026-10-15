#include "PlaylistModel.hpp"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <vector>

namespace Kalorite {

static double relLuminance(const QColor& c) {
    auto toLin = [](double v) {
        return (v <= 0.04045) ? (v / 12.92) : std::pow((v + 0.055) / 1.055, 2.4);
    };
    double r = toLin(c.redF());
    double g = toLin(c.greenF());
    double b = toLin(c.blueF());
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

QColor PlaylistModel::extractDominantColor(const QImage& img) {
    if (img.isNull()) {
        return QColor(26, 26, 30);
    }
    QImage tiny = img.scaled(24, 24, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);
    qint64 totalR = 0, totalG = 0, totalB = 0;
    int count = 0;
    for (int y = 0; y < tiny.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(tiny.constScanLine(y));
        for (int x = 0; x < tiny.width(); ++x) {
            QRgb p = line[x];
            if (qAlpha(p) > 50) {
                totalR += qRed(p);
                totalG += qGreen(p);
                totalB += qBlue(p);
                count++;
            }
        }
    }
    if (count == 0) return QColor(26, 26, 30);

    QColor avg(static_cast<int>(totalR / count),
               static_cast<int>(totalG / count),
               static_cast<int>(totalB / count));

    float h = avg.hueF();
    float s = avg.saturationF();
    float v = avg.valueF();
    if (h < 0.0f) h = 0.0f;

    // Spotify style: deep, muted atmospheric tint
    s = qBound(0.35f, s * 1.25f, 0.70f);
    v = qBound(0.14f, v * 0.40f, 0.28f);

    return QColor::fromHsvF(h, s, v);
}

// Extracts the color from the cover palette that provides the best contrast
// with the bottom region of the cover (where the progress strip is located).
QColor PlaylistModel::extractProgressAccentColor(const QImage& img) {
    if (img.isNull()) {
        return QColor(100, 140, 210);
    }

    QImage scaled = img.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);

    // 1. Analyze bottom region (bottom 25%, rows 36-47)
    qint64 bR = 0, bG = 0, bB = 0;
    int bCount = 0;
    for (int y = 36; y < 48; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(scaled.constScanLine(y));
        for (int x = 0; x < 48; ++x) {
            QRgb p = line[x];
            if (qAlpha(p) > 50) {
                bR += qRed(p);
                bG += qGreen(p);
                bB += qBlue(p);
                bCount++;
            }
        }
    }

    QColor bottomColor = (bCount > 0)
        ? QColor(static_cast<int>(bR / bCount), static_cast<int>(bG / bCount), static_cast<int>(bB / bCount))
        : QColor(20, 20, 20);

    double bottomLum = relLuminance(bottomColor);

    // 2. Quantize colors from the whole image into 8x8x8 = 512 buckets (32 levels per channel)
    struct Bucket {
        qint64 r = 0, g = 0, b = 0;
        int count = 0;
    };
    Bucket buckets[8][8][8];

    for (int y = 0; y < scaled.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(scaled.constScanLine(y));
        for (int x = 0; x < scaled.width(); ++x) {
            QRgb p = line[x];
            if (qAlpha(p) > 50) {
                int ri = qBound(0, qRed(p) / 32, 7);
                int gi = qBound(0, qGreen(p) / 32, 7);
                int bi = qBound(0, qBlue(p) / 32, 7);
                buckets[ri][gi][bi].r += qRed(p);
                buckets[ri][gi][bi].g += qGreen(p);
                buckets[ri][gi][bi].b += qBlue(p);
                buckets[ri][gi][bi].count++;
            }
        }
    }

    struct Candidate {
        QColor color;
        int count = 0;
    };
    std::vector<Candidate> candidates;
    for (int r = 0; r < 8; ++r) {
        for (int g = 0; g < 8; ++g) {
            for (int b = 0; b < 8; ++b) {
                const auto& bkt = buckets[r][g][b];
                // Ignore tiny noise (< 0.25% of image, i.e. < 6 pixels)
                if (bkt.count >= 6) {
                    candidates.push_back({
                        QColor(static_cast<int>(bkt.r / bkt.count),
                               static_cast<int>(bkt.g / bkt.count),
                               static_cast<int>(bkt.b / bkt.count)),
                        bkt.count
                    });
                }
            }
        }
    }

    if (candidates.empty()) {
        return bottomLum < 0.5 ? QColor(200, 200, 210) : QColor(40, 40, 50);
    }

    // 3. Find the candidate that contrasts best with bottomColor
    QColor bestColor = candidates[0].color;
    double bestScore = -1e9;
    double bestContrastRatio = 1.0;

    for (const auto& cand : candidates) {
        double candLum = relLuminance(cand.color);
        double maxL = std::max(candLum, bottomLum);
        double minL = std::min(candLum, bottomLum);
        double cr = (maxL + 0.05) / (minL + 0.05);

        float s = cand.color.saturationF();
        float v = cand.color.valueF();

        // Euclidean color distance in RGB
        double dr = (cand.color.redF() - bottomColor.redF());
        double dg = (cand.color.greenF() - bottomColor.greenF());
        double db = (cand.color.blueF() - bottomColor.blueF());
        double colorDist = std::sqrt(dr * dr * 0.299 + dg * dg * 0.587 + db * db * 0.114);

        // Score based on contrast ratio, color distance, and saturation
        double score = cr * 2.0 + colorDist * 5.0 + s * 4.0 + std::log1p(cand.count) * 0.3;

        // If bottom is dark, penalize very dark colors
        if (bottomLum < 0.2 && v < 0.35) score -= 10.0;
        // If bottom is light, penalize very bright washed-out colors
        if (bottomLum > 0.6 && v > 0.85 && s < 0.2) score -= 10.0;

        if (score > bestScore) {
            bestScore = score;
            bestColor = cand.color;
            bestContrastRatio = cr;
        }
    }

    // 4. If contrast ratio is low (e.g. monochrome / low-contrast cover), ensure visibility
    if (bestContrastRatio < 3.0) {
        float h = bestColor.hueF();
        float s = bestColor.saturationF();
        float v = bestColor.valueF();
        if (h < 0.0f) h = 0.0f;

        if (bottomLum < 0.5) {
            // Lighten for dark bottom
            v = std::max(v, 0.75f);
            s = qBound(0.40f, s * 1.3f, 0.90f);
        } else {
            // Darken for light bottom
            v = std::min(v, 0.25f);
            s = std::max(s, 0.50f);
        }
        return QColor::fromHsvF(h, s, v);
    }

    // Slight polish: keep selected hue, ensure sufficiently vivid against background
    if (bottomLum < 0.4) {
        float h = bestColor.hueF();
        float s = bestColor.saturationF();
        float v = bestColor.valueF();
        if (h < 0.0f) h = 0.0f;
        v = std::max(v, 0.55f);
        s = std::max(s, 0.35f);
        return QColor::fromHsvF(h, s, v);
    } else if (bottomLum > 0.6) {
        float h = bestColor.hueF();
        float s = bestColor.saturationF();
        float v = bestColor.valueF();
        if (h < 0.0f) h = 0.0f;
        v = std::min(v, 0.40f);
        return QColor::fromHsvF(h, s, v);
    }

    return bestColor;
}

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
    case ProgressRole:      return track.progress;
    case PositionMsRole:    return track.positionMs;
    case AccentColorRole:   return track.accentColor;
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
        { ProgressRole,      "progress" },
        { PositionMsRole,    "positionMs" },
        { AccentColorRole,   "accentColor" },
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

double PlaylistModel::progressAt(int row) const {
    if (row < 0 || row >= count()) return 0.0;
    return m_tracks.at(row).progress;
}

qint64 PlaylistModel::positionMsAt(int row) const {
    if (row < 0 || row >= count()) return 0;
    return m_tracks.at(row).positionMs;
}

void PlaylistModel::setProgress(int row, double progress, qint64 positionMs) {
    if (row < 0 || row >= m_tracks.size()) return;
    progress = qBound(0.0, progress, 1.0);
    if (qAbs(m_tracks[row].progress - progress) < 0.002 && qAbs(m_tracks[row].positionMs - positionMs) < 250) {
        return;
    }
    m_tracks[row].progress = progress;
    m_tracks[row].positionMs = positionMs;

    int vRow = m_visibleRows.indexOf(row);
    if (vRow >= 0) {
        emit dataChanged(index(vRow), index(vRow), { ProgressRole, PositionMsRole });
    }
}

quint64 PlaylistModel::append(const QString& path,
                              const QString& name,
                              const QString& title,
                              const QString& artist,
                              const QString& duration,
                              const QImage& cover,
                              bool hasCover,
                              double progress,
                              qint64 positionMs) {
    const int origIdx = count();
    const quint64 trackId = m_nextTrackId++;
    QColor accent = (hasCover && !cover.isNull()) ? extractProgressAccentColor(cover) : QColor(100, 140, 210);
    Track track{ trackId, path, name, title, artist, duration, 0, cover, hasCover, 1, progress, positionMs, accent };
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
                m_tracks[i].accentColor = extractProgressAccentColor(cover);
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
                                     { TitleRole, ArtistRole, NameRole, HasCoverRole, CoverUrlRole, AccentColorRole });
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
