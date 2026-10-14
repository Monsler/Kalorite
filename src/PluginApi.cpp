#include "PluginApi.hpp"
#include "PlaybackController.hpp"
#include "Playlist.hpp"
#include "Mixer.hpp"
#include <QFileInfo>

namespace Kalorite {

PluginApi::PluginApi(PlaybackController* playback, Playlist* playlist, Mixer* mixer,
                     QObject* parent)
    : QObject(parent), m_playback(playback), m_playlist(playlist), m_mixer(mixer) {
    connect(m_playback, &PlaybackController::trackLoaded, this, [this](const QString& path) {
        emit pluginTrackChanged(path, m_playlist->currentRow());
    });
    connect(m_playback, &PlaybackController::trackFinished, this, &PluginApi::pluginTrackFinished);
    connect(m_playback, &PlaybackController::pluginStateChanged, this, &PluginApi::pluginPlaybackStateChanged);
}

void PluginApi::play() { m_playback->play(); }
void PluginApi::pause() { m_playback->pause(); }
void PluginApi::stop() { m_playback->stop(); }
void PluginApi::next() { emit nextRequested(); }
void PluginApi::prev() { emit prevRequested(); }

bool PluginApi::isPlaying() const { return m_playback->isPlaying(); }
int PluginApi::positionMs() const { return m_mixer->getPositionMs(); }
int PluginApi::durationMs() const { return m_mixer->getDurationMs(); }
void PluginApi::seekMs(int ms) { m_playback->seekMs(ms); }

int PluginApi::playlistCount() const { return m_playlist->count(); }

void PluginApi::playlistAdd(const QString& path) { m_playback->addTrack(path); }

void PluginApi::playlistRemove(int index) {
    if (index >= 0 && index < m_playlist->count()) {
        m_playlist->removeRow(index);
    }
}

void PluginApi::playlistClear() { m_playlist->clear(); }

QString PluginApi::playlistPath(int index) const { return m_playlist->pathAt(index); }

QString PluginApi::playlistTitle(int index) const {
    return QFileInfo(playlistPath(index)).fileName();
}

int PluginApi::currentIndex() const { return m_playlist->currentRow(); }

void PluginApi::playIndex(int index) { m_playback->selectTrack(index); }

void PluginApi::notify(const QString& text) { emit statusMessage(text); }

} // namespace Kalorite
