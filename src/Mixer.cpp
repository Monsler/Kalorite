#include "Mixer.hpp"
#include <QDebug>
#include <cmath>

namespace Kalorite {

    void Mixer::dataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        Mixer* pMixer = static_cast<Mixer*>(pDevice->pUserData);
        if (!pMixer) return;

        // Perform standard mixing / engine processing
        ma_engine_read_pcm_frames(&pMixer->m_engine, pOutput, frameCount, NULL);

        // Apply crossfade and volume logic in audio callback thread
        float* pFloatOutput = static_cast<float*>(pOutput);
        ma_uint32 channelCount = pDevice->playback.channels;
        ma_uint32 sampleCount = frameCount * channelCount;

        // 1a. Crossfade volume ramp: both sounds are playing simultaneously and we
        // smoothly ramp the incoming sound up while ramping the outgoing one down,
        // driven by the incoming sound's playback cursor.
        if (pMixer->m_crossfadeActive) {
            bool s1 = pMixer->m_sound1Initialized && ma_sound_is_playing(&pMixer->m_sound1);
            bool s2 = pMixer->m_sound2Initialized && ma_sound_is_playing(&pMixer->m_sound2);
            if (s1 && s2) {
                ma_sound* in  = (pMixer->m_activeSoundIndex == 1) ? &pMixer->m_sound1 : &pMixer->m_sound2;
                ma_sound* out = (pMixer->m_activeSoundIndex == 1) ? &pMixer->m_sound2 : &pMixer->m_sound1;
                float cur = 0.0f;
                ma_sound_get_cursor_in_seconds(in, &cur);
                float ratio = cur / pMixer->m_crossfadeDurationSec;
                if (ratio < 0.0f) ratio = 0.0f;
                if (ratio > 1.0f) ratio = 1.0f;
                float vol = float(pMixer->volume) / 100.0f;
                ma_sound_set_volume(in, ratio * vol);
                ma_sound_set_volume(out, (1.0f - ratio) * vol);
                if (ratio >= 1.0f) {
                    ma_sound_stop(out);
                    pMixer->m_crossfadeActive = false;
                }
            } else {
                pMixer->m_crossfadeActive = false;
            }
        }

        // 1b. End-of-track detection: request the next track shortly before the
        // active sound ends so a crossfade/gapless transition can begin in time.
        if (pMixer->m_crossfadeEnabled || pMixer->m_gaplessEnabled) {
            ma_sound* active = NULL;
            if (pMixer->m_activeSoundIndex == 1 && pMixer->m_sound1Initialized) active = &pMixer->m_sound1;
            else if (pMixer->m_activeSoundIndex == 2 && pMixer->m_sound2Initialized) active = &pMixer->m_sound2;

            if (active && ma_sound_is_playing(active) && !pMixer->m_crossfadeTriggered) {
                float lenSec = 0.0f;
                float curSec = 0.0f;
                ma_sound_get_length_in_seconds(active, &lenSec);
                ma_sound_get_cursor_in_seconds(active, &curSec);
                float threshold = pMixer->m_gaplessEnabled ? 0.15f : pMixer->m_crossfadeDurationSec;
                if (lenSec > threshold && curSec >= (lenSec - threshold)) {
                    pMixer->m_crossfadeTriggered = true;
                    QMetaObject::invokeMethod(pMixer, "playbackFinished", Qt::QueuedConnection);
                }
            }
        }

        // 2. Smart Gain (peak limiting to avoid clipping). This is deliberately a
        // separate feature from Bit Perfect, which bypasses this callback entirely.
        if (pMixer->m_smartGainEnabled) {
            float peak = 0.0f;
            for (ma_uint32 i = 0; i < sampleCount; ++i) {
                float absVal = std::abs(pFloatOutput[i]);
                if (absVal > peak) {
                    peak = absVal;
                }
            }

            // If peak exceeds 0.8 (clipping / high volume zone), compress/limit it
            if (peak > 0.8f) {
                float scale = 0.8f / peak;
                for (ma_uint32 i = 0; i < sampleCount; ++i) {
                    pFloatOutput[i] *= scale;
                }
            }
        }

