#pragma once

#include <QImage>
#include <QString>

namespace Kalorite {

    class TrackMetadataWriter {
    public:
        // Caches metadata (title, artist) and embedded front cover image directly into the audio file.
        // Also saves a local cover.jpg in the track directory if possible.
        static bool write(const QString& filePath,
                          const QString& title,
                          const QString& artist,
                          const QImage& cover);
    };

} // namespace Kalorite
