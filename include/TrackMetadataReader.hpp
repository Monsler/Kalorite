#pragma once

#include <QImage>
#include <QString>

namespace Kalorite {

    struct TrackMetadata {
        QString title;
        QString artist;
        QString album;
        QImage cover;
        bool hasCover = false;
    };

    class TrackMetadataReader {
    public:
        static TrackMetadata read(const QString& filePath);

    private:
        static bool findFolderCover(const QString& filePath, QImage& outCover);
    };

} // namespace Kalorite