        // 3. EQ Processing
        {
            std::lock_guard<std::mutex> lock(pMixer->m_eqMutex);
            if (pMixer->m_eqEnabled) {
                for (int i = 0; i < 10; ++i) {
                    if (pMixer->m_eqFiltersInitialized[i]) {
                        ma_peak2_process_pcm_frames(&pMixer->m_eqFilters[i], pFloatOutput, pFloatOutput, frameCount);
                    }
                }
            }
        }

        // 4. Save latest PCM float samples
        {
            std::lock_guard<std::mutex> lock(pMixer->m_sampleMutex);
            int toCopy = std::min(sampleCount, (ma_uint32)pMixer->m_lastSamples.size());
            if (toCopy > 0) {
                if (toCopy == (int)pMixer->m_lastSamples.size()) {
                    std::copy(pFloatOutput + sampleCount - toCopy, pFloatOutput + sampleCount, pMixer->m_lastSamples.begin());
                } else {
                    std::copy(pMixer->m_lastSamples.begin() + toCopy, pMixer->m_lastSamples.end(), pMixer->m_lastSamples.begin());
                    std::copy(pFloatOutput, pFloatOutput + toCopy, pMixer->m_lastSamples.end() - toCopy);
                }
            }
        }
    }

    Mixer::Mixer() {
        m_lastSamples.resize(2048, 0.0f);

        // Common EQ bands: 31, 62, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz
        float frequencies[10] = {31.0f, 62.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f};
        for (int i = 0; i < 10; ++i) {
            m_eqGains[i] = 0.0f; // 0dB
            ma_peak2_config config = ma_peak2_config_init(ma_format_f32, 2, 44100, 0.0, 1.0, frequencies[i]);
            if (ma_peak2_init(&config, NULL, &m_eqFilters[i]) == MA_SUCCESS) {
                m_eqFiltersInitialized[i] = true;
            } else {
                m_eqFiltersInitialized[i] = false;
                qWarning() << "Failed to initialize EQ filter for band" << frequencies[i] << "Hz";
            }
        }

        if (ma_context_init(NULL, 0, NULL, &m_context) != MA_SUCCESS) {
            qWarning() << "Failed to initialize miniaudio context";
            return;
        }

        // Setup default playback device config
        ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
        devConfig.playback.format = ma_format_f32; // Floating point for processing / effects
        devConfig.playback.channels = 2;
        devConfig.sampleRate = 44100;
        devConfig.dataCallback = dataCallback;
        devConfig.pUserData = this;

        if (ma_device_init(&m_context, &devConfig, &m_device) != MA_SUCCESS) {
            qWarning() << "Failed to initialize miniaudio playback device";
            ma_context_uninit(&m_context);
            return;
        }
        m_deviceInitialized = true;

        // Initialize engine config referencing the custom device
        ma_engine_config engineConfig = ma_engine_config_init();
        engineConfig.pDevice = &m_device; 

        if (ma_engine_init(&engineConfig, &m_engine) != MA_SUCCESS) {
            qWarning() << "Failed to initialize miniaudio engine";
            ma_device_uninit(&m_device);
            ma_context_uninit(&m_context);
            m_deviceInitialized = false;
            return;
        }
        m_engineInitialized = true;

        // Start device immediately
        ma_device_start(&m_device);
    }

    Mixer::~Mixer() {
        stop();
        for (int i = 0; i < 10; ++i) {
            if (m_eqFiltersInitialized[i]) {
                ma_peak2_uninit(&m_eqFilters[i], NULL);
            }
        }
        if (m_sound1Initialized) {
            ma_sound_uninit(&m_sound1);
        }
        if (m_sound2Initialized) {
            ma_sound_uninit(&m_sound2);
        }
        if (m_engineInitialized) {
            ma_engine_uninit(&m_engine);
        }
        if (m_deviceInitialized) {
            ma_device_uninit(&m_device);
        }
        ma_context_uninit(&m_context);
    }

    void Mixer::setCurrent(const std::string& trackPath) {
        currentTrack = trackPath;
        m_crossfadeTriggered = false;

        // Bit Perfect: bypass the engine entirely and stream the source straight
        // to a dedicated device that matches the file's native format.
        if (m_bitPerfectEnabled) {
            startBitPerfect(trackPath, false /* leave paused until play() */);
            return;
        }

        // Use streaming (0) instead of decoding to memory (MA_SOUND_FLAG_DECODE)
        // to avoid blocking the GUI thread when loading or seeking.
        ma_uint32 flags = 0;
        qDebug() << "Mixer::setCurrent - loading file:" << QString::fromStdString(trackPath);

        float vol = float(volume) / 100.0f;

        if (m_crossfadeEnabled || m_gaplessEnabled) {
            // Collapse any crossfade still in flight so we only ever juggle two
            // slots: the current active sound (outgoing) and one free slot.
            finalizeCrossfade();

            // The new track always goes into the slot that is NOT currently active.
            int incoming = (m_activeSoundIndex == 1) ? 2 : 1;
            ma_sound* inSound = (incoming == 1) ? &m_sound1 : &m_sound2;
            bool*     inInit  = (incoming == 1) ? &m_sound1Initialized : &m_sound2Initialized;

            ma_sound* outSound = NULL;
            bool*     outInit  = NULL;
            if (m_activeSoundIndex == 1 && m_sound1Initialized) { outSound = &m_sound1; outInit = &m_sound1Initialized; }
            else if (m_activeSoundIndex == 2 && m_sound2Initialized) { outSound = &m_sound2; outInit = &m_sound2Initialized; }

            bool haveOutgoing = outSound && ma_sound_is_playing(outSound);

            // Free the target slot (may hold a stopped leftover from a prior swap).
            if (*inInit) {
                ma_sound_uninit(inSound);
                *inInit = false;
            }

            ma_result res = ma_sound_init_from_file(&m_engine, trackPath.c_str(), flags, NULL, NULL, inSound);
            qDebug() << "ma_sound_init_from_file (crossfade slot" << incoming << ") result:" << res;
            if (res == MA_SUCCESS) {
                *inInit = true;
                m_activeSoundIndex = incoming;

                if (m_crossfadeEnabled && haveOutgoing) {
                    // Overlap: start the incoming track silently and let the audio
                    // callback ramp it up while ramping the outgoing one down.
                    ma_sound_set_volume(inSound, 0.0f);
                    ma_sound_start(inSound);
                    m_crossfadeActive = true;
                } else {
                    // Gapless swap, or nothing was playing: start at full volume.
                    ma_sound_set_volume(inSound, vol);
                    ma_sound_start(inSound);
                    if (haveOutgoing) {
                        ma_sound_stop(outSound);
                        ma_sound_uninit(outSound);
                        *outInit = false;
                    }
                    m_crossfadeActive = false;
                }
            }
        } else {
            // Non-crossfade standard transition
            stop();
            ma_result res = ma_sound_init_from_file(&m_engine, trackPath.c_str(), flags, NULL, NULL, &m_sound1);
            qDebug() << "ma_sound_init_from_file (no crossfade) result:" << res;
            if (res == MA_SUCCESS) {
                m_sound1Initialized = true;
                ma_sound_set_volume(&m_sound1, vol);
                m_activeSoundIndex = 1;

                float lenSec = 0.0f;
                ma_sound_get_length_in_seconds(&m_sound1, &lenSec);
                qDebug() << "Loaded length in seconds:" << lenSec;
            }
        }
    }

    void Mixer::setVolume(int volume) {
        this->volume = volume;
        float volRatio = float(volume) / 100.0f;
        if (m_sound1Initialized) {
            ma_sound_set_volume(&m_sound1, volRatio);
        }
        if (m_sound2Initialized) {
            ma_sound_set_volume(&m_sound2, volRatio);
        }
    }

    void Mixer::setPosition(int positionMs) {
        if (m_bitPerfectEnabled) {
            std::lock_guard<std::mutex> lock(m_bpMutex);
            if (m_bpDecoderInitialized) {
                ma_uint64 frame = (ma_uint64)positionMs * m_bpDecoder.outputSampleRate / 1000;
                ma_decoder_seek_to_pcm_frame(&m_bpDecoder, frame);
            }
            return;
        }
        float sec = float(positionMs) / 1000.0f;
        if (m_activeSoundIndex == 1 && m_sound1Initialized) {
            ma_sound_seek_to_second(&m_sound1, sec);
        } else if (m_activeSoundIndex == 2 && m_sound2Initialized) {
            ma_sound_seek_to_second(&m_sound2, sec);
        }

        // Re-arm the end-of-track crossfade/gapless trigger after a seek so it
        // fires again relative to the new position (e.g. seeking back re-arms it,
        // seeking into the tail starts the transition immediately).
        m_crossfadeTriggered = false;
    }

    void Mixer::play() {
        if (m_bitPerfectEnabled) {
            m_bpPlaying = m_bpDecoderInitialized;
            return;
        }
        if (m_activeSoundIndex == 1 && m_sound1Initialized) {
            ma_sound_start(&m_sound1);
        } else if (m_activeSoundIndex == 2 && m_sound2Initialized) {
            ma_sound_start(&m_sound2);
        }
    }

    void Mixer::pause() {
        if (m_bitPerfectEnabled) {
            m_bpPlaying = false;
            return;
        }
        if (m_activeSoundIndex == 1 && m_sound1Initialized) {
            ma_sound_stop(&m_sound1);
        } else if (m_activeSoundIndex == 2 && m_sound2Initialized) {
            ma_sound_stop(&m_sound2);
        }
    }

    void Mixer::finalizeCrossfade() {
        if (!m_crossfadeActive) return;
        m_crossfadeActive = false; // stop the callback ramp from touching the slots

        float vol = float(volume) / 100.0f;
        // The outgoing sound lives in whichever slot is not currently active.
        if (m_activeSoundIndex == 1) {
            if (m_sound2Initialized) {
                ma_sound_stop(&m_sound2);
                ma_sound_uninit(&m_sound2);
                m_sound2Initialized = false;
            }
            if (m_sound1Initialized) ma_sound_set_volume(&m_sound1, vol);
        } else if (m_activeSoundIndex == 2) {
            if (m_sound1Initialized) {
                ma_sound_stop(&m_sound1);
                ma_sound_uninit(&m_sound1);
                m_sound1Initialized = false;
            }
            if (m_sound2Initialized) ma_sound_set_volume(&m_sound2, vol);
        }
    }

    void Mixer::stop() {
        stopBitPerfect();
        if (m_sound1Initialized) {
            ma_sound_stop(&m_sound1);
            ma_sound_uninit(&m_sound1);
            m_sound1Initialized = false;
        }
        if (m_sound2Initialized) {
            ma_sound_stop(&m_sound2);
            ma_sound_uninit(&m_sound2);
            m_sound2Initialized = false;
        }
        m_activeSoundIndex = 0;
        m_crossfadeActive = false;
    }

    int Mixer::getPositionMs() const {
        if (m_bitPerfectEnabled && m_bpDecoderInitialized) {
            ma_decoder* dec = const_cast<ma_decoder*>(&m_bpDecoder);
            ma_uint64 cursor = 0;
            ma_decoder_get_cursor_in_pcm_frames(dec, &cursor);
            if (m_bpDecoder.outputSampleRate == 0) return 0;
            return (int)(cursor * 1000 / m_bpDecoder.outputSampleRate);
        }
        float sec = 0.0f;
        if (m_activeSoundIndex == 1 && m_sound1Initialized) {
            ma_sound_get_cursor_in_seconds(&m_sound1, &sec);
        } else if (m_activeSoundIndex == 2 && m_sound2Initialized) {
            ma_sound_get_cursor_in_seconds(&m_sound2, &sec);
        }
        return qRound(sec * 1000.0f);
    }

    int Mixer::getDurationMs() const {
        if (m_bitPerfectEnabled && m_bpDecoderInitialized) {
            ma_decoder* dec = const_cast<ma_decoder*>(&m_bpDecoder);
            ma_uint64 len = 0;
            ma_decoder_get_length_in_pcm_frames(dec, &len);
            if (m_bpDecoder.outputSampleRate == 0) return 0;
            return (int)(len * 1000 / m_bpDecoder.outputSampleRate);
        }
        float lenSec = 0.0f;
        if (m_activeSoundIndex == 1 && m_sound1Initialized) {
            ma_sound_get_length_in_seconds(&m_sound1, &lenSec);
        } else if (m_activeSoundIndex == 2 && m_sound2Initialized) {
            ma_sound_get_length_in_seconds(&m_sound2, &lenSec);
        }
        return qRound(lenSec * 1000.0f);
    }

    int Mixer::getSourceChannels() const {
        ma_format format;
        ma_uint32 channels = 0;
        ma_uint32 sampleRate = 0;

        if (m_bitPerfectEnabled && m_bpDecoderInitialized) {
            return (int)m_bpDecoder.outputChannels;
        }
        if (m_activeSoundIndex == 1 && m_sound1Initialized) {
            ma_sound_get_data_format(const_cast<ma_sound*>(&m_sound1), &format, &channels, &sampleRate, NULL, 0);
        } else if (m_activeSoundIndex == 2 && m_sound2Initialized) {
            ma_sound_get_data_format(const_cast<ma_sound*>(&m_sound2), &format, &channels, &sampleRate, NULL, 0);
        }
        return (int)channels;
    }

    int Mixer::getSourceSampleRate() const {
        ma_format format;
        ma_uint32 channels = 0;
        ma_uint32 sampleRate = 0;

        if (m_bitPerfectEnabled && m_bpDecoderInitialized) {
            return (int)m_bpDecoder.outputSampleRate;
        }
        if (m_activeSoundIndex == 1 && m_sound1Initialized) {
            ma_sound_get_data_format(const_cast<ma_sound*>(&m_sound1), &format, &channels, &sampleRate, NULL, 0);
        } else if (m_activeSoundIndex == 2 && m_sound2Initialized) {
            ma_sound_get_data_format(const_cast<ma_sound*>(&m_sound2), &format, &channels, &sampleRate, NULL, 0);
        }
        return (int)sampleRate;
    }

    bool Mixer::isPlaying() const {
        if (m_bitPerfectEnabled) {
            return m_bpPlaying;
        }
        if (m_activeSoundIndex == 1 && m_sound1Initialized) {
            return ma_sound_is_playing(&m_sound1);
        } else if (m_activeSoundIndex == 2 && m_sound2Initialized) {
            return ma_sound_is_playing(&m_sound2);
        }
        return false;
    }

    std::string Mixer::getCurrentDeviceName() {
        if (!m_deviceInitialized) return "";
        ma_device_info info;
        if (ma_context_get_device_info(&m_context, ma_device_type_playback, &m_device.playback.id, &info) == MA_SUCCESS) {
            return info.name;
        }
        return "";
    }

    void Mixer::setDeviceByName(const std::string& deviceName) {
        m_chosenDeviceName = deviceName;
        if (!m_deviceInitialized) return;

        ma_device_info* pPlaybackInfos;
        ma_uint32 playbackCount;
        if (ma_context_get_devices(&m_context, &pPlaybackInfos, &playbackCount, NULL, NULL) == MA_SUCCESS) {
            ma_device_id* pChosenID = NULL;
            for (ma_uint32 i = 0; i < playbackCount; ++i) {
                if (deviceName == pPlaybackInfos[i].name) {
                    pChosenID = &pPlaybackInfos[i].id;
                    break;
                }
            }

            // Re-init device with new ID
            ma_device_uninit(&m_device);
            ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
            devConfig.playback.format = ma_format_f32;
            devConfig.playback.channels = 2;
            devConfig.sampleRate = 44100;
            devConfig.dataCallback = dataCallback;
            devConfig.pUserData = this;
            devConfig.playback.pDeviceID = pChosenID;

            if (ma_device_init(&m_context, &devConfig, &m_device) != MA_SUCCESS) {
                qWarning() << "Failed to switch audio output device";
                m_deviceInitialized = false;
            } else {
                m_deviceInitialized = true;
                ma_device_start(&m_device);
            }
        }
    }

    void Mixer::setEqBand(int bandIndex, float gainDb) {
        if (bandIndex < 0 || bandIndex >= 10) return;
        std::lock_guard<std::mutex> lock(m_eqMutex);
        m_eqGains[bandIndex] = gainDb;
        if (m_eqFiltersInitialized[bandIndex]) {
            float frequencies[10] = {31.0f, 62.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f};
            ma_peak2_config config = ma_peak2_config_init(ma_format_f32, 2, 44100, gainDb, 1.0, frequencies[bandIndex]);
            ma_peak2_reinit(&config, &m_eqFilters[bandIndex]);
        }
    }

    void Mixer::setEqEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(m_eqMutex);
        m_eqEnabled = enabled;
    }

    void Mixer::setDoubleBufferingEnabled(bool enabled) {
        if (m_doubleBufferingEnabled == enabled) return;
        m_doubleBufferingEnabled = enabled;

        // Reinitialize device with updated buffer size config
        if (m_deviceInitialized) {
            ma_device_uninit(&m_device);
            m_deviceInitialized = false;

            ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
            devConfig.playback.format = ma_format_f32;
            devConfig.playback.channels = 2;
            devConfig.sampleRate = 44100;
            devConfig.dataCallback = dataCallback;
            devConfig.pUserData = this;

            if (enabled) {
                // Low latency / Double buffering: explicitly set a small buffer period
                devConfig.periodSizeInFrames = 256; 
                devConfig.periods = 2; // Double buffering
            }

            if (ma_device_init(&m_context, &devConfig, &m_device) == MA_SUCCESS) {
                m_deviceInitialized = true;
                ma_device_start(&m_device);
            } else {
                qWarning() << "Failed to reinitialize miniaudio device with new buffering mode";
            }
        }
    }

    std::vector<float> Mixer::getLatestSamples() const {
        std::lock_guard<std::mutex> lock(m_sampleMutex);
        return m_lastSamples;
    }

    // --- Bit Perfect ---------------------------------------------------------
    // Bit Perfect streams the decoded frames straight to a dedicated device that
    // matches the source's native format, sample rate and channel count. No
    // engine, no EQ, no resampling, no volume scaling and no smart gain are
    // applied - the bytes reach the sound card untouched.

    void Mixer::bitPerfectDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        (void)pInput;
        Mixer* pMixer = static_cast<Mixer*>(pDevice->pUserData);
        if (!pMixer) return;

        std::lock_guard<std::mutex> lock(pMixer->m_bpMutex);
        if (!pMixer->m_bpDecoderInitialized || !pMixer->m_bpPlaying) {
            return; // Output was pre-zeroed by miniaudio -> silence.
        }

        ma_uint64 framesRead = 0;
        ma_decoder_read_pcm_frames(&pMixer->m_bpDecoder, pOutput, frameCount, &framesRead);

        // Read-only visualiser capture (only when the native format is f32 so we
        // never touch or convert the audio that goes to the device).
        if (pDevice->playback.format == ma_format_f32 && framesRead > 0) {
            ma_uint32 channels = pDevice->playback.channels;
            ma_uint32 sampleCount = (ma_uint32)framesRead * channels;
            const float* pFloat = static_cast<const float*>(pOutput);
            std::lock_guard<std::mutex> slock(pMixer->m_sampleMutex);
            int toCopy = std::min(sampleCount, (ma_uint32)pMixer->m_lastSamples.size());
            if (toCopy > 0) {
                std::copy(pFloat + sampleCount - toCopy, pFloat + sampleCount, pMixer->m_lastSamples.begin());
            }
        }

        if (framesRead < frameCount) {
            // Reached the end of the stream.
            pMixer->m_bpPlaying = false;
            QMetaObject::invokeMethod(pMixer, "playbackFinished", Qt::QueuedConnection);
        }
    }

    bool Mixer::startBitPerfect(const std::string& trackPath, bool autostart) {
        stopBitPerfect();

        // Decode to the file's native format (ma_format_unknown keeps it as-is).
        ma_decoder_config decConfig = ma_decoder_config_init(ma_format_unknown, 0, 0);
        ma_result res = ma_decoder_init_file(trackPath.c_str(), &decConfig, &m_bpDecoder);
        if (res != MA_SUCCESS) {
            qWarning() << "Bit Perfect: failed to open decoder for" << QString::fromStdString(trackPath) << "result:" << res;
            return false;
        }
        m_bpDecoderInitialized = true;
        m_bpTrackPath = trackPath;

        // Match the device exactly to the decoder's native output.
        ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
        devConfig.playback.format   = m_bpDecoder.outputFormat;
        devConfig.playback.channels = m_bpDecoder.outputChannels;
        devConfig.sampleRate        = m_bpDecoder.outputSampleRate;
        devConfig.dataCallback      = bitPerfectDataCallback;
        devConfig.pUserData         = this;

        // Honour the user's chosen output device if one was selected.
        if (!m_chosenDeviceName.empty()) {
            ma_device_info* pInfos = NULL;
            ma_uint32 count = 0;
            if (ma_context_get_devices(&m_context, &pInfos, &count, NULL, NULL) == MA_SUCCESS) {
                for (ma_uint32 i = 0; i < count; ++i) {
                    if (m_chosenDeviceName == pInfos[i].name) {
                        devConfig.playback.pDeviceID = &pInfos[i].id;
                        break;
                    }
                }
            }
        }

        if (ma_device_init(&m_context, &devConfig, &m_bpDevice) != MA_SUCCESS) {
            qWarning() << "Bit Perfect: failed to init native device";
            ma_decoder_uninit(&m_bpDecoder);
            m_bpDecoderInitialized = false;
            return false;
        }
        m_bpDeviceInitialized = true;
        m_bpPlaying = autostart;

        qDebug() << "Bit Perfect active:" << m_bpDecoder.outputSampleRate << "Hz,"
                 << m_bpDecoder.outputChannels << "ch, format" << m_bpDecoder.outputFormat;

        ma_device_start(&m_bpDevice);
        return true;
    }

    void Mixer::stopBitPerfect() {
        // Uninit the device first: this stops and joins the audio thread, so no
        // callback can be running afterwards and it is safe to free the decoder.
        if (m_bpDeviceInitialized) {
            ma_device_uninit(&m_bpDevice);
            m_bpDeviceInitialized = false;
        }
        m_bpPlaying = false;
        if (m_bpDecoderInitialized) {
            ma_decoder_uninit(&m_bpDecoder);
            m_bpDecoderInitialized = false;
        }
    }

    void Mixer::setBitPerfectEnabled(bool enabled) {
        if (m_bitPerfectEnabled == enabled) return;

        if (enabled) {
            // Capture engine playback state before switching modes.
            bool wasPlaying = isPlaying();
            int posMs = getPositionMs();
            std::string track = currentTrack;

            // Tear down the engine path and silence its device.
            stop();
            if (m_deviceInitialized) {
                ma_device_stop(&m_device);
            }

            m_bitPerfectEnabled = true;

            if (!track.empty()) {
                if (startBitPerfect(track, wasPlaying) && posMs > 0) {
                    setPosition(posMs);
                }
            }
        } else {
            bool wasPlaying = isPlaying();
            int posMs = getPositionMs();
            std::string track = currentTrack;

            stopBitPerfect();
            m_bitPerfectEnabled = false;

            // Resume the engine device and reload the track through the engine.
            if (m_deviceInitialized) {
                ma_device_start(&m_device);
            }
            if (!track.empty()) {
                setCurrent(track);
                if (posMs > 0) setPosition(posMs);
                if (wasPlaying) play();
            }
        }
    }
}
