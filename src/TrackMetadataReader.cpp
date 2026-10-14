#include "TrackMetadataReader.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDataStream>

#ifdef KALORITE_HAVE_TAGLIB
#if __has_include(<taglib/fileref.h>)
#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/vorbisfile.h>
#include <taglib/xiphcomment.h>
#else
#include <tag.h>
#include <fileref.h>
#include <mpegfile.h>
#include <id3v2tag.h>
#include <id3v2frame.h>
#include <attachedpictureframe.h>
#include <flacfile.h>
#include <flacpicture.h>
#include <mp4file.h>
#include <mp4tag.h>
#include <mp4coverart.h>
#include <vorbisfile.h>
#include <xiphcomment.h>
#endif
#endif

namespace Kalorite {

TrackMetadata TrackMetadataReader::read(const QString& filePath) {
    TrackMetadata meta;
    QFileInfo fileInfo(filePath);

#ifdef KALORITE_HAVE_TAGLIB
#ifdef Q_OS_WIN
    const wchar_t* pathStr = reinterpret_cast<const wchar_t*>(filePath.utf16());
    TagLib::FileRef f(pathStr);
#else
    QByteArray localPath = filePath.toLocal8Bit();
    const char* pathStr = localPath.constData();
    TagLib::FileRef f(pathStr);
#endif

    if (!f.isNull() && f.tag()) {
        meta.title = QString::fromUtf8(f.tag()->title().toCString(true)).trimmed();
        meta.artist = QString::fromUtf8(f.tag()->artist().toCString(true)).trimmed();
        meta.album = QString::fromUtf8(f.tag()->album().toCString(true)).trimmed();
    }

    const QString ext = fileInfo.suffix().toLower();
    if (ext == QStringLiteral("mp3")) {
        TagLib::MPEG::File mpegFile(pathStr);
        if (mpegFile.isValid() && mpegFile.ID3v2Tag()) {
            const auto& frameMap = mpegFile.ID3v2Tag()->frameListMap();
            if (frameMap.contains("APIC") && !frameMap["APIC"].isEmpty()) {
                auto* picFrame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frameMap["APIC"].front());
                if (picFrame && picFrame->picture().size() > 0) {
                    if (meta.cover.loadFromData(reinterpret_cast<const uchar*>(picFrame->picture().data()),
                                                static_cast<int>(picFrame->picture().size()))) {
                        meta.hasCover = true;
                    }
                }
            }
        }
    } else if (ext == QStringLiteral("flac")) {
        TagLib::FLAC::File flacFile(pathStr);
        if (flacFile.isValid()) {
            const auto& pictures = flacFile.pictureList();
            if (!pictures.isEmpty() && pictures.front()->data().size() > 0) {
                auto* pic = pictures.front();
                if (meta.cover.loadFromData(reinterpret_cast<const uchar*>(pic->data().data()),
                                            static_cast<int>(pic->data().size()))) {
                    meta.hasCover = true;
                }
            }
        }
    } else if (ext == QStringLiteral("m4a") || ext == QStringLiteral("mp4") || ext == QStringLiteral("aac")) {
        TagLib::MP4::File mp4File(pathStr);
        if (mp4File.isValid() && mp4File.tag()) {
            auto itemListMap = mp4File.tag()->itemListMap();
            if (itemListMap.contains("covr")) {
                auto coverList = itemListMap["covr"].toCoverArtList();
                if (!coverList.isEmpty() && coverList.front().data().size() > 0) {
                    if (meta.cover.loadFromData(reinterpret_cast<const uchar*>(coverList.front().data().data()),
                                                static_cast<int>(coverList.front().data().size()))) {
                        meta.hasCover = true;
                    }
                }
            }
        }
    } else if (ext == QStringLiteral("ogg") || ext == QStringLiteral("opus")) {
        TagLib::Ogg::Vorbis::File vorbisFile(pathStr);
        TagLib::Ogg::XiphComment* comment = vorbisFile.isValid() ? vorbisFile.tag() : nullptr;
        if (comment) {
            const auto& fields = comment->fieldListMap();
            if (fields.contains("METADATA_BLOCK_PICTURE") && !fields["METADATA_BLOCK_PICTURE"].isEmpty()) {
                QByteArray b64 = QByteArray::fromStdString(fields["METADATA_BLOCK_PICTURE"].front().to8Bit(true));
                QByteArray raw = QByteArray::fromBase64(b64);
                if (raw.size() > 32) {
                    QDataStream ds(raw);
                    ds.setByteOrder(QDataStream::BigEndian);
                    quint32 type = 0;
                    quint32 mimeLen = 0;
                    ds >> type >> mimeLen;
                    if (mimeLen < static_cast<quint32>(raw.size() - 8)) {
                        ds.skipRawData(static_cast<int>(mimeLen));
                        quint32 descLen = 0;
                        ds >> descLen;
                        if (descLen < static_cast<quint32>(raw.size())) {
                            ds.skipRawData(static_cast<int>(descLen));
                            ds.skipRawData(16); // width, height, color_depth, colors_indexed
                            quint32 picLen = 0;
                            ds >> picLen;
                            if (picLen > 0 && ds.device()->bytesAvailable() >= picLen) {
                                QByteArray picData = ds.device()->read(picLen);
                                if (meta.cover.loadFromData(picData)) {
                                    meta.hasCover = true;
                                }
                            }
                        }
                    }
                }
            } else if (fields.contains("COVERART") && !fields["COVERART"].isEmpty()) {
                QByteArray b64 = QByteArray::fromStdString(fields["COVERART"].front().to8Bit(true));
                QByteArray raw = QByteArray::fromBase64(b64);
                if (meta.cover.loadFromData(raw)) {
                    meta.hasCover = true;
                }
            }
        }
    }
#endif

    // Fallback for title and artist if missing
    if (meta.title.isEmpty()) {
        QString baseName = fileInfo.completeBaseName();
        int dashIdx = baseName.indexOf(QStringLiteral(" - "));
        if (dashIdx > 0) {
            if (meta.artist.isEmpty()) {
                meta.artist = baseName.left(dashIdx).trimmed();
            }
            meta.title = baseName.mid(dashIdx + 3).trimmed();
        } else {
            meta.title = baseName;
        }
    }

    // Fallback: check track directory for cover image files
    if (!meta.hasCover) {
        meta.hasCover = findFolderCover(filePath, meta.cover);
    }

    return meta;
}

bool TrackMetadataReader::findFolderCover(const QString& filePath, QImage& outCover) {
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) return false;

    const QStringList candidates = {
        QStringLiteral("cover.jpg"), QStringLiteral("cover.png"), QStringLiteral("cover.jpeg"),
        QStringLiteral("folder.jpg"), QStringLiteral("folder.png"), QStringLiteral("folder.jpeg"),
        QStringLiteral("front.jpg"), QStringLiteral("front.png"), QStringLiteral("front.jpeg"),
        QStringLiteral("album.jpg"), QStringLiteral("album.png"), QStringLiteral("album.jpeg"),
        fileInfo.completeBaseName() + QStringLiteral(".jpg"),
        fileInfo.completeBaseName() + QStringLiteral(".png")
    };

    for (const QString& name : candidates) {
        QString candPath = dir.filePath(name);
        if (QFile::exists(candPath)) {
            if (outCover.load(candPath)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace Kalorite
