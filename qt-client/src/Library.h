#pragma once

#include "AudioCache.h"
#include "NeteaseApi.h"
#include "ResponseStore.h"
#include "TrackModel.h"

#include <QAudioOutput>
#include <QJsonObject>
#include <QtQml/qqmlregistration.h>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QSet>
#include <QTimer>
#include <QUrlQuery>
#include <QVariant>
#include <functional>

class Library : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList recommendations READ recommendations NOTIFY discoveryChanged)
    Q_PROPERTY(QVariantList homeShelves READ homeShelves NOTIFY discoveryChanged)
    Q_PROPERTY(QVariantList featuredCharts READ featuredCharts NOTIFY discoveryChanged)
    Q_PROPERTY(QVariantList newAlbums READ newAlbums NOTIFY discoveryChanged)
    Q_PROPERTY(QVariantList banners READ banners NOTIFY discoveryChanged)
    Q_PROPERTY(QVariantList recentListen READ recentListen NOTIFY discoveryChanged)
    Q_PROPERTY(QVariantList radarPlaylists READ radarPlaylists NOTIFY discoveryChanged)
    Q_PROPERTY(QStringList playlistCategories READ playlistCategories NOTIFY categoriesChanged)
    Q_PROPERTY(QVariantList playlistCategoryGroups READ playlistCategoryGroups NOTIFY categoriesChanged)
    Q_PROPERTY(QString artistInitial READ artistInitial NOTIFY categoriesChanged)
    Q_PROPERTY(int likesRevision READ likesRevision NOTIFY likesChanged)
    Q_PROPERTY(int playMode READ playMode NOTIFY playModeChanged)
    Q_PROPERTY(bool hasPrevious READ hasPrevious NOTIFY queueChanged)
    Q_PROPERTY(bool hasNext READ hasNext NOTIFY queueChanged)
    Q_PROPERTY(QString searchType READ searchType NOTIFY searchChanged)
    Q_PROPERTY(QVariantList searchItems READ searchItems NOTIFY searchChanged)
    Q_PROPERTY(QStringList searchSuggestions READ searchSuggestions NOTIFY suggestionsChanged)
    Q_PROPERTY(QStringList hotSearches READ hotSearches NOTIFY suggestionsChanged)
    Q_PROPERTY(QString playlistTab READ playlistTab NOTIFY playlistTabChanged)
    Q_PROPERTY(QVariantList playlistTabItems READ playlistTabItems NOTIFY playlistTabChanged)
    Q_PROPERTY(QString playlistTabText READ playlistTabText NOTIFY playlistTabChanged)
    Q_PROPERTY(bool playlistTabBusy READ playlistTabBusy NOTIFY playlistTabChanged)
    Q_PROPERTY(QVariantList similarSongs READ similarSongs NOTIFY similarChanged)
    Q_PROPERTY(QVariantList similarPlaylists READ similarPlaylists NOTIFY similarChanged)
    Q_PROPERTY(QString playlistCategory READ playlistCategory NOTIFY categoriesChanged)
    Q_PROPERTY(QVariantList freshTracks READ freshTracks NOTIFY discoveryChanged)
    Q_PROPERTY(bool discoveryBusy READ discoveryBusy NOTIFY discoveryChanged)
    Q_PROPERTY(QString discoveryError READ discoveryError NOTIFY discoveryChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchChanged)
    Q_PROPERTY(QString searchTerm READ searchTerm NOTIFY searchChanged)
    Q_PROPERTY(bool searchBusy READ searchBusy NOTIFY searchChanged)
    Q_PROPERTY(QString searchError READ searchError NOTIFY searchChanged)
    Q_PROPERTY(QVariantList playlistTracks READ playlistTracks NOTIFY playlistChanged)
    Q_PROPERTY(TrackModel *playlistModel READ playlistModel CONSTANT)
    Q_PROPERTY(QVariantMap playlist READ playlist NOTIFY playlistChanged)
    Q_PROPERTY(bool playlistHasMore READ playlistHasMore NOTIFY playlistChanged)
    Q_PROPERTY(bool playlistBusy READ playlistBusy NOTIFY playlistChanged)
    Q_PROPERTY(QString playlistError READ playlistError NOTIFY playlistChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(int queueIndex READ queueIndex NOTIFY queueChanged)
    Q_PROPERTY(QVariantMap currentTrack READ currentTrack NOTIFY queueChanged)
    Q_PROPERTY(bool playbackBusy READ playbackBusy NOTIFY playbackChanged)
    Q_PROPERTY(bool recoveringPlayback READ recoveringPlayback NOTIFY playbackChanged)
    Q_PROPERTY(qint64 playbackPosition READ playbackPosition NOTIFY playbackPositionChanged)
    Q_PROPERTY(qint64 playbackDuration READ playbackDuration NOTIFY playbackDurationChanged)
    Q_PROPERTY(QVariantList bufferedRanges READ bufferedRanges NOTIFY bufferedRangesChanged)
    Q_PROPERTY(QVariantList lyrics READ lyrics NOTIFY lyricsChanged)
    Q_PROPERTY(bool lyricsBusy READ lyricsBusy NOTIFY lyricsChanged)
    Q_PROPERTY(QString lyricsError READ lyricsError NOTIFY lyricsChanged)
    Q_PROPERTY(bool canSeekPlayback READ canSeekPlayback NOTIFY playbackSeekableChanged)
    Q_PROPERTY(QString playbackError READ playbackError NOTIFY playbackChanged)
    Q_PROPERTY(bool preview READ preview NOTIFY playbackChanged)
    Q_PROPERTY(bool currentLiked READ currentLiked NOTIFY likesChanged)
    Q_PROPERTY(bool roamPlaying READ roamPlaying NOTIFY playbackChanged)
    Q_PROPERTY(QString podcastCategoryName READ podcastCategoryName NOTIFY sectionChanged)
    Q_PROPERTY(QString sceneName READ sceneName NOTIFY sectionChanged)
    Q_PROPERTY(QString curatedArea READ curatedArea NOTIFY categoriesChanged)
    Q_PROPERTY(QString backendStatus READ backendStatus NOTIFY backendChanged)
    Q_PROPERTY(bool backendReady READ backendReady NOTIFY backendChanged)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY accountChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY accountChanged)
    Q_PROPERTY(bool localAccountSupported READ localAccountSupported NOTIFY backendChanged)
    Q_PROPERTY(QVariantList createdPlaylists READ createdPlaylists NOTIFY personalListsChanged)
    Q_PROPERTY(QVariantList savedPlaylists READ savedPlaylists NOTIFY personalListsChanged)
    Q_PROPERTY(QVariantMap likedPlaylist READ likedPlaylist NOTIFY personalListsChanged)
    Q_PROPERTY(QUrl loginImage READ loginImage NOTIFY loginChanged)
    Q_PROPERTY(QString loginStatus READ loginStatus NOTIFY loginChanged)
    Q_PROPERTY(bool loginBusy READ loginBusy NOTIFY loginChanged)
    Q_PROPERTY(QString section READ section NOTIFY sectionChanged)
    Q_PROPERTY(QVariantList sectionItems READ sectionItems NOTIFY sectionChanged)
    Q_PROPERTY(QVariantList sectionShelves READ sectionShelves NOTIFY sectionChanged)
    Q_PROPERTY(QString artistType READ artistType NOTIFY categoriesChanged)
    Q_PROPERTY(QVariantList sectionTracks READ sectionTracks NOTIFY sectionChanged)
    Q_PROPERTY(bool sectionBusy READ sectionBusy NOTIFY sectionChanged)
    Q_PROPERTY(bool sectionNeedsLogin READ sectionNeedsLogin NOTIFY sectionChanged)
    Q_PROPERTY(QString sectionError READ sectionError NOTIFY sectionChanged)
    Q_PROPERTY(QVariantList outputDevices READ outputDevices NOTIFY outputChanged)
    Q_PROPERTY(QString outputName READ outputName NOTIFY outputChanged)
    Q_PROPERTY(QString outputId READ outputId NOTIFY outputChanged)
    Q_PROPERTY(QMediaPlayer *media READ media CONSTANT)
    Q_PROPERTY(QAudioOutput *audio READ audio CONSTANT)
    QML_ELEMENT
    QML_UNCREATABLE("Supplied by the application")

