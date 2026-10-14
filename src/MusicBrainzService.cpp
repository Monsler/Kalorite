#include "MusicBrainzService.hpp"
#include "TrackMetadataWriter.hpp"
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace Kalorite {

static const QString kUserAgent = QStringLiteral("Kalorite/0.1 ( https://github.com/Monsler/Kalorite )");

MusicBrainzService::MusicBrainzService(QObject* parent)
    : QObject(parent),
      m_net(new QNetworkAccessManager(this)),
      m_rateTimer(new QTimer(this)) {
    m_rateTimer->setSingleShot(true);
    connect(m_rateTimer, &QTimer::timeout, this, &MusicBrainzService::processQueue);
}

MusicBrainzService::~MusicBrainzService() {
    m_queue.clear();
}

void MusicBrainzService::enqueue(quint64 trackId,
                                 const QString& title,
                                 const QString& artist,
                                 const QString& filePath,
                                 bool needCover,
                                 bool needTags) {
    if (trackId == 0) return;
    if (!needCover && !needTags) return;

    qDebug() << "[MusicBrainz] Enqueued search: id=" << trackId
             << "title=" << title << "artist=" << artist
             << "needCover=" << needCover << "needTags=" << needTags;

    m_queue.enqueue(MetadataFetchJob{ trackId, title, artist, filePath, needCover, needTags });

    if (!m_isBusy && !m_rateTimer->isActive()) {
        m_rateTimer->start(50);
    }
}

void MusicBrainzService::processQueue() {
    if (m_queue.isEmpty() || m_isBusy) return;

    m_isBusy = true;
    MetadataFetchJob job = m_queue.dequeue();
    startMusicBrainzQuery(job);
}

void MusicBrainzService::scheduleNext() {
    m_isBusy = false;
    if (!m_queue.isEmpty()) {
        m_rateTimer->start(1000);
    }
}

void MusicBrainzService::extractSearchTerms(const MetadataFetchJob& job,
                                           QString& outTitle,
                                           QString& outArtist) const {
    outTitle = job.currentTitle;
    outArtist = job.currentArtist;

    QFileInfo fi(job.filePath);
    QString baseName = fi.completeBaseName();

    if (outTitle.isEmpty() || outTitle == baseName) {
        baseName.remove(QRegularExpression(QStringLiteral("^[0-9]+[\\s\\.\\-_]+")));
        baseName.remove(QRegularExpression(
            QStringLiteral("\\s*[\\[\\(].*?(official|video|audio|lyrics|remaster|hd|hq|live|remix).*?[\\]\\)]"),
            QRegularExpression::CaseInsensitiveOption));

        int dash = baseName.indexOf(QStringLiteral(" - "));
        if (dash > 0) {
            if (outArtist.isEmpty()) {
                outArtist = baseName.left(dash).trimmed();
            }
            outTitle = baseName.mid(dash + 3).trimmed();
        } else {
            outTitle = baseName.trimmed();
        }
    }

    outTitle = outTitle.replace('"', ' ').trimmed();
    outArtist = outArtist.replace('"', ' ').trimmed();
}

void MusicBrainzService::startMusicBrainzQuery(const MetadataFetchJob& job) {
    QString qTitle, qArtist;
    extractSearchTerms(job, qTitle, qArtist);

    if (qTitle.isEmpty() && qArtist.isEmpty()) {
        qDebug() << "[MusicBrainz] Skipping: empty search terms for trackId=" << job.trackId;
        scheduleNext();
        return;
    }

    QUrl url(QStringLiteral("http://musicbrainz.org/ws/2/recording/"));
    QUrlQuery q;

    if (!qArtist.isEmpty() && !qTitle.isEmpty()) {
        q.addQueryItem(QStringLiteral("query"),
                       QStringLiteral("recording:\"%1\" AND artist:\"%2\"").arg(qTitle, qArtist));
    } else {
        q.addQueryItem(QStringLiteral("query"), qTitle);
    }
    q.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("5"));
    url.setQuery(q);

    qDebug() << "[MusicBrainz] Querying metadata:" << url.toString();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(10);
    request.setTransferTimeout(5000);

    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, job]() {
        handleMusicBrainzReply(reply, job);
    });
}

