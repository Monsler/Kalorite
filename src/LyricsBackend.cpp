#include "LyricsBackend.hpp"
#include "PlaybackController.hpp"
#include "Mixer.hpp"
#include "Playlist.hpp"
#include "PlaylistModel.hpp"
#include "TrackMetadataReader.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>

namespace Kalorite {

static const QString kLyricsUserAgent = QStringLiteral("Kalorite/0.1 ( https://github.com/Monsler/Kalorite )");

LyricsBackend::LyricsBackend(PlaybackController* playback, Mixer* mixer, Playlist* playlist, QObject* parent)
    : QAbstractListModel(parent),
      m_playback(playback),
      m_mixer(mixer),
      m_playlist(playlist),
      m_net(new QNetworkAccessManager(this)) {
    if (m_playlist && m_playlist->model()) {
        connect(m_playlist->model(), &QAbstractItemModel::dataChanged, this, [this]() {
            updateCoverColor();
        });
    }
}

LyricsBackend::~LyricsBackend() {
}

int LyricsBackend::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_lines.size();
}

QVariant LyricsBackend::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size()) {
        return QVariant();
    }

    const auto& line = m_lines.at(index.row());
    switch (role) {
    case TextRole:
    case Qt::DisplayRole:
        return line.text;
    case TimeMsRole:
        return line.timeMs;
    case TimeStrRole:
        return line.timeStr;
    case IsActiveRole:
        return (index.row() == m_activeIndex);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> LyricsBackend::roleNames() const {
    return {
        { TextRole, "text" },
        { TimeMsRole, "timeMs" },
        { TimeStrRole, "timeStr" },
        { IsActiveRole, "isActive" }
    };
}

void LyricsBackend::seekToLine(int index) {
    if (index < 0 || index >= m_lines.size()) return;
    qint64 targetMs = m_lines[index].timeMs;
    if (targetMs >= 0 && m_playback) {
        m_playback->seekMs(static_cast<int>(targetMs));
        int prev = m_activeIndex;
        m_activeIndex = index;
        emit activeIndexChanged(m_activeIndex);
        if (prev >= 0 && prev < m_lines.size()) {
            emit dataChanged(this->index(prev), this->index(prev), { IsActiveRole });
        }
        emit dataChanged(this->index(m_activeIndex), this->index(m_activeIndex), { IsActiveRole });
    }
}

void LyricsBackend::reload() {
    if (!m_currentPath.isEmpty()) {
        loadForTrack(m_currentPath);
    }
}

void LyricsBackend::openLrcFile() {
    QString startDir = m_currentPath.isEmpty() ? QDir::homePath() : QFileInfo(m_currentPath).dir().path();
    QString path = QFileDialog::getOpenFileName(nullptr, tr("Open LRC file"), startDir, tr("LRC files (*.lrc);;All files (*.*)"));
    if (!path.isEmpty()) {
        loadLrcFromPath(path);
    }
}

void LyricsBackend::loadLrcFromPath(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_statusText = tr("Failed to open file: %1").arg(filePath);
        emit stateChanged();
        return;
    }
    QString content = QString::fromUtf8(file.readAll());
    parseLrcContent(content);
}

void LyricsBackend::onTrackLoaded(const QString& path) {
    if (path.isEmpty()) return;
    loadForTrack(path);
}

void LyricsBackend::onPlaybackPositionChanged(int posMs) {
    if (!m_isSynced || m_lines.isEmpty()) return;

    int newActive = -1;
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines[i].timeMs <= posMs) {
            newActive = i;
        } else {
            break;
        }
    }

    if (newActive != m_activeIndex) {
        int prev = m_activeIndex;
        m_activeIndex = newActive;
        emit activeIndexChanged(m_activeIndex);
        if (prev >= 0 && prev < m_lines.size()) {
            emit dataChanged(index(prev), index(prev), { IsActiveRole });
        }
        if (m_activeIndex >= 0 && m_activeIndex < m_lines.size()) {
            emit dataChanged(index(m_activeIndex), index(m_activeIndex), { IsActiveRole });
        }
    }
}

static void cleanSearchTerms(const QString& filePath, const QString& inTitle, const QString& inArtist,
                             QString& outTitle, QString& outArtist) {
    outTitle = inTitle;
    outArtist = inArtist;

    QFileInfo fi(filePath);
    QString baseName = fi.completeBaseName();

    if (outTitle.isEmpty() || outTitle == baseName) {
        baseName.remove(QRegularExpression(QStringLiteral("^[0-9]+[\\s\\.\\-_]+")));
        baseName.remove(QRegularExpression(
            QStringLiteral("\\s*[\\[\\(].*?(official|video|audio|lyrics|remaster|hd|hq|live|remix).*?[\\]\\)]"),
            QRegularExpression::CaseInsensitiveOption));

        int dash = baseName.indexOf(QStringLiteral(" - "));
        if (dash > 0) {
            if (outArtist.isEmpty()) {
                outArtist = baseName.left(dash).trimmed();
            }
            outTitle = baseName.mid(dash + 3).trimmed();
        } else {
            outTitle = baseName.trimmed();
        }
    }

    outTitle = outTitle.replace('"', ' ').trimmed();
    outArtist = outArtist.replace('"', ' ').trimmed();
}