public:
    explicit Library(QObject *parent = nullptr);
    ~Library() override;
    void start();

    QVariantList recommendations() const { return m_recommendations; }
    QVariantList homeShelves() const;
    QVariantList featuredCharts() const { return m_featuredCharts; }
    QVariantList newAlbums() const { return m_newAlbums; }
    QStringList playlistCategories() const { return m_playlistCategories; }
    QVariantList banners() const { return m_banners; }
    QVariantList recentListen() const { return m_recentListen; }
    QVariantList radarPlaylists() const;
    QVariantList playlistCategoryGroups() const { return m_playlistCategoryGroups; }
    QString artistInitial() const { return m_artistInitial; }
    int likesRevision() const { return m_likesRevision; }
    int playMode() const { return m_playMode; }
    bool hasPrevious() const;
    bool hasNext() const;
    QString searchType() const { return m_searchType; }
    QVariantList searchItems() const { return m_searchItems; }
    QStringList searchSuggestions() const { return m_searchSuggestions; }
    QStringList hotSearches() const { return m_hotSearches; }
    QString playlistTab() const { return m_playlistTab; }
    QVariantList playlistTabItems() const { return m_playlistTabItems; }
    QString playlistTabText() const { return m_playlistTabText; }
    bool playlistTabBusy() const { return m_playlistTabBusy; }
    QVariantList similarSongs() const { return m_similarSongs; }
    QVariantList similarPlaylists() const { return m_similarPlaylists; }
    QString playlistCategory() const { return m_playlistCategory; }
    QVariantList freshTracks() const { return m_freshTracks; }
    bool discoveryBusy() const { return m_discoveryBusy; }
    QString discoveryError() const { return m_discoveryError; }
    QVariantList searchResults() const { return m_searchResults; }
    QString searchTerm() const { return m_searchTerm; }
    bool searchBusy() const { return m_searchBusy; }
    QString searchError() const { return m_searchError; }
    QVariantList playlistTracks() const { return m_playlistTracks; }
    TrackModel *playlistModel() { return &m_playlistModel; }
    QVariantMap playlist() const { return m_playlist; }
    bool playlistHasMore() const { return m_playlistHasMore; }
    bool currentLiked() const;
    bool roamPlaying() const { return m_roamPlaying; }
    QString sceneName() const { return m_sceneName; }
    QString curatedArea() const { return m_curatedArea; }
    QString podcastCategoryName() const { return m_podcastCategoryName; }
    bool playlistBusy() const { return m_playlistBusy; }
    QString playlistError() const { return m_playlistError; }
    QVariantList queue() const { return m_queue; }
    int queueIndex() const { return m_queueIndex; }
    QVariantMap currentTrack() const;
    bool playbackBusy() const { return m_playbackBusy; }
    bool recoveringPlayback() const { return m_playbackBusy && m_resumePosition >= 0; }
    bool canSeekPlayback() const;
    qint64 playbackPosition() const { return m_resumePosition >= 0 ? m_resumePosition : m_media.position() + m_positionBase; }
    qint64 playbackDuration() const { return m_durationBeforeSeek > 0 ? m_durationBeforeSeek : m_media.duration(); }
    QVariantList bufferedRanges() const;
    QVariantList lyrics() const { return m_lyrics; }
    bool lyricsBusy() const { return m_lyricsBusy; }
    QString lyricsError() const { return m_lyricsError; }
    QString lyricsTrackId() const { return m_lyricsTrackId; }
    QString playbackError() const { return m_playbackError; }
    bool preview() const { return m_preview; }
    QString backendStatus() const { return m_backendStatus; }
    bool backendReady() const { return m_nativeApi || m_api.isValid(); }
    QVariantMap profile() const { return m_profile; }
    bool loggedIn() const { return !m_profile.isEmpty(); }
    bool localAccountSupported() const { return m_nativeApi; }
    QVariantList createdPlaylists() const { return m_createdPlaylists; }
    QVariantList savedPlaylists() const { return m_savedPlaylists; }
    QVariantMap likedPlaylist() const { return m_likedPlaylist; }
    QUrl loginImage() const { return m_loginImage; }
    QString loginStatus() const { return m_loginStatus; }
    bool loginBusy() const { return m_loginBusy; }
    QString section() const { return m_section; }
    QVariantList sectionShelves() const { return m_sectionShelves; }
    QString artistType() const { return m_artistType; }
    QVariantList sectionItems() const { return m_sectionItems; }
    QVariantList sectionTracks() const { return m_sectionTracks; }
    bool sectionBusy() const { return m_sectionBusy; }
    bool sectionNeedsLogin() const { return m_sectionNeedsLogin; }
    QString sectionError() const { return m_sectionError; }
    QVariantList outputDevices() const;
    QString outputName() const;
    QString outputId() const { return m_outputId; }
    QMediaPlayer *media() { return &m_media; }
    QAudioOutput *audio() { return &m_audio; }

    Q_INVOKABLE void refreshDiscovery(bool force = false);
    Q_INVOKABLE void search(const QString &term, const QString &type = QStringLiteral("1"));
    Q_INVOKABLE void suggest(const QString &term);
    Q_INVOKABLE void loadHotSearches();
    Q_INVOKABLE bool isLiked(const QString &id) const;
    Q_INVOKABLE void toggleLikeTrack(const QString &id);
    Q_INVOKABLE void playNext(const QVariantMap &track);
    Q_INVOKABLE void removeFromQueue(int index);
    Q_INVOKABLE void clearQueue();
    Q_INVOKABLE void cyclePlayMode();
    void setPlayMode(int mode);
    Q_INVOKABLE void loadPlaylistTab(const QString &tab);
    Q_INVOKABLE void togglePlaylistSubscribe();
    Q_INVOKABLE void addToPlaylist(const QString &playlistId, const QString &trackId);
    Q_INVOKABLE void removeFromPlaylist(const QString &trackId);
    Q_INVOKABLE void startRoam();
    Q_INVOKABLE void playSongById(const QString &id);
    Q_INVOKABLE void selectArtistInitial(const QString &initial);
    Q_INVOKABLE void loadSimilar();
    Q_INVOKABLE void prefetchCollection(const QVariantMap &playlist);
    Q_INVOKABLE void prefetchSection(const QString &section);    Q_INVOKABLE void openPlaylist(const QVariantMap &playlist, bool playNow = false);
    Q_INVOKABLE void playAll(const QVariantList &tracks);
    Q_INVOKABLE void loadMorePlaylist();
    Q_INVOKABLE void browsePodcastCategory(const QVariantMap &category);
    Q_INVOKABLE void browseSceneCategory(const QVariantMap &category);
    Q_INVOKABLE void playSection(int startIndex);
    Q_INVOKABLE void toggleLike();
    Q_INVOKABLE void dislikeRoamTrack();
    Q_INVOKABLE void playNewMusic();
    Q_INVOKABLE void browse(const QString &section);
    Q_INVOKABLE void selectPlaylistCategory(const QString &category);
    Q_INVOKABLE void selectCuratedArea(const QString &area);
    Q_INVOKABLE void selectArtistType(const QString &type);
    Q_INVOKABLE void beginQrLogin();
    Q_INVOKABLE void closeQrLogin();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void createPlaylist(const QString &name);
    Q_INVOKABLE void selectOutput(const QString &id);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void playCollection(const QVariantList &tracks, int index);
    Q_INVOKABLE void playIndex(int index);
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void switchAudioStream();
    Q_INVOKABLE void seekPlayback(qint64 position);
    Q_INVOKABLE void loadCurrentLyrics(bool retry = false);
    Q_INVOKABLE void previous();
    Q_INVOKABLE void next();

