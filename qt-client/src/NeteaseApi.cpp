#include "NeteaseApi.h"
#include "NeteaseCrypto.h"
#include "QrCode.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkCookie>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QPointer>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <memory>

namespace {
const QString webDomain = QStringLiteral("https://music.163.com");
const QString eapiDomain = QStringLiteral("https://interfacepc.music.163.com");
const QByteArray webAgent =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/124.0.0.0 Safari/537.36 Edg/124.0.0.0";
const QByteArray clientAgent = "NeteaseMusic 9.0.90/5038 (iPhone; iOS 16.2; zh_CN)";
// Matches the "pc" device profile the reference API uses for eapi.
const QByteArray osName = "pc";
const QByteArray osVersion = "Microsoft-Windows-10-Professional-build-19045-64bit";
const QByteArray appVersion = "3.1.17.204416";
const QByteArray channel = "netease";
const QStringList keptCookies{"MUSIC_U", "MUSIC_R_T", "__csrf", "NMTID"};

QString sessionPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/session.json";
}

QByteArray randomText(const char *alphabet, int length)
{
    const QByteArray chars(alphabet);
    QByteArray out(length, Qt::Uninitialized);
    for (auto &c : out)
        c = chars.at(QRandomGenerator::global()->bounded(int(chars.size())));
    return out;
}

QJsonValue number(const QUrlQuery &query, const QString &key, qint64 fallback)
{
    bool ok = false;
    const auto value = query.queryItemValue(key, QUrl::FullyDecoded).toLongLong(&ok);
    return ok ? QJsonValue(value) : QJsonValue(fallback);
}

QString value(const QUrlQuery &query, const QString &key)
{
    return query.queryItemValue(key, QUrl::FullyDecoded);
}

QByteArray encodeCookie(const QHash<QByteArray, QByteArray> &cookies)
{
    QByteArrayList parts;
    for (auto it = cookies.cbegin(); it != cookies.cend(); ++it)
        parts.append(QUrl::toPercentEncoding(QString::fromUtf8(it.key())) + '='
                     + QUrl::toPercentEncoding(QString::fromUtf8(it.value())));
    return parts.join("; ");
}

// What the reference library does when a request fails before a JSON body arrives.
QJsonObject failure(int status)
{
    return {{"code", status}};
}
}

NeteaseApi::NeteaseApi(QObject *parent)
    : QObject(parent)
{
    m_network.setProxy(QNetworkProxy::NoProxy);
    m_network.setAutoDeleteReplies(true);
    QSettings settings;
    m_deviceId = settings.value("api/deviceId").toByteArray();
    if (m_deviceId.size() != 52) {
        m_deviceId = randomText("0123456789ABCDEF", 52);
        settings.setValue("api/deviceId", m_deviceId);
    }
    m_nuid = randomText("0123456789abcdef", 32);
    m_wnmcid = randomText("abcdefghijklmnopqrstuvwxyz", 6) + '.'
               + QByteArray::number(QDateTime::currentMSecsSinceEpoch()) + ".01.0";
    loadSession();
}

