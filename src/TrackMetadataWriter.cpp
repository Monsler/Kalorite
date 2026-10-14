#include "TrackMetadataWriter.hpp"
#include <QBuffer>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

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

bool TrackMetadataWriter::write(const QString& filePath,
                                const QString& title,
                                const QString& artist,
                                const QImage& cover) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return false;
    }

    qDebug() << "[MetadataWriter] Caching metadata into file:" << filePath;
    bool writtenToFile = false;

#ifdef KALORITE_HAVE_TAGLIB
    QByteArray localPath = filePath.toLocal8Bit();
    const char* pathStr = localPath.constData();
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    QByteArray jpegData;
    if (!cover.isNull()) {
        QBuffer buf(&jpegData);
        buf.open(QIODevice::WriteOnly);
        cover.save(&buf, "JPEG", 90);
    }

    if (ext == QStringLiteral("mp3")) {
        TagLib::MPEG::File mpegFile(pathStr);
        if (mpegFile.isValid()) {
            TagLib::ID3v2::Tag* tag = mpegFile.ID3v2Tag(true);
            if (tag) {
                if (!title.isEmpty()) {
                    tag->setTitle(TagLib::String(title.toUtf8().constData(), TagLib::String::UTF8));
                }
                if (!artist.isEmpty()) {
                    tag->setArtist(TagLib::String(artist.toUtf8().constData(), TagLib::String::UTF8));
                }
                if (!jpegData.isEmpty()) {
                    auto frames = tag->frameList("APIC");
                    for (auto* f : frames) {
                        tag->removeFrame(f);
                    }
                    auto* picFrame = new TagLib::ID3v2::AttachedPictureFrame();
                    picFrame->setMimeType("image/jpeg");
                    picFrame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
                    picFrame->setPicture(TagLib::ByteVector(jpegData.constData(), static_cast<size_t>(jpegData.size())));
                    tag->addFrame(picFrame);
                }
                writtenToFile = mpegFile.save();
            }
        }
    } else if (ext == QStringLiteral("flac")) {
        TagLib::FLAC::File flacFile(pathStr);
        if (flacFile.isValid()) {
            if (flacFile.tag()) {
                if (!title.isEmpty()) {
                    flacFile.tag()->setTitle(TagLib::String(title.toUtf8().constData(), TagLib::String::UTF8));
                }
                if (!artist.isEmpty()) {
                    flacFile.tag()->setArtist(TagLib::String(artist.toUtf8().constData(), TagLib::String::UTF8));
                }
            }
            if (!jpegData.isEmpty()) {
                flacFile.removePictures();
                auto* pic = new TagLib::FLAC::Picture();
                pic->setData(TagLib::ByteVector(jpegData.constData(), static_cast<size_t>(jpegData.size())));
                pic->setType(TagLib::FLAC::Picture::FrontCover);
                pic->setMimeType("image/jpeg");
                flacFile.addPicture(pic);
            }
            writtenToFile = flacFile.save();
        }
    } else if (ext == QStringLiteral("m4a") || ext == QStringLiteral("mp4") || ext == QStringLiteral("aac")) {
        TagLib::MP4::File mp4File(pathStr);
        if (mp4File.isValid() && mp4File.tag()) {
            if (!title.isEmpty()) {
                mp4File.tag()->setTitle(TagLib::String(title.toUtf8().constData(), TagLib::String::UTF8));
            }
            if (!artist.isEmpty()) {
                mp4File.tag()->setArtist(TagLib::String(artist.toUtf8().constData(), TagLib::String::UTF8));
            }
            if (!jpegData.isEmpty()) {
                TagLib::MP4::CoverArt art(TagLib::MP4::CoverArt::JPEG,
                                          TagLib::ByteVector(jpegData.constData(), static_cast<size_t>(jpegData.size())));
                TagLib::MP4::CoverArtList list;
                list.append(art);
                mp4File.tag()->setItem("covr", TagLib::MP4::Item(list));
            }
            writtenToFile = mp4File.save();
        }
    } else if (ext == QStringLiteral("ogg") || ext == QStringLiteral("opus")) {
        TagLib::Ogg::Vorbis::File vorbisFile(pathStr);
        if (vorbisFile.isValid() && vorbisFile.tag()) {
            if (!title.isEmpty()) {
                vorbisFile.tag()->setTitle(TagLib::String(title.toUtf8().constData(), TagLib::String::UTF8));
            }
            if (!artist.isEmpty()) {
                vorbisFile.tag()->setArtist(TagLib::String(artist.toUtf8().constData(), TagLib::String::UTF8));
            }
            if (!jpegData.isEmpty()) {
                // Construct FLAC Picture block for METADATA_BLOCK_PICTURE
                QByteArray picBlock;
                QDataStream ds(&picBlock, QIODevice::WriteOnly);
                ds.setByteOrder(QDataStream::BigEndian);
                ds << static_cast<quint32>(3); // FrontCover
                QByteArray mime = "image/jpeg";
                ds << static_cast<quint32>(mime.size());
                ds.writeRawData(mime.constData(), mime.size());
                ds << static_cast<quint32>(0); // description length
                ds << static_cast<quint32>(cover.width());
                ds << static_cast<quint32>(cover.height());
                ds << static_cast<quint32>(24); // color depth
                ds << static_cast<quint32>(0);  // colors indexed
                ds << static_cast<quint32>(jpegData.size());
                ds.writeRawData(jpegData.constData(), jpegData.size());

                vorbisFile.tag()->addField("METADATA_BLOCK_PICTURE",
                                           TagLib::String(picBlock.toBase64().constData(), TagLib::String::UTF8));
            }
            writtenToFile = vorbisFile.save();
        }
    }
#endif

    if (writtenToFile) {
        qDebug() << "[MetadataWriter] Successfully embedded tags and cover into:" << filePath;
    } else {
        qDebug() << "[MetadataWriter] Could not write embedded tags into file (permissions or format).";
    }

    // Save fallback cover.jpg next to the audio file if directory is writable
    if (!cover.isNull()) {
        QFileInfo fi(filePath);
        QDir dir = fi.dir();
        QString coverPath = dir.filePath(QStringLiteral("cover.jpg"));
        if (!QFile::exists(coverPath)) {
            if (cover.save(coverPath, "JPEG", 90)) {
                qDebug() << "[MetadataWriter] Saved fallback cover to folder:" << coverPath;
            }
        }
    }

    return writtenToFile;
}

} // namespace Kalorite
