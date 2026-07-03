#include "CddaSource.hpp"

#ifdef KALORITE_HAVE_CDDA

#include <algorithm>
#include <cstring>
#include <chrono>

#include <QDebug>

#include <cdio/cdio.h>
#include <cdio/device.h>
#include <cdio/cd_types.h>
#include <cdio/paranoia/paranoia.h>

namespace Kalorite {

    // CDDA is always 16-bit signed, stereo, 44100 Hz. One raw sector is
    // CDIO_CD_FRAMESIZE_RAW (2352) bytes = 588 stereo frames.
    static constexpr int    kSamplesPerSector = CDIO_CD_FRAMESIZE_RAW / 2; // int16 samples
    static constexpr int    kFramesPerSector  = CDIO_CD_FRAMESIZE_RAW / 4; // stereo frames
    static constexpr ma_uint32 kSampleRate     = 44100;
    static constexpr ma_uint32 kChannels       = 2;

    // ------------------------------------------------------------------
    // Playlist path helpers: cdda:///dev/sr0?track=3
    // ------------------------------------------------------------------
    bool CddaSource::isCddaPath(const std::string& path) {
        return path.rfind("cdda://", 0) == 0;
    }

    std::string CddaSource::makePath(const std::string& device, int track) {
        return "cdda://" + device + "?track=" + std::to_string(track);
    }

    bool CddaSource::parsePath(const std::string& path, std::string& device, int& track) {
        if (!isCddaPath(path)) return false;
        std::string rest = path.substr(7); // after "cdda://"
        auto q = rest.find("?track=");
        if (q == std::string::npos) return false;
        device = rest.substr(0, q);
        try {
            track = std::stoi(rest.substr(q + 7));
        } catch (...) {
            return false;
        }
        return !device.empty() && track >= 1;
    }

    // ------------------------------------------------------------------
    // Disc discovery
    // ------------------------------------------------------------------
    std::vector<std::string> CddaSource::listDrives() {
        std::vector<std::string> drives;
        char** list = cdio_get_devices_with_cap(nullptr, CDIO_FS_MATCH_ALL, false);
        if (list) {
            for (int i = 0; list[i] != nullptr; ++i) drives.emplace_back(list[i]);
            cdio_free_device_list(list);
        }
        return drives;
    }

    std::vector<CddaTrackInfo> CddaSource::readToc(const std::string& device) {
        std::vector<CddaTrackInfo> tracks;
        cdrom_drive_t* d = cdio_cddap_identify(device.c_str(), CDDA_MESSAGE_FORGETIT, nullptr);
        if (!d) return tracks;
        if (cdio_cddap_open(d) != 0) {
            cdio_cddap_close(d);
            return tracks;
        }

        track_t count = cdio_cddap_tracks(d);
        for (track_t t = 1; t <= count; ++t) {
            if (!cdio_cddap_track_audiop(d, t)) continue; // skip data tracks
            CddaTrackInfo info;
            info.number      = t;
            info.firstSector = cdio_cddap_track_firstsector(d, t);
            info.lastSector  = cdio_cddap_track_lastsector(d, t);
            long sectors     = info.lastSector - info.firstSector + 1;
            info.lengthMs    = (long long)sectors * kFramesPerSector * 1000 / kSampleRate;
            info.audio       = true;
            tracks.push_back(info);
        }

        cdio_cddap_close(d);
        return tracks;
    }

    // ------------------------------------------------------------------
    // ma_data_source vtable trampolines
    // ------------------------------------------------------------------
    static ma_result cdda_read(ma_data_source* ds, void* out, ma_uint64 n, ma_uint64* read) {
        return reinterpret_cast<CddaSource*>(ds)->onRead(out, n, read);
    }
    static ma_result cdda_seek(ma_data_source* ds, ma_uint64 frame) {
        return reinterpret_cast<CddaSource*>(ds)->onSeek(frame);
    }
    static ma_result cdda_get_format(ma_data_source* ds, ma_format* f, ma_uint32* ch,
                                     ma_uint32* sr, ma_channel* map, size_t cap) {
        return reinterpret_cast<CddaSource*>(ds)->onGetDataFormat(f, ch, sr, map, cap);
    }
    static ma_result cdda_get_cursor(ma_data_source* ds, ma_uint64* c) {
        return reinterpret_cast<CddaSource*>(ds)->onGetCursor(c);
    }
    static ma_result cdda_get_length(ma_data_source* ds, ma_uint64* l) {
        return reinterpret_cast<CddaSource*>(ds)->onGetLength(l);
    }

