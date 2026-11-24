#include "Mpris.h"
#include "Library.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QUrl>

namespace {
const QString objectPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString playerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");
const QString noTrack = QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack");
// A position jump larger than this (against wall-clock progress) is reported as Seeked.
constexpr qint64 seekTolerance = 1200;

QString lrcStamp(qint64 ms)
{
    const qint64 centis = (ms % 1000) / 10;
    const qint64 seconds = (ms / 1000) % 60;
    const qint64 minutes = ms / 60000;
    return QStringLiteral("[%1:%2.%3]")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(centis, 2, 10, QLatin1Char('0'));
}
}

Mpris::Mpris(Library *library, std::function<void()> raise, std::function<void()> quit, QObject *parent)
    : QObject(parent), m_library(library), m_raise(std::move(raise)), m_quit(std::move(quit))
{
    m_clock.start();
    new MprisRootAdaptor(this);
    new MprisPlayerAdaptor(this);

    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    if (!bus.registerObject(objectPath, this, QDBusConnection::ExportAdaptors))
        return;
    m_service = QStringLiteral("org.mpris.MediaPlayer2.RecordRoom");
    if (!bus.registerService(m_service)) {
        // Another copy (e.g. a RECORD_ROOM_INSTANCE build) owns the plain name.
        m_service += QStringLiteral(".instance") + QString::number(QCoreApplication::applicationPid());
        if (!bus.registerService(m_service)) {
            m_service.clear();
            return;
        }
    }

    connect(library, &Library::queueChanged, this, &Mpris::followTrack);
    connect(library, &Library::lyricsChanged, this, &Mpris::followTrack);
    for (auto signal : {&Library::playbackChanged, &Library::playModeChanged, &Library::playbackSeekableChanged,
                        &Library::playbackDurationChanged})
        connect(library, signal, this, &Mpris::sync);
    connect(library->media(), &QMediaPlayer::playbackStateChanged, this, [this] {
        trackPosition();
        sync();
    });
    connect(library->audio(), &QAudioOutput::volumeChanged, this, &Mpris::sync);
    connect(library->audio(), &QAudioOutput::mutedChanged, this, &Mpris::sync);
    connect(library, &Library::playbackPositionChanged, this, &Mpris::trackPosition);
    followTrack();
}

Mpris::~Mpris()
{
    if (!m_service.isEmpty())
        QDBusConnection::sessionBus().unregisterService(m_service);
}

QString Mpris::playbackStatus() const
{
    if (m_library->media()->playbackState() == QMediaPlayer::PlayingState)
        return QStringLiteral("Playing");
    return m_library->queueIndex() < 0 ? QStringLiteral("Stopped") : QStringLiteral("Paused");
}

// Play modes: 0 list loop, 1 single loop, 2 shuffle (loops the list), 3 in order.
QString Mpris::loopStatus() const
{
    switch (m_library->playMode()) {
    case 1:
        return QStringLiteral("Track");
    case 3:
        return QStringLiteral("None");
    default:
        return QStringLiteral("Playlist");
    }
}

bool Mpris::shuffle() const
{
    return m_library->playMode() == 2;
}

double Mpris::volume() const
{
    const auto *audio = m_library->audio();
    return audio->isMuted() ? 0.0 : audio->volume();
}

qint64 Mpris::position() const
{
    return m_library->playbackPosition() * 1000;
}

QDBusObjectPath Mpris::trackPath() const
{
    const auto id = m_library->currentTrack().value("id").toString();
    return QDBusObjectPath(id.isEmpty() ? noTrack : QStringLiteral("/local/recordroom/track/") + id);
}

