#include "Mixer.hpp"

namespace Kalorite {
    Mixer::Mixer() {
        corePlayer = new QMediaPlayer();
        outputPipe = new QAudioOutput();

        corePlayer->setAudioOutput(outputPipe);
        outputPipe->setVolume(1.0f);

        currentTrack = "";
    }

    void Mixer::setCurrent(const std::string& trackPath) {
        currentTrack = trackPath;
        corePlayer->setSource(QUrl::fromLocalFile(QString::fromStdString(trackPath)));
    }

    void Mixer::setPosition(int position) {
        corePlayer->setPosition(position);
    }

    void Mixer::setVolume(int volume) {
        this->volume = volume;
        this->outputPipe->setVolume(float(volume) / 100);
    }

    void Mixer::play() {
        corePlayer->play();
    }

    void Mixer::pause() {
        corePlayer->pause();
    }

    void Mixer::stop() {
        corePlayer->stop();
    }
}