signals:
    void discoveryChanged();
    void categoriesChanged();
    void likesChanged();
    void playModeChanged();
    void suggestionsChanged();
    void playlistTabChanged();
    void similarChanged();
    void searchChanged();
    void playlistChanged();
    void queueChanged();
    void playbackChanged();
    void playbackPositionChanged();
    void playbackDurationChanged();
    void bufferedRangesChanged();
    void lyricsChanged();
    void playbackSeekableChanged();
    void backendChanged();
    void accountChanged();
    void personalListsChanged();
    void loginChanged();
    void sectionChanged();
    void outputChanged();
    void notification(const QString &message);

private:
    using Response = std::function<void(const QJsonObject &, const QString &)>;
    void get(const QString &route, QUrlQuery parameters, Response onResult, bool post = false);
    // Cached GET: answers from memory while fresh and merges concurrent identical requests.
    void fetch(const QString &route, const QUrlQuery &parameters, int maxAgeSeconds, Response onResult);
    void collectionRequest(const QVariantMap &playlist, int offset, QString &route, QUrlQuery &args) const;
    void dropCachedResponses();
    void loadSession(bool afterLogin = false, bool initial = false);
    void loadPersonalLists();
    void pollLogin();
    void setLoginStatus(const QString &message, bool busy);
    void rememberCurrentTrack();
    void loadLikedSongs();
    void extendRoamQueue();
    void requestCollection(const QVariantMap &playlist, int revision, bool playNow, int offset = 0);
    void finishSection(int revision, const QVariantList &items, const QVariantList &tracks, const QString &error);
    void startQueueIndex(int index);
    void requestTrackSource(qint64 resumePosition, bool playWhenReady = true, bool keepCurrent = false);
    void resolveAudio(const QString &id, int bitrate, AudioCache::SourceReady ready);
    void prefetchNextTrack();
    enum class Step { Manual, Automatic, Skip };
    int stepIndex(int direction, Step step);
    void advance(bool automatic);
    void resetShuffle(bool keepCurrentFirst = true);
    void loadCollectionDetail(const QVariantMap &playlist, int revision);
    void clearPreparedAudio();
    void handlePlaybackError(QMediaPlayer::Error error);
    void applyOutput();
    void backendFailed(const QString &message);
    void finishDiscoveryRequest(int revision, const QString &error);
    void fetchRecommendedPlaylists(const QString &route, int revision);
    void loadPlaylistCategories();

    QNetworkAccessManager m_network;
    AudioCache m_audioCache;
    QTimer m_loginPoll;
    QTimer m_bufferUpdate;
    QMediaDevices m_devices;
    QUrl m_api;
    QString m_backendStatus = QStringLiteral("正在连接");
    QAudioOutput m_audio;
    AudioCache::Asset m_playingAudio;
    QMediaPlayer m_media;

    NeteaseApi m_neteaseApi;
    bool m_nativeApi = false;
    QVariantMap m_profile;
    QVariantMap m_likedPlaylist;
    QVariantList m_createdPlaylists;
    QVariantList m_savedPlaylists;
    QVariantList m_recentTracks;
    QVariantList m_sectionItems;
    QVariantList m_sectionShelves;
    QString m_artistType = QStringLiteral("全部");
    QVariantList m_sectionTracks;
    QString m_section;
    QString m_sectionError;
    QString m_loginStatus;
    QString m_outputId;
    QUrl m_loginImage;
    bool m_loginBusy = false;
    bool m_sessionLoading = false;
    bool m_personalListsLoaded = false;
    bool m_personalListsBusy = false;
    bool m_sectionBusy = false;
    bool m_sectionNeedsLogin = false;
    bool m_skipUnavailable = false;
    int m_loginRevision = 0;
    int m_skipsRemaining = 0;
    int m_skipDirection = 1;
    int m_sectionRevision = 0;
    int m_personalRevision = 0;

    QVariantList m_homeShelves;
    QVariantList m_banners;
    QVariantList m_recentListen;
    QVariantList m_playlistCategoryGroups;
    QString m_artistInitial = QStringLiteral("热门");
    int m_likesRevision = 0;
    int m_playMode = 0;
    QList<int> m_shuffleOrder;
    struct CachedResponse {
        QJsonObject body;
        qint64 storedAt = 0;
    };
    QHash<QString, CachedResponse> m_responseCache;
    ResponseStore m_store;
    QList<QString> m_responseOrder;
    QHash<QString, QList<Response>> m_inflight;
    QSet<QString> m_prefetched;
    bool m_bypassCache = false;    // a user-requested refresh: skip saved copies, go to the network
    bool m_revalidating = false;   // a callback is delivering refreshed data for a stale copy
    QString m_searchType = QStringLiteral("1");
    QVariantList m_searchItems;
    QStringList m_searchSuggestions;
    QStringList m_hotSearches;
    int m_suggestRevision = 0;
    QString m_playlistTab = QStringLiteral("tracks");
    QVariantList m_playlistTabItems;
    QString m_playlistTabText;
    bool m_playlistTabBusy = false;
    QVariantList m_similarSongs;
    QVariantList m_similarPlaylists;
    QString m_similarTrackId;
    QVariantList m_radarShelves;
    QVariantList m_recommendations;
    QSet<QString> m_likedIds;
    QString m_podcastCategory;
    QString m_podcastCategoryName;
    QString m_sceneCategory;
    QString m_sceneName;
    QString m_curatedArea = QStringLiteral("全部");
    bool m_playlistHasMore = false;
    int m_playlistOffset = 0;
    bool m_roamPlaying = false;
    QVariantList m_featuredCharts;
    QVariantList m_newAlbums;
    QStringList m_playlistCategories{QStringLiteral("全部")};
    QString m_playlistCategory = QStringLiteral("全部");
    bool m_categoriesBusy = false;
    QVariantList m_freshTracks;
    QVariantList m_searchResults;
    QVariantList m_playlistTracks;
    TrackModel m_playlistModel;
    QVariantList m_queue;
    QVariantList m_lyrics;
    QVariantMap m_playlist;
    QString m_discoveryError;
    QString m_searchTerm;
    QString m_searchError;
    QString m_playlistError;
    QString m_playbackError;
    QString m_lyricsTrackId;
    QString m_lyricsError;
    bool m_discoveryBusy = false;
    bool m_searchBusy = false;
    bool m_playlistBusy = false;
    bool m_playbackBusy = false;
    bool m_lyricsBusy = false;
    bool m_recoveryScheduled = false;
    bool m_wantsPlayback = false;
    qint64 m_resumePosition = -1;
    qint64 m_positionBase = 0;
    qint64 m_durationBeforeSeek = 0;
    int m_streamRetries = 0;
    int m_streamBitrate = 0;
    bool m_preview = false;
    int m_pendingDiscovery = 0;
    int m_discoveryRevision = 0;
    int m_searchRevision = 0;
    int m_playlistRevision = 0;
    int m_playbackRevision = 0;
    int m_lyricsRevision = 0;
    int m_queueIndex = -1;
};