void MusicBrainzService::handleMusicBrainzReply(QNetworkReply* reply, MetadataFetchJob job) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[MusicBrainz] Network error:" << reply->errorString()
                   << "-> Falling back to iTunes Search API...";
        fallbackToITunes(job);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning() << "[MusicBrainz] Invalid JSON response -> Falling back to iTunes...";
        fallbackToITunes(job);
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray recordings = root[QStringLiteral("recordings")].toArray();
    if (recordings.isEmpty()) {
        qDebug() << "[MusicBrainz] No recordings found -> Falling back to iTunes...";
        fallbackToITunes(job);
        return;
    }

    QJsonObject rec = recordings.first().toObject();
    QString fetchedTitle = rec[QStringLiteral("title")].toString().trimmed();

    QString fetchedArtist;
    QJsonArray artists = rec[QStringLiteral("artist-credit")].toArray();
    if (!artists.isEmpty()) {
        fetchedArtist = artists.first().toObject()[QStringLiteral("name")].toString().trimmed();
    }

    QStringList releaseMbids;
    QJsonArray releases = rec[QStringLiteral("releases")].toArray();
    for (const QJsonValue& rVal : releases) {
        QString id = rVal.toObject()[QStringLiteral("id")].toString().trimmed();
        if (!id.isEmpty()) {
            releaseMbids.append(id);
        }
    }

    qDebug() << "[MusicBrainz] Match found:" << fetchedTitle << "-" << fetchedArtist
             << "| Releases count:" << releaseMbids.size();

    if (job.needCover && !releaseMbids.isEmpty()) {
        qDebug() << "[CoverArtArchive] Searching cover across" << releaseMbids.size() << "releases...";
        fetchCoverArt(releaseMbids, 0, job, fetchedTitle, fetchedArtist);
    } else {
        if (job.needTags && (!fetchedTitle.isEmpty() || !fetchedArtist.isEmpty())) {
            qDebug() << "[MusicBrainz] Updating track tags:" << fetchedTitle << "-" << fetchedArtist;
            TrackMetadataWriter::write(job.filePath, fetchedTitle, fetchedArtist, QImage());
            emit metadataFetched(job.trackId, fetchedTitle, fetchedArtist, QImage(), false);
        }
        scheduleNext();
    }
}

void MusicBrainzService::fetchCoverArt(const QStringList& releaseMbids,
                                       int mbidIndex,
                                       MetadataFetchJob job,
                                       const QString& fetchedTitle,
                                       const QString& fetchedArtist) {
    if (mbidIndex >= releaseMbids.size() || mbidIndex >= 5) {
        qDebug() << "[CoverArtArchive] Cover not found in archive -> Falling back to iTunes...";
        fallbackToITunes(job);
        return;
    }

    const QString mbid = releaseMbids.at(mbidIndex);
    QUrl coverUrl(QStringLiteral("https://coverartarchive.org/release/") + mbid + QStringLiteral("/front-250"));

    qDebug() << "[CoverArtArchive] Requesting cover [" << (mbidIndex + 1) << "]:" << coverUrl.toString();

    QNetworkRequest request(coverUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(10);
    request.setTransferTimeout(6000);

    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, releaseMbids, mbidIndex, job, fetchedTitle, fetchedArtist]() {
        reply->deleteLater();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray imgData = reply->readAll();
            QImage cover;
            if (cover.loadFromData(imgData)) {
                qDebug() << "[CoverArtArchive] Cover successfully fetched! Size:" << cover.size()
                         << "for trackId=" << job.trackId;
                // Cache cover and tags directly into the audio file
                TrackMetadataWriter::write(job.filePath, fetchedTitle, fetchedArtist, cover);
                emit metadataFetched(job.trackId, fetchedTitle, fetchedArtist, cover, true);
                scheduleNext();
                return;
            }
        }

        fetchCoverArt(releaseMbids, mbidIndex + 1, job, fetchedTitle, fetchedArtist);
    });
}

