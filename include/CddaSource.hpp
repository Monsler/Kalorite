#pragma once

// Live Audio-CD streaming source for the miniaudio engine.
//
// A CddaSource is a custom ma_data_source that reads raw CDDA sectors from the
// optical drive on the fly (no intermediate WAV files on disk) using
// libcdio_paranoia. A background producer thread pulls sectors into a ring
// buffer so the real-time audio callback never blocks on the (slow) drive.
//
// The whole feature is compiled out on platforms without libcdio (e.g. macOS,
// which no longer ships optical drives) via KALORITE_HAVE_CDDA.

#include <string>
#include <vector>
#include <cstdint>

#ifdef KALORITE_HAVE_CDDA

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "miniaudio.h"

// Opaque libcdio types kept out of the public header so callers don't need the
// libcdio include path.
typedef struct cdrom_drive_s cdrom_drive_t;
typedef struct cdrom_paranoia_s cdrom_paranoia_t;

namespace Kalorite {

    // One audio track on a disc, as reported by the drive's TOC.
    struct CddaTrackInfo {
        int      number = 0;   // 1-based track number
        long     firstSector = 0;
        long     lastSector = 0;
        long long lengthMs = 0;
        bool     audio = true;
    };

    // A playing CDDA track. The first member MUST be ma_data_source_base so a
    // CddaSource* can be handed to ma_sound_init_from_data_source().
    class CddaSource {
    public:
        // --- Playlist path helpers ---------------------------------------
        // CD tracks are represented in the playlist as URLs of the form
        //   cdda:///dev/sr0?track=3
        // so they flow through the exact same string-based plumbing as files.
        static bool        isCddaPath(const std::string& path);
        static std::string makePath(const std::string& device, int track);
        static bool        parsePath(const std::string& path, std::string& device, int& track);

        // --- Disc discovery ----------------------------------------------
        // Optical drives currently visible to the system (device node paths).
        static std::vector<std::string> listDrives();
        // Audio tracks on the disc in the given drive. Empty on failure / no disc.
        static std::vector<CddaTrackInfo> readToc(const std::string& device);

        // Opens the drive and positions on the given 1-based track. On failure
        // the returned object's isValid() is false.
        CddaSource(const std::string& device, int track);
        ~CddaSource();

        bool isValid() const { return m_valid; }
        ma_data_source* dataSource() { return reinterpret_cast<ma_data_source*>(this); }

        // ma_data_source vtable callbacks (public so the static vtable can bind).
        ma_result onRead(void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
        ma_result onSeek(ma_uint64 frameIndex);
        ma_result onGetDataFormat(ma_format* f, ma_uint32* ch, ma_uint32* sr, ma_channel* map, size_t cap);
        ma_result onGetCursor(ma_uint64* pCursor);
        ma_result onGetLength(ma_uint64* pLength);

    private:
        // MUST be first: lets us reinterpret_cast between the two.
        ma_data_source_base m_base;

        void producerLoop();
        void stopProducer();

        cdrom_drive_t*    m_drive = nullptr;
        cdrom_paranoia_t* m_paranoia = nullptr;
        bool              m_valid = false;

        long m_firstSector = 0;
        long m_lastSector  = 0;
        ma_uint64 m_totalFrames = 0; // frames in the whole track (per channel pair)

        // Ring buffer of interleaved stereo s16 samples, measured in frames.
        std::vector<int16_t> m_ring;   // size = m_capFrames * 2
        size_t m_capFrames = 0;
        size_t m_head = 0;             // consumer read position (frames)
        size_t m_tail = 0;             // producer write position (frames)
        size_t m_avail = 0;            // frames currently buffered

        std::mutex              m_mutex;
        std::condition_variable m_cvProducer; // producer waits for free space
        std::condition_variable m_cvConsumer; // consumer waits for data

        std::thread       m_producer;
        std::atomic<bool> m_stop{false};
        bool              m_eof = false;

        ma_uint64 m_cursor = 0;        // frames delivered to the consumer (track-relative)

        bool      m_seekRequested = false;
        ma_uint64 m_seekTarget = 0;    // frame index to seek to
    };

} // namespace Kalorite

#else // !KALORITE_HAVE_CDDA

namespace Kalorite {
    struct CddaTrackInfo {
        int number = 0; long firstSector = 0; long lastSector = 0;
        long long lengthMs = 0; bool audio = true;
    };
    // Stubs so callers compile on platforms without libcdio.
    class CddaSource {
    public:
        static bool isCddaPath(const std::string&) { return false; }
        static std::string makePath(const std::string&, int) { return {}; }
        static bool parsePath(const std::string&, std::string&, int&) { return false; }
        static std::vector<std::string> listDrives() { return {}; }
        static std::vector<CddaTrackInfo> readToc(const std::string&) { return {}; }
    };
}

#endif // KALORITE_HAVE_CDDA
