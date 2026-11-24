#include "ResponseStore.h"

#include <QCborValue>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

namespace {
constexpr quint32 storeMagic = 0x52524331; // "RRC1"
}

void ResponseStore::setScope(const QString &scope)
{
    const auto base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/responses/";
    m_directory = base + (scope.isEmpty() ? QStringLiteral("guest") : scope);
    QDir().mkpath(m_directory);
}

QString ResponseStore::pathFor(const QString &key) const
{
    const auto hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    return m_directory + u'/' + QString::fromLatin1(hash);
}

ResponseStore::Entry ResponseStore::load(const QString &key) const
{
    if (m_directory.isEmpty())
        return {};
    QFile file(pathFor(key));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QDataStream in(&file);
    quint32 magic = 0;
    qint64 storedAt = 0;
    QString storedKey;
    QByteArray payload;
    in >> magic >> storedAt >> storedKey >> payload;
    if (in.status() != QDataStream::Ok || magic != storeMagic || storedKey != key)
        return {};
    const auto value = QCborValue::fromCbor(qUncompress(payload));
    if (!value.isMap())
        return {};
    return {value.toJsonValue().toObject(), storedAt, true};
}

void ResponseStore::save(const QString &key, const QJsonObject &body, qint64 storedAt)
{
    if (m_directory.isEmpty())
        return;
    QSaveFile file(pathFor(key));
    if (!file.open(QIODevice::WriteOnly))
        return;
    QDataStream out(&file);
    out << storeMagic << storedAt << key << qCompress(QCborValue::fromJsonValue(body).toCbor(), 6);
    file.commit();
}

void ResponseStore::remove(const QString &key)
{
    if (!m_directory.isEmpty())
        QFile::remove(pathFor(key));
}

void ResponseStore::clearScope()
{
    if (m_directory.isEmpty())
        return;
    QDir(m_directory).removeRecursively();
    QDir().mkpath(m_directory);
}

void ResponseStore::trim(qint64 limitBytes) const
{
    const auto base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/responses";
    QList<QFileInfo> files;
    qint64 total = 0;
    QDirIterator it(base, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto info = QFileInfo(it.next());
        files.append(info);
        total += info.size();
    }
    if (total <= limitBytes)
        return;
    std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.lastModified() < b.lastModified();
    });
    for (const auto &info : std::as_const(files)) {
        if (total <= limitBytes * 3 / 4)
            break;
        total -= info.size();
        QFile::remove(info.absoluteFilePath());
    }
}
