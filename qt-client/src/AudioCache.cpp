#include "AudioCache.h"
#include "AudioStream.h"

#include <QCryptographicHash>
#include <QDir>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QPointer>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QTimer>

#include <utility>

namespace {
constexpr qint64 cacheLimit = 128 * 1024 * 1024;
constexpr int backupBitrate = 128000;
constexpr int maximumRetries = 2;
constexpr qint64 minInitialBuffer = 64 * 1024;
constexpr qint64 maxInitialBuffer = 256 * 1024;
constexpr qint64 nearbyBytes = 24 * 1024;
constexpr qint64 tailProbeBytes = 4096;

bool canStreamFromFront(const QString &extension)
{
    return extension == ".mp3" || extension == ".flac" || extension == ".aac"
        || extension == ".ogg" || extension == ".opus" || extension == ".wav";
}

bool usable(const QUrl &url)
{
    return (url.scheme() == "http" || url.scheme() == "https") && !url.host().isEmpty();
}
}

struct AudioCache::Transfer
{
    Transfer(QString trackId, int bitrate) : id(std::move(trackId)), requestedBitrate(bitrate) {}

    QString id;
    int requestedBitrate;
    int bitrate = 0;
    int attempts = 0;
    Source source;
    Asset ready;
    Completion completion;
    std::shared_ptr<QTemporaryFile> file;
    QCryptographicHash checksum{QCryptographicHash::Md5};
    QPointer<QNetworkReply> reply;
    QPointer<QNetworkReply> tailReply;
    qint64 offset = 0;
    qint64 position = 0;
    int generation = 0;
    bool recovering = false;
    qint64 replaySkip = 0;
    bool progressive = false;
    std::shared_ptr<AudioStream> stream;
    bool acceptedHeaders = false;
    bool invalidResponse = false;
    bool cannotCache = false;
};

AudioCache::AudioCache(Resolve resolve) : m_resolver(std::move(resolve))
{
    m_connections.setProxy(QNetworkProxy::NoProxy);
}

AudioCache::~AudioCache()
{
    clearPending();
    releaseStream();
}

bool AudioCache::owns(const Job &job) const
{
    return job == m_current || job == m_next || job == m_feeding;
}

void AudioCache::cancel(Job &slot)
{
    auto job = std::exchange(slot, {});
    if (!job)
        return;
    abortReply(job);
    if (auto *tail = job->tailReply.data()) {
        tail->disconnect(this);
        tail->abort();
        tail->deleteLater();
    }
    job->completion = {};
    if (job->stream)
        job->stream->finish(false);
    job->file.reset();
}

