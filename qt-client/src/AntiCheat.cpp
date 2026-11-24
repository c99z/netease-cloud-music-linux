#include "AntiCheat.h"

#include <QDateTime>
#include <algorithm>
#include <QFile>
#include <QJSValueIterator>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QPointer>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
const QString productNumber = QStringLiteral("YD00000558929251");
const QString businessId = QStringLiteral("bd5d2f973ef74cd2a61325a412ae54d9");
const QUrl loaderUrl(QStringLiteral("https://acstatic-dun.126.net/tool.min.js"));
const QByteArray browserAgent =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/124.0.0.0 Safari/537.36";
// Only NetEase's own anti-abuse hosts may be contacted from the sandboxed script.
bool allowedHost(const QUrl &url)
{
    const auto host = url.host();
    return url.scheme() == "https"
        && (host == "acstatic-dun.126.net" || host.endsWith(".dun.163.com") || host.endsWith(".dun.163yun.com"));
}
QString storagePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/dun-storage.json";
}
}

AntiCheat::AntiCheat(QObject *parent)
    : QObject(parent)
{
    m_network.setProxy(QNetworkProxy::NoProxy);
    m_network.setAutoDeleteReplies(true);
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, &AntiCheat::fail);
}

AntiCheat::~AntiCheat()
{
    m_instance = {};
    // Timers and the engine are children of this object and are deleted with it. The
    // engine must go first so it never outlives (or tries to collect) its host object.
    delete m_engine;
    m_engine = nullptr;
}

void AntiCheat::reset()
{
    for (auto *timer : std::as_const(m_timers))
        timer->deleteLater();
    m_timers.clear();
    m_instance = {};
    // This can run from inside a script callback, so the engine must outlive the call.
    if (m_engine)
        m_engine->deleteLater();
    m_engine = nullptr;
    m_starting = false;
}

double AntiCheat::now() const
{
    return double(QDateTime::currentMSecsSinceEpoch());
}

void AntiCheat::call(QJSValue function, const QJSValueList &arguments)
{
    if (!m_engine || !function.isCallable())
        return;
    const auto result = function.call(arguments);
    if (result.isError())
        qWarning().noquote() << "anti-cheat script:" << result.toString();
}

int AntiCheat::timer(int ms, const QJSValue &callback)
{
    const int id = ++m_nextTimer;
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    QJSEngine *engine = m_engine;
    connect(timer, &QTimer::timeout, this, [this, id, callback, engine] {
        if (auto *done = m_timers.take(id))
            done->deleteLater();
        if (engine == m_engine)
            call(callback);
    });
    m_timers.insert(id, timer);
    timer->start(qMax(0, ms));
    return id;
}

void AntiCheat::clearTimer(int id)
{
    if (auto *timer = m_timers.take(id))
        timer->deleteLater();
}

void AntiCheat::http(const QString &method, const QString &url, const QString &body,
                     const QJSValue &headers, const QJSValue &callback)
{
    const QUrl target(url);
    if (!allowedHost(target)) {
        call(callback, {0, QString()});
        return;
    }
    QNetworkRequest request(target);
    request.setTransferTimeout(10000);
    request.setHeader(QNetworkRequest::UserAgentHeader, browserAgent);
    request.setRawHeader("Origin", "https://music.163.com");
    request.setRawHeader("Referer", "https://music.163.com/");
    QJSValueIterator it(headers);
    while (it.hasNext()) {
        it.next();
        request.setRawHeader(it.name().toUtf8(), it.value().toString().toUtf8());
    }
    QNetworkReply *reply = method.compare("POST", Qt::CaseInsensitive) == 0
        ? m_network.post(request, body.toUtf8()) : m_network.get(request);
    QPointer<AntiCheat> self(this);
    QJSEngine *engine = m_engine;
    connect(reply, &QNetworkReply::finished, this, [self, engine, reply, callback] {
        if (!self || engine != self->m_engine)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        self->call(callback, {status, QString::fromUtf8(reply->readAll())});
    });
}

void AntiCheat::loadScript(const QString &url, const QJSValue &callback)
{
    const QUrl target(url);
    if (!allowedHost(target)) {
        call(callback, {QStringLiteral("blocked")});
        return;
    }
    auto run = [this, url, callback](const QString &source) {
        const auto result = m_engine->evaluate(source, url);
        if (result.isError()) {
            qWarning().noquote() << "anti-cheat script error:" << result.toString();
            call(callback, {result.toString()});
        } else {
            call(callback, {QJSValue::NullValue});
        }
    };
    // The versioned SDK file never changes; the JSONP config does, so only cache the SDK.
    const bool cacheable = target.path().endsWith(".js") && !target.hasQuery();
    if (cacheable && m_scriptCache.contains(url)) {
        run(m_scriptCache.value(url));
        return;
    }
    QNetworkRequest request(target);
    request.setTransferTimeout(10000);
    request.setHeader(QNetworkRequest::UserAgentHeader, browserAgent);
    request.setRawHeader("Referer", "https://music.163.com/");
    auto *reply = m_network.get(request);
    QPointer<AntiCheat> self(this);
    QJSEngine *engine = m_engine;
    connect(reply, &QNetworkReply::finished, this, [self, engine, reply, url, cacheable, callback, run] {
        if (!self || engine != self->m_engine)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            self->call(callback, {reply->errorString()});
            return;
        }
        const auto source = QString::fromUtf8(reply->readAll());
        if (cacheable)
            self->m_scriptCache.insert(url, source);
        run(source);
    });
}

