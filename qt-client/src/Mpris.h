#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <functional>

class Library;

// Publishes the player on the session bus as an MPRIS 2 media player
// (https://specifications.freedesktop.org/mpris-spec/latest/). GNOME's media
// controls, desktop-lyric, osdlyrics and media keys all read playback from here.
// The current song's timed lyrics ride along as LRC text in xesam:asText.
class Mpris : public QObject
{
    Q_OBJECT
public:
    Mpris(Library *library, std::function<void()> raise, std::function<void()> quit, QObject *parent = nullptr);
    ~Mpris() override;

    Library *library() const { return m_library; }
    void raise() { m_raise(); }
    void quit() { m_quit(); }
    QString playbackStatus() const;
    QString loopStatus() const;
    bool shuffle() const;
    double volume() const;
    QVariantMap metadata() const;   // D-Bus ready: mpris:trackid as an object path
    qint64 position() const;        // microseconds
    QDBusObjectPath trackPath() const;

signals:
    void seeked(qlonglong position);

private:
    QVariantMap properties() const;
    QVariantMap plainMetadata() const;
    void sync();
    void trackPosition();
    void followTrack();

    Library *m_library;
    std::function<void()> m_raise;
    std::function<void()> m_quit;
    QString m_service;
    QVariantMap m_published;
    QString m_trackId;
    QString m_lyricsText;
    bool m_lyricsReady = false;
    QElapsedTimer m_clock;
    qint64 m_lastPosition = -1;
    qint64 m_lastPositionAt = 0;
    bool m_wasPlaying = false;
};

class MprisRootAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool Fullscreen READ fullscreen)
    Q_PROPERTY(bool CanSetFullscreen READ canSetFullscreen)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)
public:
    explicit MprisRootAdaptor(Mpris *owner);
    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool fullscreen() const { return false; }
    bool canSetFullscreen() const { return false; }
    bool hasTrackList() const { return false; }
    QString identity() const;
    QString desktopEntry() const;
    QStringList supportedUriSchemes() const { return {}; }
    QStringList supportedMimeTypes() const { return {}; }

public slots:
    void Raise();
    void Quit();

private:
    Mpris *m_owner;
};

class MprisPlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ rate)
    Q_PROPERTY(double MaximumRate READ rate)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)
public:
    explicit MprisPlayerAdaptor(Mpris *owner);
    QString playbackStatus() const { return m_owner->playbackStatus(); }
    QString loopStatus() const { return m_owner->loopStatus(); }
    void setLoopStatus(const QString &status);
    double rate() const { return 1.0; }
    void setRate(double) {}
    bool shuffle() const { return m_owner->shuffle(); }
    void setShuffle(bool on);
    QVariantMap metadata() const { return m_owner->metadata(); }
    double volume() const { return m_owner->volume(); }
    void setVolume(double value);
    qlonglong position() const { return m_owner->position(); }
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canPlay() const { return true; }
    bool canPause() const { return true; }
    bool canSeek() const;
    bool canControl() const { return true; }

public slots:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &track, qlonglong position);
    void OpenUri(const QString &) {}

signals:
    void Seeked(qlonglong Position);

private:
    Mpris *m_owner;
};