void NeteaseApi::loadSession()
{
    QFile file(sessionPath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const auto object = QJsonDocument::fromJson(file.readAll()).object();
    for (const auto &name : keptCookies) {
        const auto text = object.value(name).toString().toUtf8();
        if (!text.isEmpty() && text.size() <= 8192 && !text.contains(';'))
            m_cookies.insert(name.toUtf8(), text);
    }
}

void NeteaseApi::saveSession()
{
    QDir().mkpath(QFileInfo(sessionPath()).absolutePath());
    if (!m_cookies.contains("MUSIC_U")) {
        QFile::remove(sessionPath());
        return;
    }
    QJsonObject object;
    for (const auto &name : keptCookies) {
        if (m_cookies.contains(name.toUtf8()))
            object.insert(name, QString::fromUtf8(m_cookies.value(name.toUtf8())));
    }
    QSaveFile file(sessionPath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    file.commit();
}

QJsonObject NeteaseApi::eapiHeader() const
{
    QJsonObject header{
        {"osver", QString::fromLatin1(osVersion)}, {"deviceId", QString::fromLatin1(m_deviceId)},
        {"os", QString::fromLatin1(osName)}, {"appver", QString::fromLatin1(appVersion)},
        {"versioncode", "140"}, {"mobilename", ""},
        {"buildver", QString::number(QDateTime::currentSecsSinceEpoch())},
        {"resolution", "1920x1080"}, {"__csrf", QString::fromUtf8(m_cookies.value("__csrf"))},
        {"channel", QString::fromLatin1(channel)},
        {"requestId", QStringLiteral("%1_%2").arg(QDateTime::currentMSecsSinceEpoch())
                          .arg(QRandomGenerator::global()->bounded(1000), 4, 10, QLatin1Char('0'))},
    };
    if (m_cookies.contains("MUSIC_U"))
        header.insert("MUSIC_U", QString::fromUtf8(m_cookies.value("MUSIC_U")));
    return header;
}

QByteArray NeteaseApi::cookieHeader(Crypto crypto, const QJsonObject &header) const
{
    if (crypto == Crypto::Eapi) {
        QHash<QByteArray, QByteArray> cookies;
        for (auto it = header.begin(); it != header.end(); ++it)
            cookies.insert(it.key().toUtf8(), it.value().toString().toUtf8());
        if (m_cookies.contains("NMTID"))
            cookies.insert("NMTID", m_cookies.value("NMTID"));
        return encodeCookie(cookies);
    }
    auto cookies = m_cookies;
    cookies.insert("__remember_me", "true");
    cookies.insert("ntes_kaola_ad", "1");
    cookies.insert("_ntes_nuid", m_nuid);
    cookies.insert("_ntes_nnid", m_nuid + ',' + QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
    cookies.insert("WNMCID", m_wnmcid);
    cookies.insert("WEVNSM", "1.0.0");
    cookies.insert("osver", osVersion);
    cookies.insert("deviceId", m_deviceId);
    cookies.insert("os", osName);
    cookies.insert("channel", channel);
    cookies.insert("appver", appVersion);
    if (!cookies.contains("NMTID"))
        cookies.insert("NMTID", "00O" + randomText("0123456789abcdef", 38));
    return encodeCookie(cookies);
}

void NeteaseApi::request(const QString &path, QJsonObject data, Crypto crypto, Json done, int timeout,
                         const QString &antiCheatToken)
{
    QNetworkRequest request;
    request.setTransferTimeout(timeout);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/x-www-form-urlencoded"));
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    QByteArray body;
    data.insert("e_r", false);
    if (crypto == Crypto::Weapi) {
        data.insert("csrf_token", QString::fromUtf8(m_cookies.value("__csrf")));
        request.setUrl(QUrl(webDomain + "/weapi/" + path.mid(5)));
        request.setRawHeader("Referer", webDomain.toUtf8());
        request.setHeader(QNetworkRequest::UserAgentHeader, webAgent);
        request.setRawHeader("Cookie", cookieHeader(crypto, {}));
        body = NeteaseCrypto::weapi(QJsonDocument(data).toJson(QJsonDocument::Compact));
    } else {
        auto header = eapiHeader();
        if (!antiCheatToken.isEmpty())
            header.insert("X-antiCheatToken", antiCheatToken);
        data.insert("header", header);
        request.setUrl(QUrl(eapiDomain + "/eapi/" + path.mid(5)));
        request.setHeader(QNetworkRequest::UserAgentHeader, clientAgent);
        request.setRawHeader("Cookie", cookieHeader(crypto, header));
        body = NeteaseCrypto::eapi(path, QJsonDocument(data).toJson(QJsonDocument::Compact));
    }

    auto *reply = m_network.post(request, body);
    QPointer<NeteaseApi> self(this);
    connect(reply, &QNetworkReply::finished, this, [self, reply, done = std::move(done)] {
        if (!self)
            return;
        const QByteArray raw = reply->readAll();
        QList<QByteArray> cookies;
        for (const auto &header : reply->rawHeaderPairs()) {
            if (header.first.compare("set-cookie", Qt::CaseInsensitive) == 0)
                cookies.append(header.second.split('\n'));
        }
        // Remember the anonymous-session id the server hands out to the desktop client.
        for (const auto &line : std::as_const(cookies)) {
            if (line.startsWith("NMTID=") && !self->m_cookies.contains("NMTID"))
                self->m_cookies.insert("NMTID", line.mid(6).split(';').first());
        }
        auto document = QJsonDocument::fromJson(raw);
        if (!document.isObject()) {
            // Encrypted body (only if the server ignored e_r=false).
            const auto plain = NeteaseCrypto::eapiDecrypt(raw);
            document = QJsonDocument::fromJson(plain);
        }
        if (!document.isObject()) {
            const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            done(failure(reply->error() == QNetworkReply::NoError && http < 400 ? 502 : http ? http : 502), 502, cookies);
            return;
        }
        auto object = document.object();
        int code = object.value("code").toVariant().toInt();
        if (object.value("code").isString())
            object.insert("code", code);
        // Same normalization as the reference: login poll codes and a few others are not errors.
        static const QSet<int> special{201, 302, 400, 502, 800, 801, 802, 803};
        const int status = !object.contains("code") || code == 200 || special.contains(code) ? 200 : code;
        done(object, status, cookies);
    });
}

void NeteaseApi::weapi(const QString &path, const QJsonObject &data, Reply reply)
{
    request(path, data, Crypto::Weapi, [reply](const QJsonObject &body, int status, const QList<QByteArray> &) {
        reply(body, status);
    });
}

void NeteaseApi::eapi(const QString &path, const QJsonObject &data, Reply reply)
{
    request(path, data, Crypto::Eapi, [reply](const QJsonObject &body, int status, const QList<QByteArray> &) {
        reply(body, status);
    });
}

void NeteaseApi::withAccount(Reply reply, std::function<void(qint64)> next)
{
    if (!hasSession()) {
        reply(failure(401), 401);
        return;
    }
    const auto uid = m_profile.value("userId").toVariant().toLongLong();
    if (uid > 0) {
        next(uid);
        return;
    }
    call("/account/session", {}, false, [this, reply, next](const QJsonObject &, int status) {
        const auto uid = m_profile.value("userId").toVariant().toLongLong();
        if (uid > 0)
            next(uid);
        else
            reply(failure(status == 200 ? 401 : status), status == 200 ? 401 : status);
    });
}

void NeteaseApi::playlistTracks(const QString &id, int limit, int offset, Reply reply)
{
    eapi("/api/v6/playlist/detail", {{"id", id}, {"n", 100000}, {"s", 8}},
         [this, limit, offset, reply](const QJsonObject &detail, int status) {
             if (status != 200) {
                 reply(detail, status);
                 return;
             }
             const auto ids = detail.value("playlist").toObject().value("trackIds").toArray();
             QStringList parts;
             for (int i = offset; i < std::min<qsizetype>(ids.size(), offset + limit); ++i)
                 parts.append(QStringLiteral("{\"id\":%1}").arg(ids.at(i).toObject().value("id").toVariant().toLongLong()));
             if (parts.isEmpty()) {
                 reply({{"code", 200}, {"songs", QJsonArray{}}, {"privileges", QJsonArray{}}}, 200);
                 return;
             }
             eapi("/api/v3/song/detail", {{"c", u'[' + parts.join(u',') + u']'}}, reply);
         });
}

void NeteaseApi::essential(const QString &route, Reply reply)
{
    auto official = [this](int limit, Reply done) {
        weapi("/api/playlist/category/list", {{"cat", "官方"}, {"limit", limit}, {"newStyle", true}}, done);
    };
    auto podcasts = [this](Reply done) {
        weapi("/api/pc/voicelist/rcmd/list", {{"limit", 12}}, done);
    };
    if (route == "/essential/official") {
        official(40, reply);
        return;
    }
    if (route == "/essential/podcasts") {
        podcasts(reply);
        return;
    }
    struct Parts {
        QJsonArray official, latest, podcasts;
        int pending = 3;
    };
    auto parts = std::make_shared<Parts>();
    auto finish = [parts, reply] {
        if (--parts->pending > 0)
            return;
        const bool loaded = !parts->official.isEmpty() || !parts->latest.isEmpty() || !parts->podcasts.isEmpty();
        reply({{"code", loaded ? 200 : 502}, {"official", parts->official}, {"latest", parts->latest},
               {"podcasts", parts->podcasts}}, loaded ? 200 : 502);
    };
    official(6, [parts, finish](const QJsonObject &body, int) {
        parts->official = body.value("playlists").toArray();
        finish();
    });
    weapi("/api/personalized/newsong", {{"type", "recommend"}, {"limit", 12}, {"areaId", 0}},
          [parts, finish](const QJsonObject &body, int) {
              parts->latest = body.value("result").toArray();
              finish();
          });
    podcasts([parts, finish](const QJsonObject &body, int) {
        parts->podcasts = body.value("data").toObject().value("recommendVoiceVOS").toArray();
        finish();
    });
}

void NeteaseApi::qrLogin(const QString &route, Reply reply)
{
    if (route == "/account/qr") {
        const int revision = ++m_qrRevision;
        m_qrKey.clear();
        eapi("/api/login/qrcode/unikey", {{"type", 3}}, [this, revision, reply](const QJsonObject &body, int status) {
            const auto key = body.value("unikey").toString();
            if (revision != m_qrRevision || status != 200 || key.isEmpty()) {
                reply(failure(502), 502);
                return;
            }
            m_qrKey = key;
            const auto image = QrCode::encode(("https://music.163.com/login?codekey=" + key).toUtf8()).toImage(6, 2);
            QByteArray png;
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG");
            reply({{"code", 200}, {"image", QString::fromLatin1("data:image/png;base64," + png.toBase64())}}, 200);
        });
        return;
    }
    // "/account/qr/check"
    if (m_qrKey.isEmpty()) {
        reply({{"code", 200}, {"phase", 800}}, 200);
        return;
    }
    const int revision = m_qrRevision;
    request("/api/login/qrcode/client/login", {{"key", m_qrKey}, {"type", 3}}, Crypto::Eapi,
            [this, revision, reply](const QJsonObject &body, int, const QList<QByteArray> &cookies) {
                if (revision != m_qrRevision) {
                    reply({{"code", 200}, {"phase", 800}}, 200);
                    return;
                }
                const int phase = body.value("code").toInt();
                if (phase != 800 && phase != 801 && phase != 802 && phase != 803) {
                    reply(failure(502), 502);
                    return;
                }
                if (phase == 803) {
                    QHash<QByteArray, QByteArray> signin;
                    for (const auto &line : cookies) {
                        const auto pair = line.split(';').first();
                        const int eq = pair.indexOf('=');
                        if (eq > 0 && keptCookies.contains(QString::fromUtf8(pair.left(eq).trimmed())))
                            signin.insert(pair.left(eq).trimmed(), pair.mid(eq + 1).trimmed());
                    }
                    if (!signin.contains("MUSIC_U")) {
                        reply(failure(502), 502);
                        return;
                    }
                    m_cookies = signin;
                    m_profile = {};
                    saveSession();
                    m_qrKey.clear();
                }
                if (phase == 800)
                    m_qrKey.clear();
                reply({{"code", 200}, {"phase", phase}}, 200);
            });
}

void NeteaseApi::call(const QString &route, const QUrlQuery &q, bool post, Reply reply)
{
    Q_UNUSED(post)
    const auto id = value(q, "id");
    const auto limit = [&q](qint64 fallback) { return number(q, "limit", fallback); };
    const auto offset = number(q, "offset", 0);

    // ---- account and session -------------------------------------------------------
    if (route == "/account/session") {
        if (!hasSession()) {
            reply({{"code", 200}, {"profile", QJsonValue::Null}}, 200);
            return;
        }
        weapi("/api/nuser/account/get", {}, [this, reply](const QJsonObject &body, int status) {
            if (status != 200) {
                reply(body, status);
                return;
            }
            const auto profile = body.value("profile").toObject();
            if (profile.value("userId").toVariant().toLongLong() <= 0) {
                m_cookies.clear();
                saveSession();
                m_profile = {};
                reply({{"code", 200}, {"profile", QJsonValue::Null}}, 200);
                return;
            }
            m_profile = {{"userId", profile.value("userId")},
                         {"nickname", profile.value("nickname").toString(QStringLiteral("网易云用户"))},
                         {"avatarUrl", profile.value("avatarUrl").toString()}};
            reply({{"code", 200}, {"profile", m_profile}}, 200);
        });
        return;
    }
    if (route == "/account/logout") {
        ++m_qrRevision;
        m_qrKey.clear();
        m_cookies.clear();
        m_profile = {};
        saveSession();
        reply({{"code", 200}}, 200);
        return;
    }
    if (route == "/account/qr" || route == "/account/qr/check") {
        qrLogin(route, reply);
        return;
    }

    // ---- account actions -------------------------------------------------------------
    const auto digits = [](const QString &text) {
        static const QRegularExpression pattern(QStringLiteral("^\\d+$"));
        return pattern.match(text).hasMatch();
    };
    if (route == "/account/like" || route == "/account/fm/trash") {
        if (!hasSession() || !digits(id)) {
            reply(failure(hasSession() ? 400 : 401), hasSession() ? 400 : 401);
            return;
        }
        if (route == "/account/like")
            weapi("/api/radio/like", {{"alg", "itembased"}, {"trackId", id},
                                      {"like", value(q, "like") != "false"}, {"time", "3"}}, reply);
        else
            weapi("/api/radio/trash/add", {{"songId", id}, {"alg", "RT"}, {"time", 25}}, reply);
        return;
    }
    if (route == "/account/playlists/create") {
        const auto name = value(q, "name").trimmed();
        if (!hasSession() || name.isEmpty() || name.size() > 40) {
            reply(failure(400), 400);
            return;
        }
        weapi("/api/playlist/create", {{"name", name}, {"privacy", "0"}, {"type", "NORMAL"}}, reply);
        return;
    }
    if (route == "/account/playlist/subscribe") {
        if (!hasSession() || !digits(id)) {
            reply(failure(400), 400);
            return;
        }
        // Like the official client, both requests carry a fresh Yidun anti-abuse token
        // (header X-antiCheatToken); subscribing also sends it as checkToken. If the token
        // cannot be produced, fall back to the client's static check token.
        const bool subscribe = value(q, "subscribe") != "false";
        const QString path = subscribe ? QStringLiteral("/api/playlist/subscribe") : QStringLiteral("/api/playlist/unsubscribe");
        QPointer<NeteaseApi> self(this);
        m_antiCheat.token([self, id, subscribe, path, reply](const QString &token) {
            if (!self)
                return;
            static const QString fallback = QStringLiteral(
                "9ca17ae2e6ffcda170e2e6ee8af14fbabdb988f225b3868eb2c15a879b9a83d274a790ac8ff54a97b889d5d42af0feaec3b92af58cff99c470a7eafd88f75e839a9ea7c14e909da883e83fb692a3abdb6b92adee9e");
            if (token.isEmpty())
                qWarning() << "Yidun token unavailable, using the static check token";
            QJsonObject data{{"id", id}};
            if (subscribe)
                data.insert("checkToken", token.isEmpty() ? fallback : token);
            self->request(path, data, Crypto::Eapi, [reply](const QJsonObject &body, int status, const QList<QByteArray> &) {
                reply(body, status);
            }, 10000, token);
        });
        return;
    }
    if (route == "/account/playlist/tracks") {
        const auto pid = value(q, "pid");
        const auto track = value(q, "track");
        const auto op = value(q, "op");
        if (!hasSession() || !digits(pid) || !digits(track) || (op != "add" && op != "del")) {
            reply(failure(400), 400);
            return;
        }
        const QJsonObject data{{"op", op}, {"pid", pid}, {"trackIds", "[\"" + track + "\"]"}, {"imme", "true"}};
        eapi("/api/playlist/manipulate/tracks", data, [this, data, track, reply](const QJsonObject &body, int status) {
            if (body.value("code").toInt() != 512) {
                reply(body, status);
                return;
            }
            auto retry = data;
            retry.insert("trackIds", "[\"" + track + "\",\"" + track + "\"]");
            eapi("/api/playlist/manipulate/tracks", retry, reply);
        });
        return;
    }

    // ---- account reads -------------------------------------------------------------
    static const QHash<QString, QPair<QString, int>> accountWeapi{
        {"/account/daily", {"/api/v3/discovery/recommend/songs", -1}},
        {"/account/recommendations", {"/api/v1/discovery/recommend/resource", -1}},
        {"/account/recent", {"/api/play-record/song/list", 100}},
        {"/account/recent/playlists", {"/api/play-record/playlist/list", 100}},
        {"/account/recent/albums", {"/api/play-record/album/list", 100}},
        {"/account/recent/radios", {"/api/play-record/djradio/list", 100}},
        {"/account/recent/voices", {"/api/play-record/voice/list", 100}},
        {"/account/recent/videos", {"/api/play-record/newvideo/list", 100}},
        {"/account/created-voices", {"/api/social/my/created/voicelist/v1", 20}},
        {"/account/collections/albums", {"/api/album/sublist", 25}},
        {"/account/collections/artists", {"/api/artist/sublist", 25}},
        {"/account/collections/videos", {"/api/cloudvideo/allvideo/sublist", 25}},
        {"/account/radios", {"/api/djradio/get/subed", 30}},
        {"/account/fm", {"/api/v1/radio/get", -1}},
        {"/account/podcasts/saved", {"/api/social/my/subscribed/voicelist/v1", 50}},
        {"/account/podcasts/bought", {"/api/voicelist/bought/list", 50}},
        {"/account/podcasts/liked", {"/api/content/my/liked/voice", 50}},
        {"/account/vip/songs", {"/api/vipnewcenter/app/viptab/recommend/song/list", 30}},
    };
    if (accountWeapi.contains(route) || route.startsWith("/account/")) {
        withAccount(reply, [this, route, q, limit, reply](qint64 uid) {
            const auto entry = accountWeapi.value(route);
            if (!entry.first.isEmpty()) {
                QJsonObject data;
                if (entry.second >= 0) {
                    data.insert("limit", limit(entry.second));
                    data.insert("offset", 0);
                    data.insert("total", true);
                }
                weapi(entry.first, data, reply);
            } else if (route == "/account/playlists") {
                weapi("/api/user/playlist", {{"uid", uid}, {"limit", limit(30)}, {"offset", 0}, {"includeVideo", true}}, reply);
            } else if (route == "/account/likes") {
                eapi("/api/song/like/get", {{"uid", uid}}, reply);
            } else if (route == "/account/follows") {
                weapi(QStringLiteral("/api/user/getfollows/%1").arg(uid), {{"offset", 0}, {"limit", limit(30)}, {"order", true}}, reply);
            } else if (route == "/account/events") {
                weapi("/api/event/pc/history/feed/get", {{"cursor", "-1"}, {"limit", 25}},
                      [this, reply](const QJsonObject &body, int status) {
                          if (status == 200) {
                              reply(body, status);
                              return;
                          }
                          weapi("/api/v1/event/get", {{"pagesize", 25}, {"lasttime", -1}}, reply);
                      });
            } else if (route == "/account/recent-listen") {
                eapi("/api/pc/recent/listen/list", {}, reply);
            } else {
                reply(failure(404), 404);
            }
        });
        return;
    }

    // ---- public catalog ------------------------------------------------------------
    if (route == "/personalized") {
        weapi("/api/personalized/playlist", {{"limit", limit(30)}, {"total", true}, {"n", 1000}}, reply);
    } else if (route == "/personalized/newsong") {
        weapi("/api/personalized/newsong", {{"type", "recommend"}, {"limit", limit(10)}, {"areaId", 0}}, reply);
    } else if (route == "/top/playlist") {
        weapi("/api/playlist/list", {{"cat", value(q, "cat").isEmpty() ? QStringLiteral("全部") : value(q, "cat")},
                                      {"order", "hot"}, {"limit", limit(50)}, {"offset", offset}, {"total", true}}, reply);
    } else if (route == "/playlist/catlist") {
        eapi("/api/playlist/catalogue", {}, reply);
    } else if (route == "/toplist/detail") {
        weapi("/api/toplist/detail", {}, reply);
    } else if (route == "/toplist/groups") {
        weapi("/api/toplist/detail/v2", {}, reply);
    } else if (route == "/banner") {
        eapi("/api/v2/banner/get", {{"clientType", "pc"}}, reply);
    } else if (route == "/playlist/detail") {
        eapi("/api/v6/playlist/detail", {{"id", id}, {"n", 100000}, {"s", 8}}, reply);
    } else if (route == "/playlist/track/all") {
        playlistTracks(id, int(limit(1000).toInteger()), int(offset.toInteger()), reply);
    } else if (route == "/playlist/comments") {
        weapi("/api/v1/resource/comments/A_PL_0_" + id,
              {{"rid", id}, {"limit", limit(20)}, {"offset", offset}, {"beforeTime", 0}}, reply);
    } else if (route == "/playlist/subscribers") {
        eapi("/api/playlist/subscribers", {{"id", id}, {"limit", limit(20)}, {"offset", offset}}, reply);
    } else if (route == "/search/suggest") {
        weapi("/api/search/suggest/web", {{"s", value(q, "keywords")}}, reply);
    } else if (route == "/search/hot") {
        weapi("/api/hotsearchlist/get", {}, reply);
    } else if (route == "/cloudsearch") {
        eapi("/api/cloudsearch/pc", {{"s", value(q, "keywords")}, {"type", number(q, "type", 1)},
                                     {"limit", limit(30)}, {"offset", offset}, {"total", true}}, reply);
    } else if (route == "/simi/song") {
        weapi("/api/v1/discovery/simiSong", {{"songid", id}, {"limit", 50}, {"offset", 0}}, reply);
    } else if (route == "/simi/playlist") {
        weapi("/api/discovery/simiPlaylist", {{"songid", id}, {"limit", 50}, {"offset", 0}}, reply);
    } else if (route == "/artist/detail") {
        eapi("/api/artist/head/info/get", {{"id", id}}, reply);
    } else if (route == "/artist/albums") {
        weapi("/api/artist/albums/" + id, {{"limit", limit(30)}, {"offset", offset}, {"total", true}}, reply);
    } else if (route == "/artist/similar") {
        weapi("/api/discovery/simiArtist", {{"artistid", id}}, reply);
    } else if (route == "/artist/desc") {
        weapi("/api/artist/introduction", {{"id", id}}, reply);
    } else if (route == "/artist/songs") {
        eapi("/api/v1/artist/songs", {{"id", id}, {"private_cloud", "true"}, {"work_type", 1},
                                      {"order", "hot"}, {"offset", offset}, {"limit", limit(100)}}, reply);
    } else if (route == "/artists") {
        QJsonObject data{{"offset", offset}, {"limit", limit(30)}, {"total", true},
                         {"type", value(q, "type").isEmpty() ? QStringLiteral("1") : value(q, "type")},
                         {"area", value(q, "area")}};
        const auto initial = value(q, "initial");
        bool numeric = false;
        const int code = initial.toInt(&numeric);
        if (numeric)
            data.insert("initial", code);
        else if (!initial.isEmpty())
            data.insert("initial", int(initial.toUpper().at(0).unicode()));
        weapi("/api/v1/artist/list", data, reply);
    } else if (route == "/album/newest") {
        weapi("/api/discovery/newAlbum", {}, reply);
    } else if (route == "/album") {
        weapi("/api/v1/album/" + id, {}, reply);
    } else if (route == "/top/song") {
        weapi("/api/v1/discovery/new/songs", {{"areaId", number(q, "type", 0)}, {"total", true}}, reply);
    } else if (route == "/top/album") {
        const auto today = QDate::currentDate();
        weapi("/api/discovery/new/albums/area",
              {{"area", value(q, "area").isEmpty() ? QStringLiteral("ALL") : value(q, "area")},
               {"limit", limit(50)}, {"offset", offset}, {"type", "new"},
               {"year", today.year()}, {"month", today.month()}, {"total", false}, {"rcmd", true}}, reply);
    } else if (route == "/style/song") {
        weapi("/api/style-tag/home/song", {{"cursor", 0}, {"size", number(q, "size", 20)},
                                            {"tagId", value(q, "tagId")}, {"sort", 0}}, reply);
    } else if (route == "/song/url") {
        eapi("/api/song/enhance/player/url",
             {{"ids", "[\"" + id + "\"]"}, {"br", number(q, "br", 999000)}}, reply);
    } else if (route == "/song/detail") {
        QStringList parts;
        for (const auto &one : value(q, "ids").split(u',', Qt::SkipEmptyParts))
            parts.append(QStringLiteral("{\"id\":%1}").arg(one.trimmed()));
        weapi("/api/v3/song/detail", {{"c", u'[' + parts.join(u',') + u']'}}, reply);
    } else if (route == "/lyric") {
        eapi("/api/song/lyric", {{"id", id}, {"tv", -1}, {"lv", -1}, {"rv", -1}, {"kv", -1}, {"_nmclfl", 1}}, reply);
    } else if (route == "/lyric/new") {
        eapi("/api/song/lyric/v1",
             {{"id", id}, {"cp", false}, {"tv", 0}, {"lv", 0}, {"rv", 0}, {"kv", 0}, {"yv", 0}, {"ytv", 0}, {"yrv", 0}},
             reply);
    } else if (route == "/dj/recommend") {
        weapi("/api/djradio/recommend/v1", {}, reply);
    } else if (route == "/dj/program/recommend") {
        weapi("/api/program/recommend/v1", {{"limit", limit(10)}, {"offset", 0}}, reply);
    } else if (route == "/dj/program/detail") {
        weapi("/api/dj/program/detail", {{"id", id}}, reply);
    } else if (route == "/dj/categories") {
        weapi("/api/djradio/category/get", {}, reply);
    } else if (route == "/dj/by-category") {
        weapi("/api/djradio/recommend", {{"cateId", value(q, "type")}}, reply);
    } else if (route == "/dj/toplist") {
        weapi("/api/djradio/toplist", {{"limit", limit(100)}, {"offset", 0},
                                        {"type", value(q, "type") == "hot" ? 1 : 0}}, reply);
    } else if (route == "/voice/list") {
        eapi("/api/voice/workbench/voices/by/voicelist",
             {{"limit", value(q, "limit").isEmpty() ? QStringLiteral("200") : value(q, "limit")},
              {"offset", "0"}, {"voiceListId", value(q, "voiceListId")}}, reply);
    } else if (route == "/dj/program") {
        weapi("/api/dj/program/byradio", {{"radioId", value(q, "rid")}, {"limit", limit(30)},
                                           {"offset", offset}, {"asc", false}}, reply);
    } else if (route == "/scene/tags") {
        weapi("/api/playlist/tags", {}, reply);
    } else if (route == "/scene/playlists") {
        weapi("/api/playlist/category/list", {{"catId", id}, {"limit", 20}, {"newStyle", true}}, reply);
    } else if (route == "/vip/playlists") {
        weapi("/api/vipnewcenter/app/viptab/recommend/playList/list", {{"limit", 30}}, reply);
    } else if (route == "/vip/charts") {
        weapi("/api/vipnewcenter/app/viptab/ranking/list", {}, reply);
    } else if (route == "/homepage/radar") {
        weapi("/api/pc/customize/block/page", {{"newStyle", true}}, reply);
    } else if (route == "/homepage/blocks") {
        const QJsonArray blocks{"HOMPAGE_BLOCK_VIP_RCMD", "HOMEPAGE_BLOCK_RED_SIMILAR_SONG",
                                "HOMEPAGE_BLOCK_STYLE_RCMD", "HOMEPAGE_MUSIC_PODCAST_RCMD_BLOCK",
                                "HOMEPAGE_BLOCK_PLAYLIST_RCMD", "HOMEPAGE_BLOCK_ALL_LISTEN",
                                "HOMEPAGE_BLOCK_OFFICIAL_PLAYLIST"};
        const auto cursor = QJsonDocument(QJsonObject{{"offset", 0}, {"blockCodeOrderList", blocks}}).toJson(QJsonDocument::Compact);
        const auto ext = QJsonDocument(QJsonObject{{"abInfo", QJsonObject{{"hp-new-homepageV3.1", "t3"}}}}).toJson(QJsonDocument::Compact);
        weapi("/api/homepage/block/page", {{"cursor", QString::fromUtf8(cursor)}, {"extInfo", QString::fromUtf8(ext)},
                                           {"newStyle", true}}, reply);
    } else if (route.startsWith("/essential/")) {
        essential(route, reply);
    } else {
        reply(failure(404), 404);
    }
}