    static ma_data_source_vtable g_cddaVTable = {
        cdda_read,
        cdda_seek,
        cdda_get_format,
        cdda_get_cursor,
        cdda_get_length,
        nullptr, // onSetLooping
        0        // flags
    };

    // ------------------------------------------------------------------
    // Construction / teardown
    // ------------------------------------------------------------------
    CddaSource::CddaSource(const std::string& device, int track) {
        ma_data_source_config cfg = ma_data_source_config_init();
        cfg.vtable = &g_cddaVTable;
        if (ma_data_source_init(&cfg, &m_base) != MA_SUCCESS) return;

        m_drive = cdio_cddap_identify(device.c_str(), CDDA_MESSAGE_FORGETIT, nullptr);
        if (!m_drive) {
            qWarning() << "CddaSource: cannot identify drive" << device.c_str();
            return;
        }
        if (cdio_cddap_open(m_drive) != 0) {
            qWarning() << "CddaSource: cannot open drive" << device.c_str();
            cdio_cddap_close(m_drive);
            m_drive = nullptr;
            return;
        }

        track_t count = cdio_cddap_tracks(m_drive);
        if (track < 1 || track > count || !cdio_cddap_track_audiop(m_drive, (track_t)track)) {
            qWarning() << "CddaSource: track" << track << "is not a valid audio track";
            return;
        }

        m_firstSector = cdio_cddap_track_firstsector(m_drive, (track_t)track);
        m_lastSector  = cdio_cddap_track_lastsector(m_drive, (track_t)track);
        long sectors  = m_lastSector - m_firstSector + 1;
        if (sectors <= 0) return;
        m_totalFrames = (ma_uint64)sectors * kFramesPerSector;

        m_paranoia = cdio_paranoia_init(m_drive);
        if (!m_paranoia) {
            qWarning() << "CddaSource: paranoia init failed";
            return;
        }
        // Prioritise smooth real-time streaming over exhaustive re-reads: a rare
        // glitch is better than an audible underrun on a healthy disc.
        cdio_paranoia_modeset(m_paranoia, PARANOIA_MODE_DISABLE);
        cdio_paranoia_seek(m_paranoia, m_firstSector, SEEK_SET);

        // ~3 s of prebuffer keeps the callback fed while the drive spins/seeks.
        m_capFrames = kSampleRate * 3;
        m_ring.assign(m_capFrames * kChannels, 0);

        m_valid = true;
        m_producer = std::thread(&CddaSource::producerLoop, this);
    }

    CddaSource::~CddaSource() {
        stopProducer();
        if (m_paranoia) cdio_paranoia_free(m_paranoia);
        if (m_drive)    cdio_cddap_close(m_drive);
        if (m_valid || m_base.vtable) ma_data_source_uninit(&m_base);
    }