QVariantMap Mpris::plainMetadata() const
{
    const auto track = m_library->currentTrack();
    if (track.isEmpty())
        return {{QStringLiteral("mpris:trackid"), noTrack}};
    QStringList artists;
    for (const auto &name : track.value("artist").toString().split(QLatin1Char('/'), Qt::SkipEmptyParts))
        if (!name.trimmed().isEmpty())
            artists.append(name.trimmed());
    qint64 length = track.value("duration").toLongLong();
    if (length <= 0)
        length = m_library->playbackDuration();
    QVariantMap data{
        {QStringLiteral("mpris:trackid"), trackPath().path()},
        {QStringLiteral("xesam:title"), track.value("title").toString()},
        {QStringLiteral("xesam:artist"), artists},
        {QStringLiteral("xesam:album"), track.value("album").toString()},
        {QStringLiteral("mpris:length"), length * 1000},
        {QStringLiteral("xesam:url"), QStringLiteral("https://music.163.com/song?id=") + track.value("id").toString()},
    };
    const auto art = track.value("art").toUrl();
    if (art.isValid() && !art.isEmpty())
        data.insert(QStringLiteral("mpris:artUrl"), art.toString());
    // Left out while loading or on error, so lyric tools can fall back to their own search.
    if (m_lyricsReady)
        data.insert(QStringLiteral("xesam:asText"), m_lyricsText);
    return data;
}

QVariantMap Mpris::metadata() const
{
    auto data = plainMetadata();
    data.insert(QStringLiteral("mpris:trackid"),
                QVariant::fromValue(QDBusObjectPath(data.value("mpris:trackid").toString())));
    return data;
}

QVariantMap Mpris::properties() const
{
    return {
        {QStringLiteral("PlaybackStatus"), playbackStatus()},
        {QStringLiteral("LoopStatus"), loopStatus()},
        {QStringLiteral("Shuffle"), shuffle()},
        {QStringLiteral("Volume"), volume()},
        {QStringLiteral("Metadata"), plainMetadata()},
        {QStringLiteral("CanGoNext"), m_library->hasNext()},
        {QStringLiteral("CanGoPrevious"), m_library->hasPrevious() || m_library->canSeekPlayback()},
        {QStringLiteral("CanSeek"), m_library->canSeekPlayback()},
    };
}

// Announce whatever changed since the last call; Position is polled by clients, never announced.
void Mpris::sync()
{
    if (m_service.isEmpty())
        return;
    const auto now = properties();
    QVariantMap changed;
    for (auto it = now.cbegin(); it != now.cend(); ++it) {
        if (m_published.value(it.key()) != it.value())
            changed.insert(it.key(), it.key() == QLatin1String("Metadata") ? QVariant(metadata()) : it.value());
    }
    m_published = now;
    if (changed.isEmpty())
        return;
    auto message = QDBusMessage::createSignal(objectPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                              QStringLiteral("PropertiesChanged"));
    message << playerInterface << changed << QStringList{};
    QDBusConnection::sessionBus().send(message);
}

// New song: fetch its lyrics even when the now-playing page is closed. Lyrics loaded: publish them.
void Mpris::followTrack()
{
    const auto id = m_library->currentTrack().value("id").toString();
    if (id != m_trackId) {
        m_trackId = id;
        m_lyricsReady = false;
        m_lyricsText.clear();
        m_lastPosition = -1;
        if (!id.isEmpty())
            m_library->loadCurrentLyrics();
    }
    if (!id.isEmpty() && m_library->lyricsTrackId() == id && !m_library->lyricsBusy()
        && m_library->lyricsError().isEmpty() && !m_lyricsReady) {
        QStringList lines;
        for (const auto &value : m_library->lyrics()) {
            const auto line = value.toMap();
            const auto time = line.value("time").toLongLong();
            const auto words = line.value("text").toString();
            lines.append(time >= 0 ? lrcStamp(time) + words : words);
        }
        m_lyricsText = lines.join(QLatin1Char('\n'));
        m_lyricsReady = true;
    }
    sync();
}

