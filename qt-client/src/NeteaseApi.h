#pragma once

#include "AntiCheat.h"

#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrlQuery>
#include <functional>

// Talks to music.163.com directly: encrypts requests like the official clients, keeps the
// login cookies, and answers the same routes the old Node bridge served, so the rest of
// the client is unchanged. Replaces the external api-enhanced/Node.js process.
class NeteaseApi : public QObject
{
    Q_OBJECT
public:
    using Reply = std::function<void(const QJsonObject &body, int status)>;

    explicit NeteaseApi(QObject *parent = nullptr);

    // Routes and parameters match the former local HTTP API (e.g. "/playlist/detail", id=...).
    void call(const QString &route, const QUrlQuery &query, bool post, Reply reply);
    bool hasSession() const { return m_cookies.contains("MUSIC_U"); }

private:
    enum class Crypto { Weapi, Eapi };
    using Json = std::function<void(const QJsonObject &body, int status, const QList<QByteArray> &cookies)>;

    void request(const QString &path, QJsonObject data, Crypto crypto, Json done, int timeout = 10000,
                 const QString &antiCheatToken = {});
    void weapi(const QString &path, const QJsonObject &data, Reply reply);
    void eapi(const QString &path, const QJsonObject &data, Reply reply);
    void withAccount(Reply reply, std::function<void(qint64 uid)> next);
    void playlistTracks(const QString &id, int limit, int offset, Reply reply);
    void essential(const QString &route, Reply reply);
    void qrLogin(const QString &route, Reply reply);

    QByteArray cookieHeader(Crypto crypto, const QJsonObject &eapiHeader) const;
    QJsonObject eapiHeader() const;
    void loadSession();
    void saveSession();

    QNetworkAccessManager m_network;
    AntiCheat m_antiCheat;
    QHash<QByteArray, QByteArray> m_cookies;      // login cookies (MUSIC_U, __csrf, ...)
    QByteArray m_deviceId;
    QByteArray m_nuid;
    QByteArray m_wnmcid;
    QJsonObject m_profile;
    qint64 m_profileFor = 0;
    QString m_qrKey;
    int m_qrRevision = 0;
};
