#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>
#include <memory>

class AudioStream;
class QNetworkReply;
class QTemporaryFile;

class AudioCache final : public QObject
{
public:
    struct Source
    {
        QUrl url;
        QByteArray md5;
        qint64 size = 0;
        QString extension;
        bool preview = false;
    };

    struct Asset
    {
        QUrl url;
        std::shared_ptr<QTemporaryFile> file;
        QByteArray md5;
        int requestedBitrate = 0;
        bool preview = false;
        std::shared_ptr<AudioStream> stream;
    };

    using SourceReady = std::function<void(Source, QString)>;
    using Resolve = std::function<void(const QString &, int, SourceReady)>;
    using Completion = std::function<void(Asset, QString)>;

    explicit AudioCache(Resolve resolve);
    ~AudioCache() override;

    void prepare(const QString &id, int bitrate, bool progressive, Completion completion);
    void prefetch(const QString &id, int bitrate);
    void clearPending();
    void releaseStream();
    bool preparing() const { return bool(m_current); }

private:
    struct Transfer;
    using Job = std::shared_ptr<Transfer>;

    bool owns(const Job &job) const;
    void cancel(Job &slot);
    void resolve(const Job &job);
    void download(const Job &job, Source source, qint64 offset = -1);
    void requestRange(const Job &job, qint64 position);
    void requestTail(const Job &job, qint64 position);
    void abortReply(const Job &job);
    void readData(const Job &job, QNetworkReply *reply);
    void publishAudio(const Job &job);
    void downloadFinished(const Job &job, QNetworkReply *reply);
    void retry(const Job &job, const QString &error, bool resumable);
    void finish(const Job &job, Asset asset, const QString &error = {});

    QNetworkAccessManager m_connections;
    Resolve m_resolver;
    Job m_current;
    Job m_next;
    Job m_feeding;
};