static QColor extractDominantColor(const QImage& img) {
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

void LyricsBackend::updateCoverColor() {
    QImage cover;
    if (m_playlist && m_playlist->currentRow() >= 0) {
        int row = m_playlist->currentRow();
        cover = m_playlist->model()->coverAt(row);
    }

    if (cover.isNull() && !m_currentPath.isEmpty()) {
        TrackMetadata tags = TrackMetadataReader::read(m_currentPath);
        cover = tags.cover;
    }

    QColor newColor = extractDominantColor(cover);
    if (newColor != m_backgroundColor) {
        m_backgroundColor = newColor;
        emit backgroundColorChanged();
    }
}

void LyricsBackend::loadForTrack(const QString& path) {
    m_currentPath = path;
    m_activeIndex = -1;

    updateCoverColor();

    beginResetModel();
    m_lines.clear();
    endResetModel();

    m_hasLyrics = false;
    m_isSynced = false;
    m_isLoading = true;
    m_statusText = tr("Searching for lyrics...");
    emit stateChanged();

    // Determine title & artist
    QString rawTitle, rawArtist;
    if (m_playlist && m_playlist->currentRow() >= 0) {
        rawTitle = m_playlist->model()->titleAt(m_playlist->currentRow());
        rawArtist = m_playlist->model()->artistAt(m_playlist->currentRow());
    }

    cleanSearchTerms(path, rawTitle, rawArtist, m_trackTitle, m_trackArtist);
    emit trackInfoChanged();

    // 1. Try local .lrc file first (exact filename with .lrc extension)
    if (tryLoadLocalLrc(path)) {
        return;
    }

    // 2. Query LRCLIB API
    int durationSec = m_mixer ? (m_mixer->getDurationMs() / 1000) : 0;
    queryLrclib(path, m_trackTitle, m_trackArtist, durationSec);
}

bool LyricsBackend::tryLoadLocalLrc(const QString& path) {
    QFileInfo fi(path);
    QString lrcPath = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".lrc"));

    if (QFile::exists(lrcPath)) {
        QFile file(lrcPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(file.readAll());
            qDebug() << "[Lyrics] Found local .lrc file:" << lrcPath;
            parseLrcContent(content);
            return true;
        }
    }
    return false;
}

void LyricsBackend::queryLrclib(const QString& path, const QString& title, const QString& artist, int durationSec) {
    if (title.isEmpty() && artist.isEmpty()) {
        m_isLoading = false;
        m_statusText = tr("No track details to search lyrics");
        emit stateChanged();
        return;
    }

    QUrl url(QStringLiteral("https://lrclib.net/api/get"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("track_name"), title);
    if (!artist.isEmpty()) {
        q.addQueryItem(QStringLiteral("artist_name"), artist);
    }
    if (durationSec > 0) {
        q.addQueryItem(QStringLiteral("duration"), QString::number(durationSec));
    }
    url.setQuery(q);

    qDebug() << "[Lyrics] Querying LRCLIB get:" << url.toString();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, kLyricsUserAgent);
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(5);
    request.setTransferTimeout(5000);

    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, path, title, artist, durationSec]() {
        handleLrclibGetReply(reply, path, title, artist, durationSec);
    });
}

void LyricsBackend::handleLrclibGetReply(QNetworkReply* reply, const QString& path, const QString& title, const QString& artist, int durationSec) {
    reply->deleteLater();

    if (m_currentPath != path) {
        // Track changed in the meantime
        return;
    }

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject root = doc.object();
            QString syncedLyrics = root[QStringLiteral("syncedLyrics")].toString().trimmed();
            QString plainLyrics = root[QStringLiteral("plainLyrics")].toString().trimmed();
            bool instrumental = root[QStringLiteral("instrumental")].toBool();

            QFileInfo fi(path);
            QString lrcPath = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".lrc"));

            if (!syncedLyrics.isEmpty()) {
                qDebug() << "[Lyrics] LRCLIB returned synced lyrics!";
                parseLrcContent(syncedLyrics, true, lrcPath);
                return;
            } else if (!plainLyrics.isEmpty()) {
                qDebug() << "[Lyrics] LRCLIB returned plain lyrics!";
                parseLrcContent(plainLyrics, true, lrcPath);
                return;
            } else if (instrumental) {
                QList<LyricsLine> lines;
                lines.append({ -1, tr("♪ Instrumental ♪"), QString() });
                setLyricsLines(lines, false);
                m_statusText = tr("Instrumental");
                emit stateChanged();
                return;
            }
        }
    }

    // Fallback: search endpoint
    QString query = !artist.isEmpty() ? (artist + QStringLiteral(" ") + title) : title;
    qDebug() << "[Lyrics] LRCLIB get failed/empty. Falling back to search:" << query;
    queryLrclibSearch(path, query);
}

