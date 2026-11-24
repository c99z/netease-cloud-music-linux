#pragma once

#include <QHash>
#include <QList>
#include <QPair>
#include <QObject>
#include <QTcpServer>
#include <QUrl>

#include <memory>

class QTcpSocket;
class QTemporaryFile;

class AudioStream final : public QObject
{
    Q_OBJECT

public:
    AudioStream(std::shared_ptr<QTemporaryFile> file, qint64 length, const QString &extension);
    ~AudioStream() override;

    QUrl url(qint64 offset = 0) const;
    qint64 offsetForTime(qint64 milliseconds) const;
    bool isComplete() const { return m_complete; }
    qint64 size() const { return m_length; }
    const QList<QPair<qint64, qint64>> &bufferedByteRanges() const { return m_ranges; }
    qint64 bufferedEnd(qint64 position) const;
    bool needsDataAt(qint64 position) const;
    void publish(qint64 first, qint64 end);
    void finish(bool complete);

signals:
    void completed();
    void bufferedBytesChanged();
    void dataNeeded(qint64 position);

private:
    struct Reader;
    void acceptClients();
    void readRequest(QTcpSocket *socket);
    void pump(QTcpSocket *socket);

    std::shared_ptr<QTemporaryFile> m_file;
    QTcpServer m_server;
    QHash<QTcpSocket *, std::shared_ptr<Reader>> m_readers;
    QByteArray m_path;
    qint64 m_length;
    QList<QPair<qint64, qint64>> m_ranges;
    bool m_finished = false;
    bool m_complete = false;
};
