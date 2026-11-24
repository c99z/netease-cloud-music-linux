#pragma once

#include <QJsonObject>
#include <QString>

// Disk cache for public API responses. Entries are compressed CBOR files named by a hash
// of the request, grouped per account so one user's recommendations never show for another.
class ResponseStore
{
public:
    struct Entry {
        QJsonObject body;
        qint64 storedAt = 0;
        bool valid = false;
    };

    void setScope(const QString &scope);
    Entry load(const QString &key) const;
    void save(const QString &key, const QJsonObject &body, qint64 storedAt);
    void remove(const QString &key);
    void clearScope();
    // Keeps the directory under the size limit by deleting the least recently written files.
    void trim(qint64 limitBytes = 64LL * 1024 * 1024) const;

private:
    QString pathFor(const QString &key) const;
    QString m_directory;
};
