#pragma once

#include <qaudiooutput.h>
#include <qmediaplayer.h>
#include <string>
namespace Kalorite {
    class Mixer {
        public:
        Mixer();

        void setCurrent(const std::string& trackPath);
        void setVolume(int volume);
        void setPosition(int position);

        void play();
        void pause();
        void stop();

        QMediaPlayer* corePlayer;
        QAudioOutput* outputPipe;


        std::string currentTrack;
        int volume;
    };
} // namespace Kalorite
