#include "Playlist.hpp"
#include "PlaylistModel.hpp"
#include "TrackMetadataReader.hpp"
#include "MusicBrainzService.hpp"
#include "miniaudio.h"
#include <QFileInfo>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Kalorite {

Playlist::Playlist(QObject* parent)
    : QObject(parent),
      m_model(new PlaylistModel(this)),
      m_musicBrainz(new MusicBrainzService(this)) {
    connect(m_model, &QAbstractItemModel::rowsMoved, this, &Playlist::onRowsMoved);
    connect(m_musicBrainz, &MusicBrainzService::metadataFetched, m_model, &PlaylistModel::updateMetadataById);
}

int Playlist::count() const { return m_model->count(); }

int Playlist::currentRow() const { return m_model->currentRow(); }

void Playlist::setCurrentRow(int row) { m_model->setCurrentRow(row); }

QString Playlist::pathAt(int row) const { return m_model->pathAt(row); }

QString Playlist::titleAt(int row) const { return m_model->titleAt(row); }

QString Playlist::artistAt(int row) const { return m_model->artistAt(row); }

bool Playlist::hasCoverAt(int row) const { return m_model->hasCoverAt(row); }

int Playlist::rowForPath(const QString& path) const {
    for (int i = 0; i < count(); ++i) {
        if (m_model->pathAt(i) == path) return i;
    }
    return -1;
}

bool Playlist::addTrack(const QString& path, const QString& displayName, qint64 durationMs) {
    if (path.isEmpty()) return false;
    if (rowForPath(path) >= 0) return false;

    TrackMetadata meta = TrackMetadataReader::read(path);

    QString title = meta.title;
    QString artist = meta.artist;

    if (!displayName.isEmpty()) {
        title = displayName;
    }

    if (title.isEmpty()) {
        title = QFileInfo(path).fileName();
    }

    QString name = !artist.isEmpty() ? (artist + QStringLiteral(" - ") + title) : title;

    if (durationMs < 0) {
        durationMs = getAudioDurationMs(path);
    }

    QString durStr = formatDuration(durationMs);

    quint64 trackId = m_model->append(path, name, title, artist, durStr, meta.cover, meta.hasCover);
    emit contentsChanged();

    bool needCover = !meta.hasCover;
    bool needTags = artist.isEmpty() || title.isEmpty() || title == QFileInfo(path).completeBaseName();
    if (needCover || needTags) {
        m_musicBrainz->enqueue(trackId, title, artist, path, needCover, needTags);
    }

    return true;
}

void Playlist::removeRow(int row) {
    if (row < 0 || row >= count()) return;
    m_model->removeAt(row);
    emit contentsChanged();
}

void Playlist::clear() {
    m_model->clear();
    emit contentsChanged();
}

QString Playlist::formatDuration(qint64 durationMs) {
    if (durationMs <= 0) return QStringLiteral("--:--");
    int totalSeconds = static_cast<int>(durationMs / 1000);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

qint64 Playlist::getAudioDurationMs(const QString& path) {
    if (path.isEmpty()) return -1;

    ma_decoder decoder;
    ma_result res;
#ifdef Q_OS_WIN
    res = ma_decoder_init_file_w(reinterpret_cast<const wchar_t*>(path.utf16()), nullptr, &decoder);
#else
    res = ma_decoder_init_file(path.toLocal8Bit().constData(), nullptr, &decoder);
#endif
    if (res != MA_SUCCESS) {
        return -1;
    }

    ma_uint64 totalFrames = 0;
    ma_result lenRes = ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
    qint64 durationMs = -1;
    if (lenRes == MA_SUCCESS && decoder.outputSampleRate > 0) {
        durationMs = static_cast<qint64>((totalFrames * 1000) / decoder.outputSampleRate);
    }
    ma_decoder_uninit(&decoder);
    return durationMs;
}

void Playlist::setRowDuration(int row, qint64 durationMs) {
    if (durationMs <= 0) return;
    m_model->setDuration(row, formatDuration(durationMs));
}

// Enqueuing appends the next number ([1], [2], ...); toggling an already-queued row
// removes it and renumbers the rest.
void Playlist::toggleQueue(int row) {
    if (row < 0 || row >= count()) return;

    int q = m_model->queueAt(row);
    if (q > 0) {
        m_model->setQueue(row, 0);
        closeQueueGap(q);
    } else {
        int maxq = 0;
        for (int i = 0; i < count(); ++i)
            maxq = std::max(maxq, m_model->queueAt(i));
        m_model->setQueue(row, maxq + 1);
    }
}

// Drops the row from the queue without playing it, renumbering the rest.
void Playlist::clearQueueEntry(int row) {
    if (row < 0 || row >= count()) return;
    int q = m_model->queueAt(row);
    if (q <= 0) return;

    m_model->setQueue(row, 0);
    closeQueueGap(q);
}

void Playlist::clearAllQueues() {
    for (int i = 0; i < count(); ++i)
        m_model->setQueue(i, 0);
}

// Pops the lowest positive queue number and returns its row, or -1 if the queue is empty.
int Playlist::takeQueueHead() {
    int headRow = -1, headQ = 0;
    for (int i = 0; i < count(); ++i) {
        int qi = m_model->queueAt(i);
        if (qi > 0 && (headRow < 0 || qi < headQ)) { headRow = i; headQ = qi; }
    }
    if (headRow < 0) return -1;

    m_model->setQueue(headRow, 0);
    closeQueueGap(headQ);
    return headRow;
}

// Every queue number above a removed slot moves down by one.
void Playlist::closeQueueGap(int slot) {
    for (int i = 0; i < count(); ++i) {
        int qi = m_model->queueAt(i);
        if (qi > slot) m_model->setQueue(i, qi - 1);
    }
}

void Playlist::saveToFile(const QString& filePath) const {
    nlohmann::json playlist = nlohmann::json::array();
    for (int i = 0; i < count(); i++) {
        playlist.push_back(m_model->pathAt(i).toStdString());
    }

    std::ofstream file(filePath.toStdString());
    file << playlist;

}

QStringList Playlist::readPathsFromFile(const QString& filePath) {
    std::ifstream file(filePath.toStdString());
    nlohmann::json playlist;
    file >> playlist;

    QStringList paths;
    for (size_t i = 0; i < playlist.size(); i++) {
        paths << QString::fromStdString(playlist[i]);
    }
    return paths;
}

void Playlist::onRowsMoved() {
    emit rowsReordered();
    emit contentsChanged();
}

} // namespace Kalorite