void AudioCache::abortReply(const Job &job)
{
    if (auto *reply = job->reply.data()) {
        job->reply.clear();
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}

void AudioCache::clearPending()
{
    cancel(m_current);
    cancel(m_next);
}

void AudioCache::releaseStream()
{
    cancel(m_feeding);
}

void AudioCache::prepare(const QString &id, int bitrate, bool progressive, Completion completion)
{
    cancel(m_current);
    if (m_next && m_next->id == id && m_next->requestedBitrate == bitrate) {
        m_current = std::exchange(m_next, {});
        auto job = m_current;
        job->progressive = progressive;
        job->completion = std::move(completion);
        if (job->ready.file)
            finish(job, std::move(job->ready));
        else
            publishAudio(job);
        return;
    }

    cancel(m_next);
    auto job = std::make_shared<Transfer>(id, bitrate);
    job->completion = std::move(completion);
    job->progressive = progressive;
    m_current = job;
    resolve(job);
}

void AudioCache::prefetch(const QString &id, int bitrate)
{
    if (m_current || m_feeding || id.isEmpty() || (m_next && m_next->id == id && m_next->requestedBitrate == bitrate))
        return;
    cancel(m_next);
    m_next = std::make_shared<Transfer>(id, bitrate);
    resolve(m_next);
}

void AudioCache::resolve(const Job &job)
{
    job->bitrate = !job->stream && job->attempts >= maximumRetries
        ? qMin(job->requestedBitrate, backupBitrate) : job->requestedBitrate;
    const QPointer<AudioCache> cache(this);
    const int generation = job->generation;
    m_resolver(job->id, job->bitrate, [cache, job, generation](Source source, const QString &error) {
        if (!cache || !cache->owns(job) || generation != job->generation)
            return;
        if (!error.isEmpty()) {
            cache->retry(job, error, true);
        } else if (!usable(source.url)) {
            if (job->stream)
                cache->retry(job, tr("音频续传地址暂不可用。"), true);
            else
                cache->finish(job, {});
        } else {
            cache->download(job, std::move(source));
        }
    });
}

void AudioCache::download(const Job &job, Source source, qint64 offset)
{
    job->recovering = false;
    const qint64 destination = offset >= 0 ? offset
        : job->stream ? job->position : job->file ? job->file->size() : 0;
    // Only join byte ranges when the API identifies the exact same encoded file.
    const bool sameBytes = job->file && !source.md5.isEmpty() && source.md5 == job->source.md5
        && source.size > 0 && source.size == job->source.size
        && source.preview == job->source.preview && destination < source.size;
    if (job->stream && !sameBytes) {
        retry(job, tr("续传地址返回了不同的音频数据。"), true);
        return;
    }
    if (source.size > cacheLimit) {
        finish(job, {source.url, {}, {}, job->bitrate, source.preview});
        return;
    }

    if (!sameBytes) {
        const auto pattern = QDir::temp().filePath(QStringLiteral("record-room-audio-XXXXXX") + source.extension);
        job->file = std::make_shared<QTemporaryFile>(pattern);
        job->checksum.reset();
        if (!job->file->open()) {
            finish(job, {source.url, {}, {}, job->bitrate, source.preview});
            return;
        }
    }
    job->source = std::move(source);
    job->offset = sameBytes ? destination : 0;
    job->position = job->offset;
    if (!job->file->seek(job->position)) {
        finish(job, {job->source.url, {}, {}, job->bitrate, job->source.preview});
        return;
    }
    job->replaySkip = 0;
    job->acceptedHeaders = false;
    job->invalidResponse = false;
    job->cannotCache = false;

    QNetworkRequest request(job->source.url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setRawHeader("Accept-Encoding", "identity");
    request.setTransferTimeout(12000);
    request.setPriority(job == m_next ? QNetworkRequest::LowPriority : QNetworkRequest::NormalPriority);
    if (job->offset)
        request.setRawHeader("Range", "bytes=" + QByteArray::number(job->offset) + "-");

    auto *reply = m_connections.get(request);
    job->reply = reply;
    reply->setReadBufferSize(512 * 1024);
    connect(reply, &QNetworkReply::readyRead, this, [this, job, reply] {
        readData(job, reply);
        if (owns(job) && job->reply == reply && (job->invalidResponse || job->cannotCache))
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, job, reply] { downloadFinished(job, reply); });
}

void AudioCache::requestRange(const Job &job, qint64 position)
{
    if (!owns(job) || !job->stream->needsDataAt(position))
        return;

    // The decoder can request data just ahead of the active transfer.
    // Only abandon the transfer for a genuinely different read position.
    if ((job->reply || job->recovering) && position >= job->position
        && position - job->position <= nearbyBytes)
        return;

    if ((job->reply || job->recovering) && position >= job->source.size - tailProbeBytes) {
        if (!job->tailReply)
            requestTail(job, position);
        return;
    }

    abortReply(job);
    ++job->generation;
    job->attempts = 0;
    download(job, job->source, position);
}

void AudioCache::requestTail(const Job &job, qint64 position)
{
    QNetworkRequest request(job->source.url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setRawHeader("Accept-Encoding", "identity");
    request.setRawHeader("Range", "bytes=" + QByteArray::number(position) + "-");
    request.setTransferTimeout(5000);
    auto *reply = m_connections.get(request);
    job->tailReply = reply;
    reply->setReadBufferSize(tailProbeBytes + 1);
    connect(reply, &QNetworkReply::readyRead, this, [reply] {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 206
            || reply->bytesAvailable() > tailProbeBytes)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, job, reply, position] {
        if (job->tailReply != reply) {
            reply->deleteLater();
            return;
        }
        job->tailReply.clear();
        const auto expectedRange = "bytes " + QByteArray::number(position) + "-"
            + QByteArray::number(job->source.size - 1) + "/" + QByteArray::number(job->source.size);
        const bool valid = reply->error() == QNetworkReply::NoError
            && reply->rawHeader("Content-Range") == expectedRange;
        const auto bytes = reply->readAll();
        reply->deleteLater();
        if (!owns(job) || !valid || bytes.size() != job->source.size - position)
            return;
        if (!job->file->seek(position) || job->file->write(bytes) != bytes.size()
            || !job->file->flush() || !job->file->seek(job->position))
            return;
        job->stream->publish(position, job->source.size);
    });
}

void AudioCache::readData(const Job &job, QNetworkReply *reply)
{
    if (!owns(job) || job->reply != reply || job->invalidResponse || job->cannotCache)
        return;

    if (!job->acceptedHeaders) {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200) {
            if (job->offset && job->stream) {
                // An active reader must keep its prefix if this origin ignores Range.
                job->replaySkip = job->offset;
            } else if (job->offset) {
                if (!job->file->resize(0) || !job->file->seek(0)) {
                    job->cannotCache = true;
                    return;
                }
                job->checksum.reset();
                job->position = 0;
            }
            job->offset = 0;
        } else if (status == 206) {
            static const QRegularExpression range(QStringLiteral("^bytes (\\d+)-(\\d+)/(\\d+)$"));
            const auto match = range.match(QString::fromLatin1(reply->rawHeader("Content-Range")));
            const auto last = match.captured(2).toLongLong();
            const auto total = match.captured(3).toLongLong();
            if (!match.hasMatch() || match.captured(1).toLongLong() != job->offset
                || last < job->offset || last >= total
                || (job->source.size && total != job->source.size)) {
                job->invalidResponse = true;
                return;
            }
            if (total > cacheLimit) {
                job->cannotCache = true;
                return;
            }
            job->source.size = total;
        } else {
            job->invalidResponse = true;
            return;
        }
        const auto advertised = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        if (advertised > cacheLimit - job->offset) {
            job->cannotCache = true;
            return;
        }
        job->acceptedHeaders = true;
    }

    auto data = reply->readAll();
    if (job->replaySkip) {
        const auto skip = qMin(job->replaySkip, qint64(data.size()));
        data.remove(0, skip);
        job->replaySkip -= skip;
    }
    if (data.isEmpty())
        return;
    if (data.size() > cacheLimit - job->position) {
        job->cannotCache = true;
    } else if (job->source.size && data.size() > job->source.size - job->position) {
        job->invalidResponse = true;
    } else if (job->file->write(data) != data.size()) {
        job->cannotCache = true;
    } else {
        const qint64 start = job->position;
        job->position += data.size();
        if (job->stream) {
            if (!job->file->flush()) {
                job->cannotCache = true;
                return;
            }
            // Keep the terminal byte pending until the response has been checked.
            job->stream->publish(start, qMin(job->position, job->source.size - 1));
        } else {
            if (!job->source.md5.isEmpty())
                job->checksum.addData(data);
            publishAudio(job);
        }
    }
}

void AudioCache::publishAudio(const Job &job)
{
    if (!job->file || !job->acceptedHeaders || job->stream)
        return;
    const auto size = job->position;
    const auto initialBuffer = qBound(minInitialBuffer, qint64(job->bitrate) / 2, maxInitialBuffer);
    if (job != m_current || !job->progressive || job->source.md5.isEmpty()
        || !canStreamFromFront(job->source.extension) || size < initialBuffer || size >= job->source.size)
        return;
    if (!job->file->flush()) {
        job->cannotCache = true;
        return;
    }
    auto stream = std::make_shared<AudioStream>(job->file, job->source.size, job->source.extension);
    const auto url = stream->url();
    if (!url.isValid())
        return;
    releaseStream();
    job->stream = std::move(stream);
    // The signal originates while one of the local HTTP readers is pumping data.
    // Defer network changes until that read has finished.
    const std::weak_ptr<Transfer> weakJob = job;
    connect(job->stream.get(), &AudioStream::dataNeeded, this, [this, weakJob](qint64 position) {
        if (const auto active = weakJob.lock())
            requestRange(active, position);
    }, Qt::QueuedConnection);
    job->stream->publish(0, size);
    auto completion = std::move(job->completion);
    m_feeding = job;
    m_current.reset();
    if (completion)
        completion({url, job->file, job->source.md5, job->bitrate, job->source.preview, job->stream}, {});
}

void AudioCache::downloadFinished(const Job &job, QNetworkReply *reply)
{
    if (!owns(job) || job->reply != reply) {
        reply->deleteLater();
        return;
    }
    readData(job, reply);
    job->reply.clear();
    const auto networkError = reply->error();
    reply->deleteLater();

    if (job->cannotCache) {
        finish(job, {job->source.url, {}, {}, job->bitrate, job->source.preview});
        return;
    }
    const auto size = job->position;
    const bool complete = networkError == QNetworkReply::NoError && job->acceptedHeaders
        && !job->invalidResponse && size > 0 && (job->source.size == 0 || size == job->source.size);
    if (!complete) {
        retry(job, tr("音频下载中断，可点播放重试。"), !job->invalidResponse || bool(job->stream));
        return;
    }
    if (job->stream) {
        if (job->stream->bufferedEnd(0) < job->source.size - 1) {
            // A seek can complete the tail while earlier data remains sparse.
            // Keep the relay available for subsequent range requests.
            job->stream->publish(job->source.size - 1, job->source.size);
            return;
        }
        job->checksum.reset();
        if (!job->file->flush() || !job->file->seek(0) || !job->checksum.addData(job->file.get())) {
            finish(job, {});
            return;
        }
    }
    if (!job->source.md5.isEmpty() && job->checksum.result().toHex() != job->source.md5) {
        retry(job, tr("音频数据校验失败，可切换音源重试。"), false);
        return;
    }
    if (!job->file->flush()) {
        finish(job, {job->source.url, {}, {}, job->bitrate, job->source.preview});
        return;
    }
    const auto url = QUrl::fromLocalFile(job->file->fileName());
    job->file->close();
    finish(job, {url, std::move(job->file), job->source.md5, job->bitrate, job->source.preview});
}

void AudioCache::retry(const Job &job, const QString &error, bool resumable)
{
    if (!resumable && job->stream) {
        finish(job, {}, error);
        return;
    }
    if (!resumable)
        job->file.reset();
    if (job->attempts >= maximumRetries) {
        finish(job, {}, error);
        return;
    }
    job->recovering = true;
    const auto delay = ++job->attempts == 1 ? 180 : 500;
    const int generation = job->generation;
    QTimer::singleShot(delay, this, [this, job, generation] {
        if (owns(job) && job->generation == generation)
            resolve(job);
    });
}

void AudioCache::finish(const Job &job, Asset asset, const QString &error)
{
    if (job == m_feeding) {
        m_feeding.reset();
        job->stream->finish(bool(asset.file));
    } else if (job == m_current) {
        auto completion = std::move(job->completion);
        m_current.reset();
        if (completion)
            completion(std::move(asset), error);
    } else if (job == m_next) {
        if (asset.file)
            job->ready = std::move(asset);
        else
            m_next.reset();
    }
}