void MusicBrainzService::fallbackToITunes(const MetadataFetchJob& job) {
    QString qTitle, qArtist;
    extractSearchTerms(job, qTitle, qArtist);

    QString searchTerm = !qArtist.isEmpty() ? (qArtist + QStringLiteral(" ") + qTitle) : qTitle;
    if (searchTerm.trimmed().isEmpty()) {
        scheduleNext();
        return;
    }

    QUrl url(QStringLiteral("https://itunes.apple.com/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("term"), searchTerm);
    q.addQueryItem(QStringLiteral("entity"), QStringLiteral("song"));
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("1"));
    url.setQuery(q);

    qDebug() << "[iTunes] Fallback search URL:" << url.toString();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(10);
    request.setTransferTimeout(6000);

    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, job, qTitle, qArtist]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[iTunes] Fallback search error:" << reply->errorString();
            scheduleNext();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            scheduleNext();
            return;
        }

        QJsonArray results = doc.object()[QStringLiteral("results")].toArray();
        if (results.isEmpty()) {
            qDebug() << "[iTunes] No matches found for:" << job.filePath;
            scheduleNext();
            return;
        }

        QJsonObject item = results.first().toObject();
        QString itTitle = item[QStringLiteral("trackName")].toString().trimmed();
        QString itArtist = item[QStringLiteral("artistName")].toString().trimmed();
        QString artworkUrl = item[QStringLiteral("artworkUrl100")].toString().trimmed();

        artworkUrl.replace(QStringLiteral("100x100bb"), QStringLiteral("300x300bb"));

        QString finalTitle = (job.needTags && !itTitle.isEmpty()) ? itTitle : job.currentTitle;
        QString finalArtist = (job.needTags && !itArtist.isEmpty()) ? itArtist : job.currentArtist;

        if (job.needCover && !artworkUrl.isEmpty()) {
            qDebug() << "[iTunes] Fetching cover:" << artworkUrl;
            QNetworkRequest imgReq{ QUrl(artworkUrl) };
            imgReq.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
            imgReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            imgReq.setMaximumRedirectsAllowed(10);
            imgReq.setTransferTimeout(6000);

            QNetworkReply* imgReply = m_net->get(imgReq);
            connect(imgReply, &QNetworkReply::finished, this, [this, imgReply, job, finalTitle, finalArtist]() {
                imgReply->deleteLater();
                if (imgReply->error() == QNetworkReply::NoError) {
                    QImage cover;
                    if (cover.loadFromData(imgReply->readAll())) {
                        qDebug() << "[iTunes] Cover successfully fetched! Size:" << cover.size()
                                 << "for trackId=" << job.trackId;
                        // Cache cover and tags directly into the audio file
                        TrackMetadataWriter::write(job.filePath, finalTitle, finalArtist, cover);
                        emit metadataFetched(job.trackId, finalTitle, finalArtist, cover, true);
                        scheduleNext();
                        return;
                    }
                }
                if (job.needTags && (!finalTitle.isEmpty() || !finalArtist.isEmpty())) {
                    TrackMetadataWriter::write(job.filePath, finalTitle, finalArtist, QImage());
                    emit metadataFetched(job.trackId, finalTitle, finalArtist, QImage(), false);
                }
                scheduleNext();
            });
        } else {
            if (job.needTags && (!finalTitle.isEmpty() || !finalArtist.isEmpty())) {
                TrackMetadataWriter::write(job.filePath, finalTitle, finalArtist, QImage());
                emit metadataFetched(job.trackId, finalTitle, finalArtist, QImage(), false);
            }
            scheduleNext();
        }
    });
}

} // namespace Kalorite
