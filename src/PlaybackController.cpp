#include "PlaybackController.hpp"
#include "Mixer.hpp"
#include "Playlist.hpp"
#include <QDebug>
#include <QTimer>
#include <algorithm>
#include <random>

namespace Kalorite {

PlaybackController::PlaybackController(Mixer* mixer, Playlist* playlist, QObject* parent)
    : QObject(parent), m_mixer(mixer), m_playlist(playlist), m_timer(new QTimer(this)) {
    connect(m_timer, &QTimer::timeout, this, &PlaybackController::onTimer);
    connect(m_mixer, &Mixer::playbackFinished, this, &PlaybackController::onCrossfadeAdvance);
    connect(m_playlist, &Playlist::rowsReordered, this, &PlaybackController::onRowsReordered);
    connect(m_playlist, &Playlist::contentsChanged, this, [this]() { genShuffle(); });
}

void PlaybackController::play() {
    m_isPlaying = true;
    m_mixer->play();
    m_timer->start(50);
    emit playStateChanged(true);
    emit pluginStateChanged("playing");
}

void PlaybackController::pause() {
    m_isPlaying = false;
    m_mixer->pause();
    m_timer->stop();
    emit playStateChanged(false);
    emit pluginStateChanged("paused");
}

void PlaybackController::stop() {
    pause();
    m_mixer->setPosition(0);
}

void PlaybackController::togglePlay() {
    if (m_isPlaying) {
        pause();
    } else {
        play();
    }
}

void PlaybackController::seekMs(int ms) {
    m_mixer->setPosition(ms);
}

void PlaybackController::selectTrack(int id) {
    if (id < 0 || id >= m_playlist->count()) return;

    std::string requested = m_playlist->pathAt(id).toStdString();
    bool sameTrack = (requested == m_currentAudio);

    m_playlist->setCurrentRow(id);
    m_currentId = id;
    // Starting a queued track directly consumes its queue entry.
    m_playlist->clearQueueEntry(id);

    if (m_crossfadeEnabled || m_mixer->isGaplessEnabled()) {
        // Seamless transition: do not stop/start the mixer, so the miniaudio sound
        // slots are not torn down under the outgoing track.
        if (sameTrack && m_isPlaying) {
            // Re-selecting the playing track restarts it in place; loading it again
            // would overlap a second copy of the same track.
            m_mixer->setPosition(0);
            m_isPlaying = true;
            emit playStateChanged(true);
            m_timer->start(50);
        } else if (m_isPlaying) {
            // setCurrent overlaps the incoming sound with the outgoing one; Bit Perfect
            // loads paused, so play() is needed there (a no-op for the engine path).
            loadTrack(requested);
            m_mixer->play();
            emit playStateChanged(true);
            m_timer->start(50);
        } else {
            loadTrack(requested);
            play();
        }
    } else {
        pause();
        loadTrack(requested);
        play();
    }
}

void PlaybackController::skipNext() {
    if (m_playlist->count() == 0) return;

    int queued = takeNextQueuedTrack();
    if (queued >= 0) {
        selectTrack(queued);
        return;
    }

    if (m_loopType == 3) {
        int next = getNextShuffleTrack();
        if (next >= 0) selectTrack(next);
        return;
    }

    if (m_loopType == 2) {
        int next = m_playlist->currentRow() + 1;
        if (next >= m_playlist->count()) next = 0;
        selectTrack(next);
        return;
    }

    int next = m_playlist->currentRow() + 1;
    if (next < m_playlist->count()) {
        selectTrack(next);
    }
}

void PlaybackController::skipPrevious() {
    if (m_playlist->count() == 0) return;

    if (m_loopType == 3) {
        if (m_shufflePos > 1 && !m_shuffle.empty()) {
            m_shufflePos -= 2;
            selectTrack(m_shuffle[m_shufflePos++]);
            return;
        } else if (!m_shuffle.empty()) {
            selectTrack(m_shuffle[0]);
            return;
        }
    }

    if (m_loopType == 2) {
        int prev = m_playlist->currentRow() - 1;
        if (prev < 0) prev = m_playlist->count() - 1;
        selectTrack(prev);
        return;
    }

    int prev = m_playlist->currentRow() - 1;
    if (prev >= 0) {
        selectTrack(prev);
    }
}

void PlaybackController::cycleLoopType() {
    m_loopType++;
    if (m_loopType > 3) {
        m_loopType = 0;
    }

    if (m_loopType == 3) {
        genShuffle();
    }
    emit loopTypeChanged(m_loopType);
}

void PlaybackController::toggleQueue(int row) {
    m_playlist->toggleQueue(row);
}

void PlaybackController::addTrack(const QString& path, const QString& displayName, qint64 durationMs) {
    if (!m_playlist->addTrack(path, displayName, durationMs)) return;

    if (m_currentAudio.empty()) {
        loadTrack(path.toStdString());
        m_currentId = 0;
        m_playlist->setCurrentRow(0);
    }
    emit controlsAvailable();
}

void PlaybackController::loadTrack(const std::string& path) {
    m_currentAudio = path;
    m_mixer->setCurrent(path);
    emit trackLoaded(QString::fromStdString(path));
}

void PlaybackController::playDetached(const QString& path) {
    m_mixer->setCurrent(path.toStdString());
    m_isPlaying = true;
    m_mixer->play();
    m_timer->start(50);
    emit playStateChanged(true);
}

void PlaybackController::onTimer() {
    emit tick();

    m_mixer->setCrossfadeEnabled(m_crossfadeEnabled);
    m_mixer->setSmartGainEnabled(m_smartGainEnabled);
    m_mixer->setBitPerfectEnabled(m_bitPerfectEnabled);

    // A freshly loaded track can briefly report duration 0; require a real duration so
    // it is not treated as finished at once.
    bool finished = m_isPlaying && !m_mixer->isPlaying() && m_mixer->getDurationMs() > 100
        && m_mixer->getPositionMs() >= (m_mixer->getDurationMs() - 100);
    if (!finished) return;

    emit trackFinished(QString::fromStdString(m_currentAudio));

    // The playlist may have been emptied while the track was playing.
    if (m_playlist->count() == 0) {
        pause();
        return;
    }

    // Priority queue overrides the regular order in all loop modes
    int queued = takeNextQueuedTrack();
    if (queued >= 0) {
        selectTrack(queued);
        return;
    }

    if (m_loopType == 0) {
        pause();
        m_mixer->setPosition(0);
    } else if (m_loopType == 1) {
        m_mixer->setPosition(0);
        play();
    } else if (m_loopType == 2) {
        int next = (m_currentId + 1 < m_playlist->count()) ? m_currentId + 1 : 0;
        selectTrack(next);
    } else if (m_loopType == 3) {
        int next = getNextShuffleTrack();
        if (next >= 0) {
            selectTrack(next);
        }
    }
}

// Fired by the mixer shortly before the current track ends (crossfade/gapless modes).
void PlaybackController::onCrossfadeAdvance() {
    if (!m_isPlaying || m_playlist->count() == 0) return;

    int nextId = -1;
    int queued = takeNextQueuedTrack();
    if (queued >= 0) {
        nextId = queued;
    } else {
        switch (m_loopType) {
            case 0:
                break;
            case 1:
                nextId = m_currentId;
                break;
            case 2:
                nextId = ((m_currentId + 1 < m_playlist->count()) ? m_currentId + 1 : 0);
                break;
            case 3:
                nextId = getNextShuffleTrack();
                break;
        }
    }

    if (nextId < 0) return;

    m_currentId = nextId;
    m_playlist->setCurrentRow(nextId);
    loadTrack(m_playlist->pathAt(nextId).toStdString());
    m_mixer->play();
    m_isPlaying = true;
    emit playStateChanged(true);
    if (!m_timer->isActive()) m_timer->start(50);
}

void PlaybackController::onRowsReordered() {
    if (m_isPlaying) {
        int row = m_playlist->rowForPath(QString::fromStdString(m_currentAudio));
        if (row >= 0) {
            m_currentId = row;
            m_playlist->setCurrentRow(row);
        }
    } else {
        int row = m_playlist->currentRow();
        if (row >= 0 && row < m_playlist->count()) {
            m_currentId = row;
            loadTrack(m_playlist->pathAt(row).toStdString());
        }
    }
}

// Pops the next queued track.
int PlaybackController::takeNextQueuedTrack() {
    return m_playlist->takeQueueHead();
}

void PlaybackController::genShuffle() {
    int size = m_playlist->count();
    m_shufflePos = 0;

    m_shuffle.resize(size);
    for (int i = 0; i < size; i++) {
        m_shuffle[i] = i;
    }

    if (size <= 1) return;

    std::random_device rd;
    std::mt19937_64 g(rd());
    std::shuffle(m_shuffle.begin(), m_shuffle.end(), g);

    int currentPlayingRow = m_playlist->rowForPath(QString::fromStdString(m_currentAudio));
    if (currentPlayingRow < 0) {
        currentPlayingRow = m_currentId;
    }

    if (currentPlayingRow >= 0 && currentPlayingRow < size && m_shuffle[0] == currentPlayingRow) {
        std::uniform_int_distribution<size_t> dist(1, size - 1);
        size_t other = dist(g);
        std::swap(m_shuffle[0], m_shuffle[other]);
    }
}

int PlaybackController::getNextShuffleTrack() {
    int size = m_playlist->count();
    if (size == 0) return -1;
    if (size == 1) return 0;

    if (m_shuffle.empty() || m_shufflePos >= (int)m_shuffle.size()) {
        genShuffle();
    }

    int currentPlayingRow = m_playlist->rowForPath(QString::fromStdString(m_currentAudio));
    if (currentPlayingRow < 0) {
        currentPlayingRow = m_currentId;
    }

    if (currentPlayingRow >= 0 && currentPlayingRow < size && m_shufflePos < (int)m_shuffle.size() && m_shuffle[m_shufflePos] == currentPlayingRow) {
        if (m_shufflePos + 1 < (int)m_shuffle.size()) {
            std::swap(m_shuffle[m_shufflePos], m_shuffle[m_shufflePos + 1]);
        } else {
            genShuffle();
        }
    }

    int next = m_shuffle[m_shufflePos];
    m_shufflePos++;
    return next;
}

} // namespace Kalorite