// Seeks happen in several places (slider, lyrics, previous, stream recovery). Rather than hook
// each, compare the reported position with where steady playback would have put it.
void Mpris::trackPosition()
{
    if (m_service.isEmpty())
        return;
    const qint64 position = m_library->playbackPosition();
    const qint64 now = m_clock.elapsed();
    const bool playing = m_library->media()->playbackState() == QMediaPlayer::PlayingState;
    if (m_lastPosition >= 0) {
        const qint64 expected = m_lastPosition + (m_wasPlaying ? now - m_lastPositionAt : 0);
        if (qAbs(position - expected) > seekTolerance)
            emit seeked(position * 1000);
    }
    m_lastPosition = position;
    m_lastPositionAt = now;
    m_wasPlaying = playing;
}

MprisRootAdaptor::MprisRootAdaptor(Mpris *owner) : QDBusAbstractAdaptor(owner), m_owner(owner) {}

QString MprisRootAdaptor::identity() const
{
    const auto name = QGuiApplication::applicationDisplayName();
    return name.isEmpty() ? QCoreApplication::applicationName() : name;
}

QString MprisRootAdaptor::desktopEntry() const
{
    return QGuiApplication::desktopFileName();
}

void MprisRootAdaptor::Raise()
{
    m_owner->raise();
}

void MprisRootAdaptor::Quit()
{
    m_owner->quit();
}

MprisPlayerAdaptor::MprisPlayerAdaptor(Mpris *owner) : QDBusAbstractAdaptor(owner), m_owner(owner)
{
    connect(owner, &Mpris::seeked, this, &MprisPlayerAdaptor::Seeked);
}

void MprisPlayerAdaptor::setLoopStatus(const QString &status)
{
    if (status == QLatin1String("Track"))
        m_owner->library()->setPlayMode(1);
    else if (status == QLatin1String("None"))
        m_owner->library()->setPlayMode(3);
    else if (status == QLatin1String("Playlist") && m_owner->library()->playMode() != 2)
        m_owner->library()->setPlayMode(0);
}

void MprisPlayerAdaptor::setShuffle(bool on)
{
    if (on != m_owner->shuffle())
        m_owner->library()->setPlayMode(on ? 2 : 0);
}

void MprisPlayerAdaptor::setVolume(double value)
{
    auto *audio = m_owner->library()->audio();
    value = qBound(0.0, value, 1.0);
    audio->setVolume(float(value));
    if (value > 0 && audio->isMuted())
        audio->setMuted(false);
}

bool MprisPlayerAdaptor::canGoNext() const
{
    return m_owner->library()->hasNext();
}

bool MprisPlayerAdaptor::canGoPrevious() const
{
    return m_owner->library()->hasPrevious() || m_owner->library()->canSeekPlayback();
}

bool MprisPlayerAdaptor::canSeek() const
{
    return m_owner->library()->canSeekPlayback();
}

void MprisPlayerAdaptor::Next()
{
    m_owner->library()->next();
}

void MprisPlayerAdaptor::Previous()
{
    m_owner->library()->previous();
}

void MprisPlayerAdaptor::Pause()
{
    if (m_owner->playbackStatus() == QLatin1String("Playing"))
        m_owner->library()->togglePlayback();
}

void MprisPlayerAdaptor::PlayPause()
{
    m_owner->library()->togglePlayback();
}

void MprisPlayerAdaptor::Stop()
{
    Pause();
}

void MprisPlayerAdaptor::Play()
{
    if (m_owner->playbackStatus() != QLatin1String("Playing"))
        m_owner->library()->togglePlayback();
}

void MprisPlayerAdaptor::Seek(qlonglong offset)
{
    auto *library = m_owner->library();
    if (!library->canSeekPlayback())
        return;
    const qint64 target = library->playbackPosition() + offset / 1000;
    const qint64 length = library->playbackDuration();
    if (length > 0 && target >= length)
        library->next();
    else
        library->seekPlayback(qMax(qint64{0}, target));
}

void MprisPlayerAdaptor::SetPosition(const QDBusObjectPath &track, qlonglong position)
{
    auto *library = m_owner->library();
    if (track != m_owner->trackPath() || position < 0 || !library->canSeekPlayback())
        return;
    const qint64 length = library->playbackDuration();
    if (length > 0 && position / 1000 > length)
        return;
    library->seekPlayback(position / 1000);
}
