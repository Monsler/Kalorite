#pragma once

#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace Kalorite {

    struct MetadataFetchJob {
        quint64 trackId = 0;
        QString currentTitle;
        QString currentArtist;
        QString filePath;
        bool needCover = true;
        bool needTags = true;
    };

    // Queries MusicBrainz API + Cover Art Archive, with automatic fallback
    // to iTunes Search API if MusicBrainz is unavailable (e.g. 503) or missing cover.
    class MusicBrainzService : public QObject {
        Q_OBJECT

    public:
        explicit MusicBrainzService(QObject* parent = nullptr);
        ~MusicBrainzService() override;

        void enqueue(quint64 trackId,
                     const QString& title,
                     const QString& artist,
                     const QString& filePath,
                     bool needCover,
                     bool needTags);

    signals:
        void metadataFetched(quint64 trackId,
                             const QString& title,
                             const QString& artist,
                             const QImage& cover,
                             bool hasCover);

    private slots:
        void processQueue();

    private:
        void startMusicBrainzQuery(const MetadataFetchJob& job);
        void handleMusicBrainzReply(QNetworkReply* reply, MetadataFetchJob job);
        void fetchCoverArt(const QStringList& releaseMbids,
                           int mbidIndex,
                           MetadataFetchJob job,
                           const QString& fetchedTitle,
                           const QString& fetchedArtist);
        void fallbackToITunes(const MetadataFetchJob& job);
        void scheduleNext();

        void extractSearchTerms(const MetadataFetchJob& job,
                                QString& outTitle,
                                QString& outArtist) const;

        QNetworkAccessManager* m_net;
        QQueue<MetadataFetchJob> m_queue;
        QTimer* m_rateTimer;
        bool m_isBusy = false;
    };

} // namespace Kalorite
