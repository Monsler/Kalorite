#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
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
        // Enumerate playback device names via miniaudio (the same names used by
        // setDeviceByName / getCurrentDeviceName), so the UI stays consistent.
        std::vector<std::string> getAudioDeviceNames();

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

    private slots:
        // Recovers playback on the GUI thread after the output device stops
        // unexpectedly (e.g. the audio device was unplugged mid-playback).
        void recoverFromDeviceLoss();

    private:
        static void dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
        static void bitPerfectDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
        static void deviceNotificationCallback(const ma_device_notification* pNotification);

        // Re-initialize the engine playback device to the given id (NULL = system
        // default). Caller must hold m_deviceMutex.
        bool reinitEngineDevice(ma_device_id* pDeviceID);

        // Bit-perfect helpers
        bool startBitPerfect(const std::string& trackPath, bool autostart);
        void stopBitPerfect();

        // Audio CD: build a live CDDA data source and attach it to slot 1.
        void setCurrentCdda(const std::string& trackPath);
        // Owned CddaSource* for the currently playing CD track (opaque here to
        // keep libcdio out of this header). Freed in stop().
        void* m_cddaSource = nullptr;

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

        // Serializes context enumeration and device (re)init/uninit so the GUI
        // thread never races the audio thread over the miniaudio context/device.
        std::mutex m_deviceMutex;
        // Cached name of the active playback device (avoids querying a possibly
        // stale/invalid device id from the backend on every call).
        std::string m_currentDeviceName;
        // Set true while we intentionally stop/uninit a device, so the stop
        // notification is not mistaken for an unexpected device loss.
        std::atomic<bool> m_expectedDeviceStop{false};

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

        // Optional volume + EQ for bit-perfect. These are applied only when the
        // user actually moves them (volume != 100 or a non-zero EQ gain); while
        // untouched the stream stays truly bit-perfect. The filters are matched
        // to the source's native rate/channels, unlike the engine's 44.1k/2ch set.
        ma_format m_bpFormat = ma_format_f32;
        ma_uint32 m_bpChannels = 2;
        ma_uint32 m_bpSampleRate = 44100;
        ma_peak2 m_bpEqFilters[10];
        bool m_bpEqFiltersInitialized[10] = {false};
        std::vector<float> m_bpFloatBuf; // scratch buffer for format conversion
        void initBitPerfectEq();         // (re)build the bp EQ filter set

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