void LyricsBackend::queryLrclibSearch(const QString& path, const QString& query) {
    QUrl url(QStringLiteral("https://lrclib.net/api/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), query);
    url.setQuery(q);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, kLyricsUserAgent);
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(5);
    request.setTransferTimeout(5000);

    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, path]() {
        handleLrclibSearchReply(reply, path);
    });
}

void LyricsBackend::handleLrclibSearchReply(QNetworkReply* reply, const QString& path) {
    reply->deleteLater();

    if (m_currentPath != path) {
        return;
    }

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            QJsonArray results = doc.array();
            QJsonObject bestMatch;

            // Prioritize results with syncedLyrics
            for (const QJsonValue& val : results) {
                QJsonObject item = val.toObject();
                if (!item[QStringLiteral("syncedLyrics")].toString().trimmed().isEmpty()) {
                    bestMatch = item;
                    break;
                }
            }

            if (bestMatch.isEmpty() && !results.isEmpty()) {
                bestMatch = results.first().toObject();
            }

            if (!bestMatch.isEmpty()) {
                QString synced = bestMatch[QStringLiteral("syncedLyrics")].toString().trimmed();
                QString plain = bestMatch[QStringLiteral("plainLyrics")].toString().trimmed();

                QFileInfo fi(path);
                QString lrcPath = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".lrc"));

                if (!synced.isEmpty()) {
                    parseLrcContent(synced, true, lrcPath);
                    return;
                } else if (!plain.isEmpty()) {
                    parseLrcContent(plain, true, lrcPath);
                    return;
                }
            }
        }
    }

    // If nothing found or error
    m_isLoading = false;
    m_hasLyrics = false;
    m_statusText = tr("No lyrics found");
    emit stateChanged();
}

void LyricsBackend::parseLrcContent(const QString& content, bool saveToLocal, const QString& targetLrcPath) {
    if (content.trimmed().isEmpty()) {
        m_isLoading = false;
        m_hasLyrics = false;
        m_statusText = tr("Empty lyrics");
        emit stateChanged();
        return;
    }

    // Save to local file if requested and possible
    if (saveToLocal && !targetLrcPath.isEmpty()) {
        QFile outFile(targetLrcPath);
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            outFile.write(content.toUtf8());
            outFile.close();
            qDebug() << "[Lyrics] Cached lyrics to local file:" << targetLrcPath;
        }
    }

    static const QRegularExpression timeTagRx(QStringLiteral("\\[(\\d{1,2}):(\\d{2})(?:[\\.:](\\d{1,3}))?\\]"));
    QStringList rawLines = content.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);

    QList<LyricsLine> parsedLines;
    bool hasTimeTags = false;

    for (const QString& line : rawLines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        // Skip metadata tags like [ar:Artist], [ti:Title], [al:Album], [length:...], [by:...]
        if (trimmed.startsWith('[') && !trimmed.isEmpty()) {
            QChar secondChar = trimmed.length() > 1 ? trimmed.at(1) : QChar();
            if (!secondChar.isDigit()) {
                continue;
            }
        }

        // Match all timestamp tags in the line
        auto it = timeTagRx.globalMatch(trimmed);
        QList<qint64> timestamps;
        int lastPos = 0;

        while (it.hasNext()) {
            auto match = it.next();
            hasTimeTags = true;
            int min = match.captured(1).toInt();
            int sec = match.captured(2).toInt();
            QString fracStr = match.captured(3);
            int ms = 0;
            if (fracStr.length() == 1) ms = fracStr.toInt() * 100;
            else if (fracStr.length() == 2) ms = fracStr.toInt() * 10;
            else if (fracStr.length() >= 3) ms = fracStr.left(3).toInt();

            qint64 timeMs = static_cast<qint64>(min) * 60000 + sec * 1000 + ms;
            timestamps.append(timeMs);
            lastPos = match.capturedEnd();
        }

        QString text = trimmed.mid(lastPos).trimmed();

        if (!timestamps.isEmpty()) {
            for (qint64 tMs : timestamps) {
                int totalSec = static_cast<int>(tMs / 1000);
                QString timeStr = QString::asprintf("%02d:%02d", totalSec / 60, totalSec % 60);
                parsedLines.append({ tMs, text, timeStr });
            }
        } else {
            // Line without timestamps
            parsedLines.append({ -1, trimmed, QString() });
        }
    }

    if (hasTimeTags) {
        // Sort lines chronologically
        std::sort(parsedLines.begin(), parsedLines.end(), [](const LyricsLine& a, const LyricsLine& b) {
            return a.timeMs < b.timeMs;
        });
    }

    setLyricsLines(parsedLines, hasTimeTags);
}

void LyricsBackend::setLyricsLines(const QList<LyricsLine>& lines, bool synced) {
    beginResetModel();
    m_lines = lines;
    endResetModel();

    m_hasLyrics = !m_lines.isEmpty();
    m_isSynced = synced;
    m_isLoading = false;
    m_statusText.clear();
    m_activeIndex = -1;

    emit stateChanged();

    // Trigger position check immediately if playback is ongoing
    if (m_mixer) {
        onPlaybackPositionChanged(m_mixer->getPositionMs());
    }
}

} // namespace Kalorite
