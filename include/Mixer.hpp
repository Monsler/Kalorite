#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <QObject>
#include "miniaudio.h"

namespace Kalorite {
    class Mixer : public QObject {
        Q_OBJECT

        public:
        Mixer();
        ~Mixer();

        void setCurrent(const std::string& trackPath);
        void setVolume(int volume);
        void setPosition(int positionMs);

        void play();
        void pause();
        void stop();

        // Helper getters for UI compatibility
        int getPositionMs() const;
        int getDurationMs() const;
        bool isPlaying() const;
        std::string getCurrentDeviceName();
        void setDeviceByName(const std::string& deviceName);

        // Properties needed for external polling / UI updates
        std::string currentTrack;
        int volume = 100;

        // Custom features called from MainWindow
        void setCrossfadeEnabled(bool enabled) { m_crossfadeEnabled = enabled; }
        void setCrossfadeDuration(float seconds) { if (seconds > 0.1f) m_crossfadeDurationSec = seconds; }

        // Smart Gain: peak limiting to avoid clipping (an effect applied to the mix).
        void setSmartGainEnabled(bool enabled) { m_smartGainEnabled = enabled; }
        bool isSmartGainEnabled() const { return m_smartGainEnabled; }

        // Bit Perfect: stream the decoded audio straight to the sound card,
        // bypassing the engine, EQ, resampling, volume and every other stage.
        void setBitPerfectEnabled(bool enabled);
        bool isBitPerfectEnabled() const { return m_bitPerfectEnabled; }

        void setGaplessEnabled(bool enabled) { m_gaplessEnabled = enabled; }
        void setDoubleBufferingEnabled(bool enabled);

        bool isGaplessEnabled() const { return m_gaplessEnabled; }
        bool isDoubleBufferingEnabled() const { return m_doubleBufferingEnabled; }

        void setEqBand(int bandIndex, float gainDb);
        void setEqEnabled(bool enabled);
        std::vector<float> getLatestSamples() const;

        // Native format of the currently loaded audio source (not the output device).
        // Returns 0 when nothing is loaded.
        int getSourceChannels() const;
        int getSourceSampleRate() const;

    signals:
        void playbackFinished();

    private:
        static void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
        static void bitPerfectDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

        // Bit-perfect helpers
        bool startBitPerfect(const std::string& trackPath, bool autostart);
        void stopBitPerfect();

        // Snap any in-progress crossfade to completion, leaving only the active
        // sound playing at full volume. Keeps the two-slot juggling deterministic.
        void finalizeCrossfade();

        ma_context m_context;
        ma_device m_device;
        bool m_deviceInitialized = false;

        ma_engine m_engine;
        bool m_engineInitialized = false;

        ma_sound m_sound1;
        ma_sound m_sound2;
        bool m_sound1Initialized = false;
        bool m_sound2Initialized = false;

        int m_activeSoundIndex = 0; // 0 = none, 1 = sound1, 2 = sound2
        std::string m_chosenDeviceName;

        bool m_crossfadeEnabled = false;
        bool m_bitPerfectEnabled = false;
        bool m_smartGainEnabled = false;
        bool m_crossfadeTriggered = false;
        bool m_gaplessEnabled = false;
        bool m_doubleBufferingEnabled = false;

        // Crossfade state
        float m_crossfadeDurationSec = 3.0f;
        bool m_crossfadeActive = false; // both sounds playing, ramping volumes

        // Bit-perfect state: dedicated decoder + device matching the source natively
        ma_decoder m_bpDecoder;
        ma_device m_bpDevice;
        bool m_bpDecoderInitialized = false;
        bool m_bpDeviceInitialized = false;
        bool m_bpPlaying = false;
        std::mutex m_bpMutex;
        std::string m_bpTrackPath;

        // Equalizer member variables
        mutable std::mutex m_eqMutex;
        bool m_eqEnabled = true; // Enabled by default to match UI state
        float m_eqGains[10];
        ma_peak2 m_eqFilters[10];
        bool m_eqFiltersInitialized[10];

        // Sample buffer variables
        mutable std::mutex m_sampleMutex;
        std::vector<float> m_lastSamples;
    };
} // namespace Kalorite
