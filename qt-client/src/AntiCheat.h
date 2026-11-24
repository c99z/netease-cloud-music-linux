#pragma once

#include <QHash>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <functional>

// Produces the NetEase Yidun anti-abuse token ("checkToken") that some account actions,
// such as subscribing to a playlist, require. It runs the official Yidun web SDK, fetched
// from NetEase's CDN at runtime, inside Qt's own JavaScript engine with a small browser
// shim (assets/dun/browser-shim.js); C++ supplies networking, timers and storage.
class AntiCheat : public QObject
{
    Q_OBJECT
public:
    using Done = std::function<void(const QString &token)>;

    explicit AntiCheat(QObject *parent = nullptr);
    ~AntiCheat() override;

    // Calls back with a fresh token, or an empty string if it could not be produced.
    void token(Done done);

    // Host functions called from the shim.
    Q_INVOKABLE int timer(int ms, const QJSValue &callback);
    Q_INVOKABLE void clearTimer(int id);
    Q_INVOKABLE void http(const QString &method, const QString &url, const QString &body,
                          const QJSValue &headers, const QJSValue &callback);
    Q_INVOKABLE void loadScript(const QString &url, const QJSValue &callback);
    Q_INVOKABLE void persist();
    Q_INVOKABLE double now() const;
    Q_INVOKABLE void loaded(bool ok);
    Q_INVOKABLE void deliver(int run, const QString &token);

private:
    void start();
    void ready(bool ok);
    void fail();
    void call(QJSValue function, const QJSValueList &arguments = {});
    void reset();

    QNetworkAccessManager m_network;
    QJSEngine *m_engine = nullptr;
    QJSValue m_instance;
    QHash<int, QTimer *> m_timers;
    QHash<QString, QString> m_scriptCache;
    QList<Done> m_waiting;
    QTimer m_deadline;
    int m_nextTimer = 0;
    qint64 m_lastToken = 0;
    int m_run = 0;          // increments for every SDK instance; stale callbacks are ignored
    bool m_starting = false;
};