void AntiCheat::persist()
{
    if (!m_engine)
        return;
    const auto storage = m_engine->globalObject().property("__host").property("storage");
    QSaveFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(m_engine->fromScriptValue<QJsonObject>(storage)).toJson(QJsonDocument::Compact));
    file.commit();
}

void AntiCheat::token(Done done)
{
    m_waiting.append(std::move(done));
    if (m_waiting.size() > 1)
        return;   // a token is already being produced; share it
    m_deadline.start(24000);
    // A fresh SDK instance per token matches what a page load does. Yidun answers 430 to
    // rapid repeats, so space requests at least a few seconds apart.
    const qint64 wait = std::max<qint64>(0, m_lastToken + 4000 - QDateTime::currentMSecsSinceEpoch());
    QTimer::singleShot(int(wait), this, [this] {
        m_starting = false;
        start();
    });
}

void AntiCheat::start()
{
    if (m_starting)
        return;
    reset();
    ++m_run;
    m_starting = true;
    QFile shim(QStringLiteral(":/dun/assets/dun/browser-shim.js"));
    if (!shim.open(QIODevice::ReadOnly)) {
        fail();
        return;
    }
    m_engine = new QJSEngine(this);
    m_engine->installExtensions(QJSEngine::ConsoleExtension);
    // The engine must not take ownership of (and later delete) this object.
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
    auto host = m_engine->newQObject(this);
    QJsonObject saved;
    QFile stored(storagePath());
    if (stored.open(QIODevice::ReadOnly))
        saved = QJsonDocument::fromJson(stored.readAll()).object();
    host.setProperty("storage", m_engine->toScriptValue(saved));
    m_engine->globalObject().setProperty("__host", host);
    const auto shimResult = m_engine->evaluate(QString::fromUtf8(shim.readAll()), "browser-shim.js");
    if (shimResult.isError()) {
        qWarning().noquote() << "anti-cheat shim:" << shimResult.toString();
        fail();
        return;
    }

    QNetworkRequest request(loaderUrl);
    request.setTransferTimeout(10000);
    request.setHeader(QNetworkRequest::UserAgentHeader, browserAgent);
    auto *reply = m_network.get(request);
    QPointer<AntiCheat> self(this);
    QJSEngine *engine = m_engine;
    connect(reply, &QNetworkReply::finished, this, [self, engine, reply] {
        if (!self || engine != self->m_engine)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            self->fail();
            return;
        }
        const auto loaded = engine->evaluate(QString::fromUtf8(reply->readAll()), "tool.min.js");
        const auto init = engine->globalObject().property("initWatchman");
        if (loaded.isError() || !init.isCallable()) {
            self->fail();
            return;
        }
        auto options = engine->newObject();
        options.setProperty("auto", true);
        options.setProperty("productNumber", productNumber);
        auto onload = engine->evaluate(QStringLiteral(
            "(function (instance) { window.__watchman = instance; __host.loaded(true); })"));
        auto onerror = engine->evaluate(QStringLiteral("(function () { __host.loaded(false); })"));
        options.setProperty("onload", onload);
        options.setProperty("onerror", onerror);
        self->call(init, {options});
    });
}

void AntiCheat::ready(bool ok)
{
    if (!ok || !m_engine) {
        fail();
        return;
    }
    m_starting = false;
    m_instance = m_engine->globalObject().property("__watchman");
    const auto getToken = m_instance.property("getToken");
    const auto raw = m_instance.property("getInstance").callWithInstance(m_instance);
    if (!getToken.isCallable() || !raw.isObject()) {
        fail();
        return;
    }
    // Wait for the SDK's own collection step, then request the token.
    const auto deliver = m_engine->evaluate(QStringLiteral(
        "(function (instance, raw, business, run) {"
        "  raw.I(function () {"
        "    var sent = false;"
        "    instance.getToken(business, function (tk) {"
        "      if (sent) return; sent = true;"
        "      __host.deliver(run, typeof tk === 'string' ? tk : '');"
        "    });"
        "  });"
        "})"));
    call(deliver, {m_instance, raw, businessId, m_run});
}

void AntiCheat::loaded(bool ok)
{
    QTimer::singleShot(0, this, [this, ok] { ready(ok); });
}

void AntiCheat::deliver(int run, const QString &token)
{
    if (run != m_run || m_waiting.isEmpty() || !m_engine)
        return;
    m_deadline.stop();
    m_lastToken = QDateTime::currentMSecsSinceEpoch();
    const auto waiting = std::exchange(m_waiting, {});
    // Done with this instance; stop its background polling. A request made from inside
    // one of the callbacks below starts its own run after this.
    ++m_run;
    const int finished = m_run;
    QTimer::singleShot(0, this, [this, finished] {
        // Only tear down if no newer run has started meanwhile.
        if (m_run == finished && !m_starting)
            reset();
    });
    for (const auto &done : waiting)
        done(token);
}

void AntiCheat::fail()
{
    m_deadline.stop();
    const auto waiting = std::exchange(m_waiting, {});
    reset();   // start from scratch next time
    for (const auto &done : waiting)
        done({});
}