    void CddaSource::stopProducer() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_cvProducer.notify_all();
        m_cvConsumer.notify_all();
        if (m_producer.joinable()) m_producer.join();
    }

    // ------------------------------------------------------------------
    // Background producer: reads sectors and fills the ring buffer.
    // ------------------------------------------------------------------
    void CddaSource::producerLoop() {
        long cur = m_firstSector;
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(m_mutex);

                if (m_seekRequested) {
                    long sectorOffset = (long)(m_seekTarget / kFramesPerSector);
                    cur = m_firstSector + sectorOffset;
                    m_head = m_tail = m_avail = 0;
                    m_cursor = (ma_uint64)sectorOffset * kFramesPerSector;
                    m_eof = false;
                    m_seekRequested = false;
                    cdio_paranoia_seek(m_paranoia, cur, SEEK_SET);
                }

                if (m_stop) return;

                if (cur > m_lastSector) {
                    // End of track: idle until a seek re-arms us (or we stop).
                    m_eof = true;
                    m_cvConsumer.notify_all();
                    m_cvProducer.wait(lock, [&] { return m_stop || m_seekRequested; });
                    if (m_stop) return;
                    continue;
                }

                // Wait for room for one more sector.
                m_cvProducer.wait(lock, [&] {
                    return m_stop || m_seekRequested || (m_avail + kFramesPerSector <= m_capFrames);
                });
                if (m_stop) return;
                if (m_seekRequested) continue;
            }

            // Read a sector WITHOUT holding the lock (this can block on the drive).
            int16_t* sector = cdio_paranoia_read(m_paranoia, nullptr);
            if (!sector) {
                // Read failure: treat as end of track so playback can advance.
                std::lock_guard<std::mutex> lock(m_mutex);
                cur = m_lastSector + 1;
                continue;
            }
            long readSector = cur;
            cur++;

            std::lock_guard<std::mutex> lock(m_mutex);
            // A seek may have landed while we were reading; discard this sector.
            if (m_seekRequested || m_stop) continue;
            if (readSector > m_lastSector) continue;
            for (int i = 0; i < kFramesPerSector; ++i) {
                size_t idx = ((m_tail + i) % m_capFrames) * kChannels;
                m_ring[idx + 0] = sector[i * 2 + 0];
                m_ring[idx + 1] = sector[i * 2 + 1];
            }
            m_tail = (m_tail + kFramesPerSector) % m_capFrames;
            m_avail += kFramesPerSector;
            m_cvConsumer.notify_all();
        }
    }

    // ------------------------------------------------------------------
    // Consumer: called from the real-time audio callback.
    // ------------------------------------------------------------------
    ma_result CddaSource::onRead(void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) {
        int16_t* out = static_cast<int16_t*>(pFramesOut);
        ma_uint64 done = 0;

        std::unique_lock<std::mutex> lock(m_mutex);
        while (done < frameCount) {
            if (m_avail == 0) {
                if (m_eof) break;
                if (m_stop) break;
                // Bounded wait: don't wedge the audio thread if the drive stalls.
                m_cvConsumer.wait_for(lock, std::chrono::milliseconds(500),
                                      [&] { return m_avail > 0 || m_eof || m_stop; });
                if (m_avail == 0) {
                    if (m_eof || m_stop) break;
                    // Underrun timeout: give back what we have to avoid a hang.
                    break;
                }
            }

            size_t n = (size_t)std::min<ma_uint64>(frameCount - done, m_avail);
            for (size_t i = 0; i < n; ++i) {
                size_t idx = ((m_head + i) % m_capFrames) * kChannels;
                out[(done + i) * 2 + 0] = m_ring[idx + 0];
                out[(done + i) * 2 + 1] = m_ring[idx + 1];
            }
            m_head = (m_head + n) % m_capFrames;
            m_avail -= n;
            m_cursor += n;
            done += n;
            m_cvProducer.notify_all();
        }

        if (pFramesRead) *pFramesRead = done;
        if (done == 0) return MA_AT_END;
        return MA_SUCCESS;
    }

    ma_result CddaSource::onSeek(ma_uint64 frameIndex) {
        if (frameIndex > m_totalFrames) frameIndex = m_totalFrames;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_seekRequested = true;
            m_seekTarget = frameIndex;
            m_head = m_tail = m_avail = 0;
            m_cursor = frameIndex;
            m_eof = false;
        }
        m_cvProducer.notify_all();
        m_cvConsumer.notify_all();
        return MA_SUCCESS;
    }

    ma_result CddaSource::onGetDataFormat(ma_format* f, ma_uint32* ch, ma_uint32* sr,
                                          ma_channel* /*map*/, size_t /*cap*/) {
        if (f)  *f  = ma_format_s16;
        if (ch) *ch = kChannels;
        if (sr) *sr = kSampleRate;
        return MA_SUCCESS;
    }

    ma_result CddaSource::onGetCursor(ma_uint64* pCursor) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (pCursor) *pCursor = m_cursor;
        return MA_SUCCESS;
    }

    ma_result CddaSource::onGetLength(ma_uint64* pLength) {
        if (pLength) *pLength = m_totalFrames;
        return MA_SUCCESS;
    }

} // namespace Kalorite

#endif // KALORITE_HAVE_CDDA
