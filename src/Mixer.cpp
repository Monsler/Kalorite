#include "Mixer.hpp"
#include "CddaSource.hpp"
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
                // Crossfade takes precedence when both are on: it needs the full
                // crossfade duration of lead time, whereas gapless only needs a
                // hair before the end. Using the gapless 0.15s here would make the
                // transition fire far too late for the crossfade to ever run.
                float threshold = pMixer->m_crossfadeEnabled ? pMixer->m_crossfadeDurationSec : 0.15f;
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
        devConfig.notificationCallback = deviceNotificationCallback;
        devConfig.pUserData = this;

        if (ma_device_init(&m_context, &devConfig, &m_device) != MA_SUCCESS) {
            qWarning() << "Failed to initialize miniaudio playback device";
            ma_context_uninit(&m_context);
            return;
        }
        m_deviceInitialized = true;

        // Cache the active device's name once, while the id is known valid.
        {
            ma_device_info info;
            if (ma_context_get_device_info(&m_context, ma_device_type_playback,
                    &m_device.playback.id, &info) == MA_SUCCESS) {
                m_currentDeviceName = info.name;
            }
        }

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
        // All teardown from here on is intentional; suppress loss recovery.
        m_expectedDeviceStop = true;
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

        // Audio CD: stream sectors live off the drive through a custom data
        // source. Always routed through the engine on a single slot (no
        // crossfade/gapless overlap, and no bit-perfect device) so we never run
        // two paranoia readers against one physical drive at once.
        if (CddaSource::isCddaPath(trackPath)) {
            setCurrentCdda(trackPath);
            return;
        }

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
                } else if (haveOutgoing) {
                    // Gapless swap while something is actually playing: start the
                    // incoming track at full volume and drop the outgoing one.
                    ma_sound_set_volume(inSound, vol);
                    ma_sound_start(inSound);
                    ma_sound_stop(outSound);
                    ma_sound_uninit(outSound);
                    *outInit = false;
                    m_crossfadeActive = false;
                } else {
                    // Nothing is playing yet — this is just a track being selected
                    // (e.g. added to the playlist). Load it into the slot but leave
                    // it stopped; play() will start it when the user asks. Without
                    // this, crossfade/gapless mode would auto-play on selection,
                    // out of sync with the UI (timer stopped, showing --:--).
                    ma_sound_set_volume(inSound, vol);
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

    void Mixer::setCurrentCdda(const std::string& trackPath) {
#ifdef KALORITE_HAVE_CDDA
        // Tear down anything currently playing (also frees a prior CD source).
        stop();

        std::string device;
        int track = 0;
        if (!CddaSource::parsePath(trackPath, device, track)) {
            qWarning() << "Mixer: malformed cdda path" << QString::fromStdString(trackPath);
            return;
        }

        auto* src = new CddaSource(device, track);
        if (!src->isValid()) {
            qWarning() << "Mixer: failed to open CD track" << track << "on" << device.c_str();
            delete src;
            return;
        }

        ma_result res = ma_sound_init_from_data_source(
            &m_engine, src->dataSource(), 0, NULL, &m_sound1);
        if (res != MA_SUCCESS) {
            qWarning() << "Mixer: ma_sound_init_from_data_source failed:" << res;
            delete src;
            return;
        }

        m_cddaSource = src;
        m_sound1Initialized = true;
        m_activeSoundIndex = 1;
        ma_sound_set_volume(&m_sound1, float(volume) / 100.0f);
#else
        Q_UNUSED(trackPath);
        qWarning() << "Mixer: Audio CD support not compiled in";
#endif
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

#ifdef KALORITE_HAVE_CDDA
        // The sound owning this data source is uninitialised above, so it is now
        // safe to stop the producer thread and release the drive.
        if (m_cddaSource) {
            delete static_cast<CddaSource*>(m_cddaSource);
            m_cddaSource = nullptr;
        }
#endif
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
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        return m_currentDeviceName;
    }

    std::vector<std::string> Mixer::getAudioDeviceNames() {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        std::vector<std::string> names;
        ma_device_info* pInfos = NULL;
        ma_uint32 count = 0;
        if (ma_context_get_devices(&m_context, &pInfos, &count, NULL, NULL) == MA_SUCCESS
            && pInfos != NULL) {
            names.reserve(count);
            for (ma_uint32 i = 0; i < count; ++i) names.emplace_back(pInfos[i].name);
        }
        return names;
    }

    // Caller must hold m_deviceMutex. Tears down and re-creates the engine's
    // playback device on the given id (NULL = system default).
    bool Mixer::reinitEngineDevice(ma_device_id* pDeviceID) {
        // Our own uninit triggers a "stopped" notification; flag it so the
        // notification handler doesn't treat it as an unexpected device loss.
        m_expectedDeviceStop = true;
        if (m_deviceInitialized) {
            ma_device_uninit(&m_device);
            m_deviceInitialized = false;
        }
        m_expectedDeviceStop = false;

        ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
        devConfig.playback.format   = ma_format_f32;
        devConfig.playback.channels = 2;
        devConfig.sampleRate        = 44100;
        devConfig.dataCallback      = dataCallback;
        devConfig.notificationCallback = deviceNotificationCallback;
        devConfig.pUserData         = this;
        devConfig.playback.pDeviceID = pDeviceID;

        if (ma_device_init(&m_context, &devConfig, &m_device) != MA_SUCCESS) {
            qWarning() << "Failed to (re)initialize audio output device";
            m_deviceInitialized = false;
            m_currentDeviceName.clear();
            return false;
        }
        m_deviceInitialized = true;

        ma_device_info info;
        if (ma_context_get_device_info(&m_context, ma_device_type_playback,
                &m_device.playback.id, &info) == MA_SUCCESS) {
            m_currentDeviceName = info.name;
        }
        ma_device_start(&m_device);
        return true;
    }

    void Mixer::setDeviceByName(const std::string& deviceName) {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        m_chosenDeviceName = deviceName;
        if (!m_deviceInitialized) return;

        ma_device_info* pPlaybackInfos = NULL;
        ma_uint32 playbackCount = 0;
        if (ma_context_get_devices(&m_context, &pPlaybackInfos, &playbackCount, NULL, NULL) != MA_SUCCESS
            || pPlaybackInfos == NULL) {
            qWarning() << "Failed to enumerate audio output devices";
            return;
        }

        ma_device_id* pChosenID = NULL;
        for (ma_uint32 i = 0; i < playbackCount; ++i) {
            if (deviceName == pPlaybackInfos[i].name) {
                pChosenID = &pPlaybackInfos[i].id;
                break;
            }
        }
        reinitEngineDevice(pChosenID);
    }

    void Mixer::deviceNotificationCallback(const ma_device_notification* pNotification) {
        if (!pNotification || !pNotification->pDevice) return;
        Mixer* self = static_cast<Mixer*>(pNotification->pDevice->pUserData);
        if (!self) return;

        if (pNotification->type == ma_device_notification_type_stopped
            && !self->m_expectedDeviceStop) {
            // The device stopped on its own (e.g. it was unplugged). Never touch
            // the device from inside this callback — hand off to the GUI thread.
            QMetaObject::invokeMethod(self, "recoverFromDeviceLoss", Qt::QueuedConnection);
        }
    }

    void Mixer::recoverFromDeviceLoss() {
        std::lock_guard<std::mutex> lock(m_deviceMutex);

        if (m_bitPerfectEnabled) {
            // Bit-perfect owns its own device; rebuild it on the current track.
            if (!m_bpTrackPath.empty()) {
                bool wasPlaying = m_bpPlaying;
                startBitPerfect(m_bpTrackPath, wasPlaying);
            }
            return;
        }

        // Try the user's chosen device again; if it's gone (typical on unplug),
        // fall back to the system default so playback can carry on.
        ma_device_id* pChosenID = NULL;
        ma_device_info* pInfos = NULL;
        ma_uint32 count = 0;
        if (!m_chosenDeviceName.empty()
            && ma_context_get_devices(&m_context, &pInfos, &count, NULL, NULL) == MA_SUCCESS
            && pInfos != NULL) {
            for (ma_uint32 i = 0; i < count; ++i) {
                if (m_chosenDeviceName == pInfos[i].name) { pChosenID = &pInfos[i].id; break; }
            }
        }
        reinitEngineDevice(pChosenID);
    }

    void Mixer::setEqBand(int bandIndex, float gainDb) {
        if (bandIndex < 0 || bandIndex >= 10) return;
        std::lock_guard<std::mutex> lock(m_eqMutex);
        m_eqGains[bandIndex] = gainDb;
        float frequencies[10] = {31.0f, 62.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f};
        if (m_eqFiltersInitialized[bandIndex]) {
            ma_peak2_config config = ma_peak2_config_init(ma_format_f32, 2, 44100, gainDb, 1.0, frequencies[bandIndex]);
            ma_peak2_reinit(&config, &m_eqFilters[bandIndex]);
        }
        // Keep the bit-perfect EQ filter (matched to the source's native
        // rate/channels) in sync so the change is heard in bit-perfect mode too.
        std::lock_guard<std::mutex> bpLock(m_bpMutex);
        if (m_bpEqFiltersInitialized[bandIndex]) {
            ma_peak2_config bpCfg = ma_peak2_config_init(ma_format_f32, m_bpChannels,
                m_bpSampleRate, gainDb, 1.0, frequencies[bandIndex]);
            ma_peak2_reinit(&bpCfg, &m_bpEqFilters[bandIndex]);
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
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        if (m_deviceInitialized) {
            m_expectedDeviceStop = true;
            ma_device_uninit(&m_device);
            m_expectedDeviceStop = false;
            m_deviceInitialized = false;

            ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
            devConfig.playback.format = ma_format_f32;
            devConfig.playback.channels = 2;
            devConfig.sampleRate = 44100;
            devConfig.dataCallback = dataCallback;
            devConfig.notificationCallback = deviceNotificationCallback;
            devConfig.pUserData = this;

            // Preserve the user's chosen output device across the reinit.
            ma_device_info* pInfos = NULL;
            ma_uint32 count = 0;
            if (!m_chosenDeviceName.empty()
                && ma_context_get_devices(&m_context, &pInfos, &count, NULL, NULL) == MA_SUCCESS
                && pInfos != NULL) {
                for (ma_uint32 i = 0; i < count; ++i) {
                    if (m_chosenDeviceName == pInfos[i].name) {
                        devConfig.playback.pDeviceID = &pInfos[i].id;
                        break;
                    }
                }
            }

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

        // Optional volume + EQ. Only touch the samples if the user actually moved
        // a control, so an untouched stream (100% volume, flat/off EQ) stays truly
        // bit-perfect and reaches the device byte-for-byte.
        if (framesRead > 0) {
            const float vol = float(pMixer->volume) / 100.0f;
            const bool doVol = pMixer->volume != 100;
            bool anyGain = false;
            if (pMixer->m_eqEnabled) {
                for (int i = 0; i < 10; ++i) {
                    if (std::fabs(pMixer->m_eqGains[i]) > 0.01f) { anyGain = true; break; }
                }
            }
            const bool doEq = anyGain;

            if (doVol || doEq) {
                const ma_uint32 ch = pMixer->m_bpChannels;
                const ma_uint64 sampleCount = framesRead * ch;

                // Get a float view of the frames: in place if the source is
                // already f32, otherwise convert into the scratch buffer.
                float* pf;
                const bool needConvert = (pMixer->m_bpFormat != ma_format_f32);
                if (needConvert) {
                    if (pMixer->m_bpFloatBuf.size() < sampleCount)
                        pMixer->m_bpFloatBuf.resize(sampleCount);
                    pf = pMixer->m_bpFloatBuf.data();
                    ma_convert_pcm_frames_format(pf, ma_format_f32, pOutput,
                        pMixer->m_bpFormat, framesRead, ch, ma_dither_mode_none);
                } else {
                    pf = static_cast<float*>(pOutput);
                }

                if (doEq) {
                    for (int i = 0; i < 10; ++i) {
                        if (pMixer->m_bpEqFiltersInitialized[i]) {
                            ma_peak2_process_pcm_frames(&pMixer->m_bpEqFilters[i], pf, pf, framesRead);
                        }
                    }
                }
                if (doVol) {
                    for (ma_uint64 k = 0; k < sampleCount; ++k) pf[k] *= vol;
                }

                if (needConvert) {
                    ma_convert_pcm_frames_format(pOutput, pMixer->m_bpFormat, pf,
                        ma_format_f32, framesRead, ch, ma_dither_mode_triangle);
                }
            }
        }

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

        // Remember the native format so the optional volume/EQ stage can convert
        // to/from f32, and build an EQ filter set matched to this rate/channels.
        m_bpFormat     = m_bpDecoder.outputFormat;
        m_bpChannels   = m_bpDecoder.outputChannels;
        m_bpSampleRate = m_bpDecoder.outputSampleRate;
        initBitPerfectEq();

        // Match the device exactly to the decoder's native output.
        ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
        devConfig.playback.format   = m_bpDecoder.outputFormat;
        devConfig.playback.channels = m_bpDecoder.outputChannels;
        devConfig.sampleRate        = m_bpDecoder.outputSampleRate;
        devConfig.dataCallback      = bitPerfectDataCallback;
        devConfig.notificationCallback = deviceNotificationCallback;
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

    // Build (or rebuild) the bit-perfect EQ filters using the current source's
    // native sample rate / channel count and the current per-band gains.
    void Mixer::initBitPerfectEq() {
        static const float frequencies[10] = {31.0f, 62.0f, 125.0f, 250.0f, 500.0f,
                                              1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f};
        for (int i = 0; i < 10; ++i) {
            if (m_bpEqFiltersInitialized[i]) {
                ma_peak2_uninit(&m_bpEqFilters[i], NULL);
                m_bpEqFiltersInitialized[i] = false;
            }
            ma_peak2_config cfg = ma_peak2_config_init(ma_format_f32, m_bpChannels,
                m_bpSampleRate, m_eqGains[i], 1.0, frequencies[i]);
            if (ma_peak2_init(&cfg, NULL, &m_bpEqFilters[i]) == MA_SUCCESS) {
                m_bpEqFiltersInitialized[i] = true;
            }
        }
    }

    void Mixer::stopBitPerfect() {
        // Uninit the device first: this stops and joins the audio thread, so no
        // callback can be running afterwards and it is safe to free the decoder.
        if (m_bpDeviceInitialized) {
            // Flag our intentional stop so the notification handler ignores it.
            m_expectedDeviceStop = true;
            ma_device_uninit(&m_bpDevice);
            m_expectedDeviceStop = false;
            m_bpDeviceInitialized = false;
        }
        m_bpPlaying = false;
        if (m_bpDecoderInitialized) {
            ma_decoder_uninit(&m_bpDecoder);
            m_bpDecoderInitialized = false;
        }
        for (int i = 0; i < 10; ++i) {
            if (m_bpEqFiltersInitialized[i]) {
                ma_peak2_uninit(&m_bpEqFilters[i], NULL);
                m_bpEqFiltersInitialized[i] = false;
            }
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
                // Intentional stop: don't let the notification handler treat it
                // as an unexpected device loss and try to recover.
                m_expectedDeviceStop = true;
                ma_device_stop(&m_device);
                m_expectedDeviceStop = false;
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
