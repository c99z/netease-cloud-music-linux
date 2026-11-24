#include "AudioStream.h"

#include <QFile>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QTimer>
#include <QUuid>

#include <utility>

namespace {
constexpr int maxReaders = 3;
constexpr qint64 maxHeaderBytes = 8192;
constexpr qint64 socketBufferBytes = 128 * 1024;

void reject(QTcpSocket *socket, const QByteArray &status)
{
    socket->write("HTTP/1.1 " + status + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    socket->disconnectFromHost();
}
}

struct AudioStream::Reader
{
    QByteArray header;
    QFile file;
    qint64 position = 0;
    qint64 end = 0;
    qint64 waitingAt = -1;
    bool requested = false;
    bool body = false;
};

AudioStream::AudioStream(std::shared_ptr<QTemporaryFile> file, qint64 length, const QString &extension)
    : m_file(std::move(file)),
      m_path("/" + QUuid::createUuid().toByteArray(QUuid::WithoutBraces) + extension.toLatin1()),
      m_length(length)
{
    m_server.setProxy(QNetworkProxy::NoProxy);
    m_server.setMaxPendingConnections(maxReaders);
    connect(&m_server, &QTcpServer::newConnection, this, &AudioStream::acceptClients);
    m_server.listen(QHostAddress::LocalHost);
}

AudioStream::~AudioStream()
{
    for (auto *socket : m_readers.keys()) {
        socket->disconnect(this);
        socket->abort();
    }
}

QUrl AudioStream::url(qint64 offset) const
{
    if (!m_server.isListening())
        return {};
    QUrl result(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()) + QString::fromLatin1(m_path));
    if (offset > 0)
        result.setQuery(QStringLiteral("from=%1").arg(offset));
    return result;
}

qint64 AudioStream::offsetForTime(qint64 milliseconds) const
{
    if (!m_path.endsWith(".mp3") || milliseconds < 0)
        return -1;
    QFile input(m_file->fileName());
    if (!input.open(QIODevice::ReadOnly))
        return -1;
    const auto prefix = input.read(64 * 1024);
    if (prefix.size() < 10)
        return -1;
    const auto *bytes = reinterpret_cast<const unsigned char *>(prefix.constData());
    int start = 0;
    if (prefix.startsWith("ID3"))
        start = 10 + ((bytes[6] & 127) << 21) + ((bytes[7] & 127) << 14)
             + ((bytes[8] & 127) << 7) + (bytes[9] & 127);

    // Only open slices for MP3 with a stable frame bitrate. Indexed/VBR media
    // uses the demuxer's normal seeking rather than an estimated byte offset.
    const int mpeg1Rates[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    const int otherRates[] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
    const int frequencies[] = {44100, 48000, 32000};
    if (prefix.mid(start, 256).contains("Xing") || prefix.mid(start, 256).contains("VBRI"))
        return -1;
    int bitrate = 0;
    int frame = start;
    for (int sample = 0; sample < 4; ++sample) {
        if (frame + 4 > prefix.size() || bytes[frame] != 0xff || (bytes[frame + 1] & 0xe0) != 0xe0)
            return -1;
        const int version = (bytes[frame + 1] >> 3) & 3;
        const int layer = (bytes[frame + 1] >> 1) & 3;
        const int frequencyIndex = (bytes[frame + 2] >> 2) & 3;
        if (version == 1 || layer != 1 || frequencyIndex == 3)
            return -1;
        const int rate = (version == 3 ? mpeg1Rates : otherRates)[bytes[frame + 2] >> 4];
        const int frequency = frequencies[frequencyIndex] / (version == 3 ? 1 : version == 2 ? 2 : 4);
        if (!rate || (bitrate && rate != bitrate))
            return -1;
        bitrate = rate;
        frame += (version == 3 ? 144000 : 72000) * rate / frequency + ((bytes[frame + 2] >> 1) & 1);
    }
    return qMin(m_length - 1, start + milliseconds * bitrate / 8);
}

void AudioStream::acceptClients()
{
    while (auto *socket = m_server.nextPendingConnection()) {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        if (m_readers.size() >= maxReaders) {
            reject(socket, "503 Service Unavailable");
            continue;
        }
        m_readers.insert(socket, std::make_shared<Reader>());
        socket->setReadBufferSize(maxHeaderBytes);
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] { m_readers.remove(socket); });
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { readRequest(socket); });
        connect(socket, &QTcpSocket::bytesWritten, this, [this, socket] { pump(socket); });
        QTimer::singleShot(3000, socket, [this, socket] {
            const auto reader = m_readers.value(socket);
            if (reader && !reader->requested)
                socket->disconnectFromHost();
        });
    }
}

void AudioStream::readRequest(QTcpSocket *socket)
{
    const auto reader = m_readers.value(socket);
    if (!reader)
        return;
    if (reader->requested) {
        socket->readAll();
        return;
    }
    reader->header += socket->readAll();
    const int end = reader->header.indexOf("\r\n\r\n");
    if (reader->header.size() >= maxHeaderBytes) {
        reader->requested = true;
        reject(socket, "431 Request Header Fields Too Large");
        return;
    }
    if (end < 0)
        return;

    reader->requested = true;
    const auto lines = reader->header.left(end).split('\n');
    const auto parts = lines.value(0).trimmed().split(' ');
    const auto target = parts.value(1);
    qint64 base = 0;
    bool validTarget = target == m_path;
    const auto prefix = m_path + "?from=";
    if (target.startsWith(prefix)) {
        base = target.mid(prefix.size()).toLongLong(&validTarget);
        validTarget = validTarget && base >= 0 && base < m_length;
    }
    if (parts.size() != 3 || !validTarget || (parts.value(0) != "GET" && parts.value(0) != "HEAD")) {
        reject(socket, "404 Not Found");
        return;
    }
    QByteArray range;
    for (const auto &line : lines) {
        const auto header = line.trimmed().toLower();
        if (header.startsWith("origin:") || header.startsWith("sec-fetch-")) {
            reject(socket, "403 Forbidden");
            return;
        }
        if (header.startsWith("range:"))
            range = header.mid(6).trimmed();
    }
    const qint64 length = m_length - base;
    reader->position = base;
    reader->end = m_length;
    if (!range.isEmpty()) {
        static const QRegularExpression syntax(QStringLiteral("^bytes=(\\d*)-(\\d*)$"));
        const auto match = syntax.match(QString::fromLatin1(range));
        bool firstOk = false, lastOk = false;
        const auto first = match.captured(1).toLongLong(&firstOk);
        const auto last = match.captured(2).toLongLong(&lastOk);
        if (!match.hasMatch() || (!firstOk && !lastOk)
            || (firstOk && (first >= length || (lastOk && last < first)))
            || (!firstOk && last <= 0)
            || (!match.captured(1).isEmpty() && !firstOk)
            || (!match.captured(2).isEmpty() && !lastOk)) {
            socket->write("HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */"
                          + QByteArray::number(length) + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            socket->disconnectFromHost();
            return;
        }
        reader->position = base + (firstOk ? first : qMax(qint64{0}, length - last));
        if (firstOk && lastOk)
            reader->end = base + qMin(last, length - 1) + 1;
    }
    const bool get = parts.value(0) == "GET";
    if (get) {
        reader->file.setFileName(m_file->fileName());
        if (!reader->file.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
            reject(socket, "500 Internal Server Error");
            return;
        }
    }
    QByteArray response = range.isEmpty() ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 206 Partial Content\r\n";
    if (!range.isEmpty()) {
        response += "Content-Range: bytes " + QByteArray::number(reader->position - base) + "-"
                 + QByteArray::number(reader->end - base - 1) + "/" + QByteArray::number(length) + "\r\n";
    }
    response += "Content-Type: application/octet-stream\r\nAccept-Ranges: bytes\r\n"
                "Cache-Control: no-store\r\nConnection: close\r\nContent-Length: "
             + QByteArray::number(reader->end - reader->position) + "\r\n\r\n";
    socket->write(response);
    reader->body = get;
    if (get)
        pump(socket);
    else
        socket->disconnectFromHost();
}

qint64 AudioStream::bufferedEnd(qint64 position) const
{
    for (const auto &range : m_ranges) {
        if (position < range.first)
            break;
        if (position < range.second)
            return range.second;
    }
    return position;
}

bool AudioStream::needsDataAt(qint64 position) const
{
    if (m_finished || bufferedEnd(position) > position)
        return false;
    for (auto it = m_readers.cbegin(); it != m_readers.cend(); ++it) {
        const auto &reader = it.value();
        if (it.key()->state() == QAbstractSocket::ConnectedState && reader->body
            && reader->position == position && position < reader->end)
            return true;
    }
    return false;
}

void AudioStream::pump(QTcpSocket *socket)
{
    const auto reader = m_readers.value(socket);
    if (!reader || !reader->body || socket->state() != QAbstractSocket::ConnectedState)
        return;
    const auto available = qMin(reader->end, bufferedEnd(reader->position));
    const auto amount = qMin(available - reader->position, socketBufferBytes - socket->bytesToWrite());
    if (amount > 0) {
        if (!reader->file.seek(reader->position)) {
            socket->disconnectFromHost();
            return;
        }
        const auto bytes = reader->file.read(amount);
        if (bytes.isEmpty() || socket->write(bytes) != bytes.size()) {
            socket->disconnectFromHost();
            return;
        }
        reader->position += bytes.size();
        reader->waitingAt = -1;
    }
    if (reader->position == reader->end || (m_finished && reader->position == available)) {
        socket->disconnectFromHost();
    } else if (reader->position == available && reader->waitingAt != reader->position) {
        reader->waitingAt = reader->position;
        emit dataNeeded(reader->position);
    }
}

void AudioStream::publish(qint64 first, qint64 end)
{
    if (first >= end || bufferedEnd(first) >= end)
        return;
    auto it = m_ranges.begin();
    while (it != m_ranges.end() && it->second < first)
        ++it;
    while (it != m_ranges.end() && it->first <= end) {
        first = qMin(first, it->first);
        end = qMax(end, it->second);
        it = m_ranges.erase(it);
    }
    m_ranges.insert(it, {first, end});
    for (auto *socket : m_readers.keys())
        pump(socket);
    emit bufferedBytesChanged();
}

void AudioStream::finish(bool complete)
{
    if (m_finished)
        return;
    m_finished = true;
    m_complete = complete;
    if (complete) {
        publish(0, m_length);
        emit completed();
    } else {
        for (auto *socket : m_readers.keys())
            pump(socket);
    }
}
