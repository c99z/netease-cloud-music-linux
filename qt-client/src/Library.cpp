#include "Library.h"
#include "AudioStream.h"

#include <QAudioDevice>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QMediaTimeRange>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QDateTime>
#include <QRandomGenerator>
#include <QPointer>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <algorithm>
#include <utility>

namespace {
constexpr int normalStreamBitrate = 192000;
constexpr int fallbackStreamBitrate = 128000;

QString text(const QJsonObject &value, const char *key)
{
    return value.value(QLatin1String(key)).toString();
}

QString idOf(const QJsonObject &value)
{
    const auto id = value.value("id");
    if (id.isString())
        return id.toString();
    const auto number = id.toInteger();
    return number > 0 ? QString::number(number) : QString{};
}

QUrl coverUrl(const QString &value)
{
    QUrl url(value);
    if (url.scheme() != "https" && url.scheme() != "http")
        return {};
    if (url.host().endsWith(".music.126.net")) {
        url.setScheme("https");
        QUrlQuery query(url);
        if (query.isEmpty() || (query.queryItems().size() == 1 && query.hasQueryItem("param"))) {
            query.removeAllQueryItems("param");
            query.addQueryItem("param", "400y400");
            url.setQuery(query);
        }
    }
    return url;
}

QVariantMap trackFrom(const QJsonObject &item)
{
    QJsonObject nested = item.value("song").toObject();
    if (nested.isEmpty())
        nested = item.value("data").toObject();
    const QJsonObject song = nested.isEmpty() ? item : nested;
    QVariantMap track;
    const QString id = idOf(song);
    if (id.isEmpty() || text(song, "name").isEmpty())
        return track;

    QJsonArray artists = song.value("ar").toArray();
    if (artists.isEmpty())
        artists = song.value("artists").toArray();
    QStringList artistNames;
    for (const auto &entry : artists) {
        const auto name = text(entry.toObject(), "name");
        if (!name.isEmpty())
            artistNames.append(name);
    }
    QJsonObject album = song.value("al").toObject();
    if (album.isEmpty())
        album = song.value("album").toObject();
    const QString artwork = text(album, "picUrl");
    const auto duration = song.value("dt").toInteger(song.value("duration").toInteger());

    track.insert("id", id);
    track.insert("title", text(song, "name"));
    track.insert("artist", artistNames.isEmpty() ? QStringLiteral("未知艺人") : artistNames.join(" / "));
    track.insert("album", text(album, "name"));
    track.insert("art", coverUrl(artwork.isEmpty() ? text(item, "picUrl") : artwork));
    track.insert("duration", duration);
    track.insert("artistId", artists.isEmpty() ? QString{} : idOf(artists.first().toObject()));
    track.insert("albumId", idOf(album));
    track.insert("vip", song.value("fee").toInt() == 1);
    track.insert("original", song.value("originCoverType").toInt() == 1);
    track.insert("sq", song.value("sq").toObject().value("size").toInteger() > 0);
    return track;
}

QVariantList tracksFrom(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &item : items) {
        const auto track = trackFrom(item.toObject());
        if (!track.isEmpty())
            result.append(track);
    }
    return result;
}

struct LyricWord {
    qint64 time = 0;
    qint64 duration = 0;
    int offset = 0;
    int length = 0;
};

struct LyricLineData {
    qint64 time = 0;
    qint64 duration = 0;
    QString text;
    QList<LyricWord> words;
};

struct LyricText {
    QMap<qint64, QString> timed;
    QList<LyricLineData> yrcLines;
    QStringList plain;
};

QList<LyricLineData> parseYrc(const QString &source)
{
    static const QRegularExpression linePattern(QStringLiteral(R"(^\[(\d+),(\d+)\](.*)$)"));
    static const QRegularExpression wordPattern(QStringLiteral(R"(\((\d+),(\d+),\d+\)([^\(]*))"));
    QList<LyricLineData> result;
    for (const auto &raw : source.split(u'\n')) {
        const auto line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(u'{'))
            continue;
        const auto m = linePattern.match(line);
        if (!m.hasMatch())
            continue;
        LyricLineData entry;
        entry.time = m.captured(1).toLongLong();
        entry.duration = m.captured(2).toLongLong();
        const QString body = m.captured(3);
        auto it = wordPattern.globalMatch(body);
        while (it.hasNext()) {
            const auto wm = it.next();
            const QString chunk = wm.captured(3);
            LyricWord word;
            word.time = wm.captured(1).toLongLong();
            word.duration = wm.captured(2).toLongLong();
            word.offset = int(entry.text.size());
            word.length = int(chunk.size());
            entry.text += chunk;
            if (word.length > 0 || word.duration > 0)
                entry.words.append(word);
        }
        const auto trimmed = entry.text.trimmed();
        if (trimmed.isEmpty())
            continue;
        // Adjust word offsets if leading whitespace was trimmed.
        const int leading = int(entry.text.indexOf(trimmed));
        if (leading > 0) {
            for (auto &w : entry.words)
                w.offset = qMax(0, w.offset - leading);
        }
        entry.text = trimmed;
        result.append(entry);
    }
    std::sort(result.begin(), result.end(), [](const LyricLineData &a, const LyricLineData &b) {
        return a.time < b.time;
    });
    return result;
}

LyricText parseLyrics(const QString &source)
{
    static const QRegularExpression timestamp(QStringLiteral(R"(\[(\d{1,4}):([0-5]?\d)(?:[.:](\d{1,3}))?\])"));
    static const QRegularExpression offsetTag(QStringLiteral(R"(\[offset:\s*([+-]?\d{1,7})\s*\])"),
                                              QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression metadata(QStringLiteral(R"(^\[[A-Za-z]+:.*\]$)"));
    const auto offset = offsetTag.match(source).captured(1).toLongLong();
    LyricText result;
    for (const auto &raw : source.split(u'\n')) {
        const auto line = raw.trimmed();
        QList<qint64> times;
        qsizetype cursor = 0;
        while (true) {
            const auto tag = timestamp.match(line, cursor, QRegularExpression::NormalMatch,
                                             QRegularExpression::AnchorAtOffsetMatchOption);
            if (!tag.hasMatch())
                break;
            const auto fraction = tag.captured(3).leftJustified(3, u'0').toInt();
            const auto time = (tag.captured(1).toLongLong() * 60 + tag.captured(2).toInt()) * 1000 + fraction;
            times.append(qMax(qint64{0}, time - offset));
            cursor = tag.capturedEnd();
        }
        const auto words = line.mid(cursor).trimmed();
        if (words.isEmpty())
            continue;
        if (times.isEmpty() && !metadata.match(words).hasMatch())
            result.plain.append(words);
        for (auto time : times) {
            auto &entry = result.timed[time];
            if (!entry.isEmpty())
                entry += u'\n';
            entry += words;
        }
    }
    return result;
}

QVariantList playlistsFrom(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &value : items) {
        const auto item = value.toObject();
        const QString id = idOf(item);
        const QString title = text(item, "name");
        if (id.isEmpty() || title.isEmpty())
            continue;
        const QString photo = text(item, "picUrl");
        const auto creatorId = item.value("creator").toObject().value("userId");
        const auto ownerId = creatorId.isUndefined() ? item.value("userId") : creatorId;
        result.append(QVariantMap{
            {"id", id},
            {"title", title},
            {"kind", "playlist"},
            {"art", coverUrl(photo.isEmpty() ? text(item, "coverImgUrl") : photo)},
            {"specialType", item.value("specialType").toInt()},
            {"ownerId", idOf(QJsonObject{{"id", ownerId}})},
            {"description", text(item, "copywriter")},
            {"creatorName", text(item.value("creator").toObject(), "nickname")},
            {"trackCount", item.value("trackCount").toInt()},
            {"playCount", item.value("playCount").toInteger(item.value("playcount").toInteger())},
        });
    }
    return result;
}

QString dateText(qint64 milliseconds)
{
    return milliseconds > 0 ? QDateTime::fromMSecsSinceEpoch(milliseconds).toString("yyyy-MM-dd") : QString{};
}

// Chart categories as the web client groups them: one group lists the top three
// songs of every chart, the others are cover tiles.
QVariantList chartGroupsFrom(const QJsonArray &categories)
{
    QVariantList result;
    for (const auto &value : categories) {
        const auto category = value.toObject();
        const bool rows = text(category, "displayType") == "TOP_3";
        QVariantList charts;
        for (const auto &entry : category.value("list").toArray()) {
            const auto item = entry.toObject();
            const auto id = idOf(item);
            if (id.isEmpty() || text(item, "targetType") != "PLAYLIST" || text(item, "name").isEmpty())
                continue;
            QVariantList preview;
            for (const auto &row : item.value("trackRankList").toArray()) {
                const auto track = row.toObject();
                const int rank = track.value("rank").toInt();
                const int last = track.value("lastRank").toInt();
                preview.append(QVariantMap{
                    {"title", text(track, "songName")}, {"artist", text(track, "artistName")},
                    {"trend", last <= 0 ? "new" : last > rank ? "up" : last < rank ? "down" : "flat"},
                });
                if (preview.size() == 3)
                    break;
            }
            const auto tile = text(item, "firstCoverUrl");
            const auto song = text(item, "songCoverImgUrl");
            charts.append(QVariantMap{
                {"id", id}, {"title", text(item, "name")}, {"kind", "chart"},
                {"art", coverUrl(rows && !song.isEmpty() ? song : text(item, "coverUrl"))},
                {"tile", coverUrl(tile.isEmpty() ? text(item, "coverUrl") : tile)},
                {"description", text(item, "updateFrequency")},
                {"preview", preview},
            });
        }
        if (!charts.isEmpty())
            result.append(QVariantMap{{"title", text(category, "name")},
                                      {"kind", rows ? "chartRows" : "chartTiles"}, {"items", charts}});
    }
    return result;
}

QVariantList commentsFrom(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &value : items) {
        const auto item = value.toObject();
        const auto user = item.value("user").toObject();
        QString reply;
        const auto replied = item.value("beReplied").toArray();
        if (!replied.isEmpty()) {
            const auto first = replied.first().toObject();
            reply = QStringLiteral("@%1: %2").arg(text(first.value("user").toObject(), "nickname"),
                                                  text(first, "content"));
        }
        result.append(QVariantMap{
            {"kind", "comment"}, {"title", text(user, "nickname")},
            {"art", coverUrl(text(user, "avatarUrl"))}, {"text", text(item, "content")},
            {"reply", reply}, {"time", text(item, "timeStr")},
            {"likes", item.value("likedCount").toInt()},
        });
    }
    return result;
}

QVariantList albumsFrom(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &value : items) {
        const auto item = value.toObject();
        const auto id = idOf(item);
        if (id.isEmpty() || text(item, "name").isEmpty())
            continue;
        const auto artist = text(item.value("artist").toObject(), "name");
        result.append(QVariantMap{
            {"id", id}, {"title", text(item, "name")}, {"kind", "album"},
            {"art", coverUrl(text(item, "picUrl"))},
            {"description", artist},
            {"createTime", dateText(item.value("publishTime").toInteger())},
            {"trackCount", item.value("size").toInt()},
        });
    }
    return result;
}

QVariantList radiosFrom(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &value : items) {
        const auto item = value.toObject();
        const auto id = idOf(item);
        if (id.isEmpty() || text(item, "name").isEmpty())
            continue;
        result.append(QVariantMap{
            {"id", id}, {"title", text(item, "name")}, {"kind", "radio"},
            {"art", coverUrl(text(item, "picUrl"))},
            {"description", text(item, "desc")},
            {"trackCount", item.value("programCount").toInt()},
        });
    }
    return result;
}

QVariantList episodesFrom(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &value : items) {
        auto item = value.toObject();
        QJsonObject song = item.value("mainSong").toObject();
        auto track = trackFrom(QJsonObject{{"song", song}, {"picUrl", item.value("coverUrl")}});
        if (track.isEmpty())
            continue;
        track["artist"] = item.value("dj").toObject().value("nickname").toString(QStringLiteral("播客节目"));
        track["album"] = item.value("radio").toObject().value("name").toString();
        result.append(track);
    }
    return result;
}

QVariantList peopleFrom(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &value : items) {
        const auto item = value.toObject();
        const auto id = idOf(QJsonObject{{"id", item.value("userId")}});
        if (!id.isEmpty())
            result.append(QVariantMap{
                {"id", id}, {"title", text(item, "nickname")}, {"kind", "person"},
                {"art", coverUrl(text(item, "avatarUrl"))}, {"description", text(item, "signature")},
            });
    }
    return result;
}

QJsonObject unwrap(const QJsonObject &value)
{
    const auto data = value.value("data").toObject();
    return data.isEmpty() ? value : data;
}

QString elementTitle(const QJsonObject &object, bool subtitle = false)
{
    const auto element = object.value("uiElement").toObject();
    const auto main = text(element.value("mainTitle").toObject(), "title");
    const auto sub = text(element.value("subTitle").toObject(), "title");
    return subtitle && !sub.isEmpty() ? sub : !main.isEmpty() ? main : sub;
}

QString elementImage(const QJsonObject &object)
{
    const auto ui = object.value("uiElement").toObject();
    const auto image = text(ui.value("image").toObject(), "imageUrl");
    const auto images = ui.value("images").toArray();
    return image.isEmpty() && !images.isEmpty() ? text(images.first().toObject(), "imageUrl") : image;
}

QVariantMap programFrom(const QJsonObject &item, const QJsonObject &resource = {})
{
    auto program = unwrap(item);
    for (const auto &key : {"djProgram", "program", "voice"}) {
        if (program.value(QLatin1String(key)).isObject()) {
            program = program.value(QLatin1String(key)).toObject();
            break;
        }
    }
    auto id = idOf(program);
    if (id.isEmpty())
        id = idOf(QJsonObject{{"id", program.value("programId")}});
    const auto name = text(program, "name").isEmpty() ? text(program, "title") : text(program, "name");
    if (id.isEmpty() || name.isEmpty())
        return {};

    const auto ext = resource.value("resourceExtInfo").toObject();
    const auto songs = program.value("songs").toArray();
    QString related = text(ext, "voiceRelatedSongName");
    if (related.isEmpty() && !songs.isEmpty())
        related = text(songs.first().toObject(), "name");
    const auto count = ext.value("voiceRelatedSongSize").toInt(program.value("trackCount").toInt(songs.size()));
    const auto cover = text(program, "coverUrl");
    QString description = text(program.value("radio").toObject(), "name");
    if (!related.isEmpty() && count > 0)
        description = QStringLiteral("包含「%1」等 %2 首歌曲").arg(related).arg(count);
    return {
        {"id", id}, {"title", name}, {"kind", "program"}, {"description", description},
        {"art", coverUrl(cover.isEmpty() ? elementImage(resource) : cover)},
        {"songCount", count},
    };
}

QVariantList programsFrom(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &item : items) {
        const auto program = programFrom(item.toObject());
        if (!program.isEmpty())
            result.append(program);
    }
    return result;
}

QVariantList essentialPrograms(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &entry : items) {
        const auto item = entry.toObject();
        auto row = programFrom(item);
        if (row.isEmpty())
            continue;
        const auto image = text(item, "picUrl");
        if (row.value("art").toUrl().isEmpty() && !image.isEmpty())
            row["art"] = coverUrl(image);
        const auto radioName = text(item, "radioName");
        if (!radioName.isEmpty())
            row["description"] = radioName;
        row["label"] = text(item, "label");
        result.append(row);
    }
    return result;
}

QVariantMap songFromResource(const QJsonObject &resource)
{
    const auto ext = resource.value("resourceExtInfo").toObject();
    auto song = ext.value("song").toObject();
    if (song.isEmpty())
        song = ext.value("songData").toObject();
    if (song.isEmpty())
        return {};

    auto track = trackFrom(QJsonObject{{"song", song}, {"picUrl", elementImage(resource)}});
    if (track.isEmpty())
        return track;
    const auto remark = text(resource.value("uiElement").toObject().value("subTitle").toObject(), "title");
    // Some blocks repeat the song name as the subtitle; only keep real badges such as "百万红心".
    track["remark"] = remark == track.value("title").toString() ? QString{} : remark;
    track["vip"] = track.value("vip").toBool()
                   || ext.value("songPrivilege").toObject().value("fee").toInt() == 1;
    return track;
}

QVariantList shelvesFrom(const QJsonArray &blocks, bool radar)
{
    QVariantList result;
    QVariantList fallbackPrograms;
    bool hasMusicProgramBlock = false;
    for (const auto &value : blocks) {
        const auto block = value.toObject();
        const auto code = text(block, "blockCode");
        const bool songs = code == "HOMEPAGE_BLOCK_STYLE_RCMD" || code == "HOMEPAGE_BLOCK_VIP_RCMD"
                           || code == "HOMEPAGE_BLOCK_RED_SIMILAR_SONG" || code == "HOMEPAGE_BLOCK_ALL_LISTEN";
        const bool musicPrograms = code == "HOMEPAGE_MUSIC_PODCAST_RCMD_BLOCK";
        const bool genericPrograms = code == "HOMEPAGE_VOICELIST_RCMD";
        const bool lists = radar || code == "HOMEPAGE_BLOCK_OFFICIAL_PLAYLIST";
        if (!(songs || musicPrograms || genericPrograms || lists))
            continue;

        const auto shelfTitle = elementTitle(block, true);
        QVariantList flat;
        auto appendShelf = [&](const QString &title, const QString &kind, const QVariantList &rows) {
            if (!rows.isEmpty() && !title.isEmpty())
                result.append(QVariantMap{{"title", title}, {"kind", kind}, {"items", rows}});
        };
        for (const auto &entry : block.value("creatives").toArray()) {
            const auto creative = entry.toObject();
            const auto creativeTitle = elementTitle(creative);
            QVariantList group;
            for (const auto &element : creative.value("resources").toArray()) {
                const auto resource = element.toObject();
                QVariantMap row;
                if (songs) {
                    row = songFromResource(resource);
                } else if (musicPrograms || genericPrograms) {
                    row = programFrom(resource.value("resourceExtInfo").toObject().value("djProgram").toObject(), resource);
                    if (genericPrograms && row.value("songCount").toInt() == 0)
                        row.clear();
                } else {
                    const auto type = text(resource, "resourceType");
                    const auto id = text(resource, "resourceId");
                    const auto title = elementTitle(resource).isEmpty() ? creativeTitle : elementTitle(resource);
                    if ((type == "list" || type == "playlist") && !id.isEmpty() && !title.isEmpty()) {
                        const auto picture = elementImage(resource);
                        row = {{"id", id}, {"title", title}, {"kind", "playlist"},
                               {"art", coverUrl(picture.isEmpty() ? elementImage(creative) : picture)}};
                    }
                }
                if (!row.isEmpty())
                    group.append(row);
            }
            if (songs && !creativeTitle.isEmpty() && creativeTitle != shelfTitle)
                appendShelf(creativeTitle, "tracks", group);
            else
                flat.append(group);
        }
        if (genericPrograms) {
            fallbackPrograms.append(flat);
        } else if (musicPrograms) {
            hasMusicProgramBlock = hasMusicProgramBlock || !flat.isEmpty();
            appendShelf(shelfTitle.isEmpty() ? QStringLiteral("每天听点儿好音乐") : shelfTitle, "programs", flat);
        } else {
            appendShelf(shelfTitle, songs ? "tracks" : "playlists", flat);
        }
    }
    if (!hasMusicProgramBlock && !fallbackPrograms.isEmpty()) {
        const auto afterSongs = std::find_if(result.cbegin(), result.cend(), [](const QVariant &shelf) {
            return shelf.toMap().value("kind") == "playlists";
        });
        result.insert(afterSongs, QVariantMap{{"title", QStringLiteral("音乐播客推荐")},
                      {"kind", "programs"}, {"items", fallbackPrograms.mid(0, 6)}});
    }
    return result;
}

QJsonArray recentObjects(const QJsonObject &body)
{
    QJsonArray entries;
    for (const auto &row : body.value("data").toObject().value("list").toArray())
        entries.append(unwrap(row.toObject()));
    return entries;
}

QVariantList simpleItems(const QJsonArray &items, const QString &kind)
{
    QVariantList result;
    for (const auto &value : items) {
        const auto item = unwrap(value.toObject());
        auto id = idOf(item);
        if (kind == "video" && id.isEmpty())
            id = text(item, "vid");
        if (kind == "voiceList" && id.isEmpty())
            id = idOf(QJsonObject{{"id", item.value("voiceListId")}});
        auto title = text(item, "name");
        if (title.isEmpty())
            title = text(item, "title");
        if (id.isEmpty() || title.isEmpty())
            continue;
        auto artwork = kind == "artist" ? text(item, "img1v1Url") : QString{};
        if (artwork.isEmpty())
            artwork = text(item, "picUrl");
        if (artwork.isEmpty())
            artwork = text(item, "coverUrl");
        if (artwork.isEmpty())
            artwork = text(item, "coverImgUrl");
        result.append(QVariantMap{
            {"id", id}, {"title", title}, {"kind", kind},
            {"art", coverUrl(artwork)},
            {"description", kind == "artist" && item.contains("musicSize")
                 ? QStringLiteral("单曲：%1").arg(item.value("musicSize").toInt())
                 : text(item, "description")},
        });
    }
    return result;
}

QVariantList vipChartsFrom(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &entry : items) {
        const auto chart = entry.toObject();
        const auto id = idOf(QJsonObject{{"id", chart.value("rankingListId")}});
        const auto title = text(chart, "rankingListName");
        QStringList ids;
        for (const auto &value : chart.value("songIds").toArray()) {
            const auto songId = idOf(QJsonObject{{"id", value}});
            if (!songId.isEmpty())
                ids.append(songId);
            if (ids.size() == 1000)
                break;
        }
        if (!id.isEmpty() && !title.isEmpty() && !ids.isEmpty()) {
            result.append(QVariantMap{
                {"id", id}, {"title", title}, {"kind", "vipChart"},
                {"art", coverUrl(text(chart, "rankListCoverImgUrl"))},
                {"trackCount", ids.size()}, {"songIds", ids.join(',')},
            });
        }
    }
    return result;
}

QJsonArray itemArray(const QJsonValue &value)
{
    if (value.isArray())
        return value.toArray();
    const auto object = value.toObject();
    for (const auto &key : {"data", "list", "items", "songs", "voices", "voiceLists", "voicelist", "voiceListVOs"}) {
        if (object.value(QLatin1String(key)).isArray())
            return object.value(QLatin1String(key)).toArray();
    }
    return {};
}

QVariantList voicePurchasesFrom(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &entry : items) {
        const auto item = unwrap(entry.toObject());
        if (item.value("productType").toInt() == 1) {
            const auto program = programFrom(item);
            if (!program.isEmpty())
                result.append(program);
        } else {
            result.append(simpleItems(QJsonArray{item}, "voiceList"));
        }
    }
    return result;
}

QVariantList eventsFrom(const QJsonArray &items)
{
    QVariantList result;
    for (const auto &value : items) {
        const auto item = value.toObject();
        const auto details = QJsonDocument::fromJson(text(item, "json").toUtf8()).object();
        const auto user = item.value("user").toObject();
        QString message = text(details, "msg");
        QVariantMap event;
        if (details.value("song").isObject()) {
            const auto track = trackFrom(details.value("song").toObject());
            if (!track.isEmpty()) {
                event = track;
                event["kind"] = "track";
                if (message.isEmpty())
                    message = QStringLiteral("分享歌曲 · ") + track.value("title").toString();
            }
        } else if (details.value("playlist").isObject()) {
            const auto list = playlistsFrom(QJsonArray{details.value("playlist")});
            if (!list.isEmpty())
                event = list.first().toMap();
        } else if (details.value("album").isObject()) {
            const auto albums = albumsFrom(QJsonArray{details.value("album")});
            if (!albums.isEmpty())
                event = albums.first().toMap();
        }
        if (event.isEmpty() && message.isEmpty())
            continue;
        if (event.isEmpty())
            event["kind"] = "post";
        if (message.isEmpty())
            message = QStringLiteral("分享 · ") + event.value("title").toString();
        event["subtitle"] = message;
        event["displayTitle"] = text(user, "nickname");
        if (event.value("art").toUrl().isEmpty())
            event["art"] = coverUrl(text(user, "avatarUrl"));
        result.append(event);
    }
    return result;
}

QString encoded(const QString &input)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(input));
}
}

Library::Library(QObject *parent)
    : QObject(parent),
      m_audioCache([this](const QString &id, int bitrate, AudioCache::SourceReady ready) {
          resolveAudio(id, bitrate, std::move(ready));
      })
{
    m_media.setAudioOutput(&m_audio);
    m_playlistModel.setLikedIds(&m_likedIds);
    QSettings settings;
    m_audio.setVolume(qBound(0.0f, settings.value("volume", 0.55).toFloat(), 1.0f));
    m_playMode = qBound(0, settings.value("playMode", 0).toInt(), 3);
    // The last signed-in account's saved pages show while the session is still being checked.
    m_store.setScope(settings.value("lastUserId").toString());
    QTimer::singleShot(5000, this, [this] { m_store.trim(); });
    m_outputId = settings.value("audioOutput").toString();
    m_recentTracks = settings.value("recentTracks").toList();
    applyOutput();
    connect(&m_devices, &QMediaDevices::audioOutputsChanged, this, &Library::applyOutput);
    m_loginPoll.setSingleShot(true);
    connect(&m_loginPoll, &QTimer::timeout, this, &Library::pollLogin);
    connect(&m_media, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState) {
            rememberCurrentTrack();
            prefetchNextTrack();
        }
    });
    connect(&m_audio, &QAudioOutput::volumeChanged, this, [](float volume) {
        QSettings().setValue("volume", volume);
    });
    m_network.setProxy(QNetworkProxy::NoProxy);
    connect(&m_media, &QMediaPlayer::positionChanged, this, &Library::playbackPositionChanged);
    connect(&m_media, &QMediaPlayer::seekableChanged, this, &Library::playbackSeekableChanged);
    connect(&m_media, &QMediaPlayer::durationChanged, this, &Library::playbackDurationChanged);
    m_bufferUpdate.setSingleShot(true);
    m_bufferUpdate.setInterval(80);
    connect(&m_bufferUpdate, &QTimer::timeout, this, &Library::bufferedRangesChanged);
    connect(&m_media, &QMediaPlayer::bufferProgressChanged, this, [this] {
        if (!m_playingAudio.stream && !m_playingAudio.file && !m_bufferUpdate.isActive())
            m_bufferUpdate.start();
    });
    connect(this, &Library::playbackDurationChanged, this, &Library::bufferedRangesChanged);
    connect(&m_media, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (m_recoveryScheduled)
            return;
        if (status == QMediaPlayer::EndOfMedia) {
            m_wantsPlayback = false;
            const int revision = m_playbackRevision;
            QTimer::singleShot(0, this, [this, revision] {
                if (revision == m_playbackRevision && !m_recoveryScheduled)
                    advance(true);
            });
        } else if (status == QMediaPlayer::LoadedMedia && recoveringPlayback()) {
            const auto position = qMax(qint64{0}, m_resumePosition - m_positionBase);
            m_resumePosition = -1;
            m_playbackBusy = false;
            if (m_media.isSeekable() && (m_media.duration() <= 0 || position < m_media.duration()))
                m_media.setPosition(position);
            if (m_wantsPlayback)
                m_media.play();
            emit playbackPositionChanged();
            emit playbackChanged();
        }
    });
    connect(&m_media, &QMediaPlayer::errorOccurred, this, &Library::handlePlaybackError);

}

Library::~Library() = default;

void Library::start()
{
    // An external api-enhanced server can still be used for debugging, but by default the
    // client talks to music.163.com itself through NeteaseApi; no Node.js process is needed.
    const QString override = qEnvironmentVariable("NCM_API_URL");
    if (!override.isEmpty()) {
        const QUrl url(override);
        if ((url.scheme() != "http" && url.scheme() != "https") || url.host().isEmpty()
            || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()) {
            backendFailed(tr("NCM_API_URL 不是有效的 HTTP 地址。"));
            return;
        }
        m_api = url;
        m_nativeApi = false;
        m_backendStatus = tr("自定义 API");
        emit backendChanged();
        refreshDiscovery();
        return;
    }
    m_nativeApi = true;
    m_backendStatus = tr("已连接");
    emit backendChanged();
    loadSession(false, true);
}

void Library::backendFailed(const QString &message)
{
    m_api.clear();
    m_backendStatus = tr("连接失败");
    m_discoveryBusy = false;
    m_discoveryError = message;
    ++m_discoveryRevision;
    emit backendChanged();
    emit discoveryChanged();
}

void Library::get(const QString &route, QUrlQuery parameters, Response onResult, bool post)
{
    if (!backendReady()) {
        onResult({}, tr("本地 API 尚未连接，请稍后重试。"));
        return;
    }
    if (m_nativeApi) {
        // Parameters arrive percent-encoded (the old HTTP bridge decoded them itself).
        QUrlQuery decoded;
        for (const auto &item : parameters.queryItems(QUrl::FullyDecoded))
            decoded.addQueryItem(item.first, QString::fromUtf8(QUrl::toPercentEncoding(item.second)));
        QPointer<Library> self(this);
        m_neteaseApi.call(route, decoded, post, [self, onResult = std::move(onResult)](const QJsonObject &body, int status) {
            if (!self)
                return;
            QString error;
            if (status != 200)
                error = status == 502 ? tr("连接音乐服务失败，请检查网络后重试。")
                                      : tr("音乐服务返回错误（%1），请稍后重试。").arg(status);
            onResult(body, error);
        });
        return;
    }
    QUrl url(m_api);
    QString basePath = url.path();
    if (basePath.endsWith('/'))
        basePath.chop(1);
    url.setPath(basePath + route);
    url.setQuery(parameters);

    QNetworkRequest request(url);
    request.setTransferTimeout(12000);
    request.setRawHeader("Accept", "application/json");
    {
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    }
    if (post)
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    auto *reply = post ? m_network.post(request, QByteArray{}) : m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, onResult = std::move(onResult)] {
        const auto document = QJsonDocument::fromJson(reply->readAll());
        const auto body = document.object();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const int status = body.value("code").toInt(200);
        QString error;
        if (http >= 400 || status != 200)
            error = tr("音乐服务返回错误（%1），请稍后重试。").arg(status != 200 ? status : http);
        else if (reply->error() != QNetworkReply::NoError)
            error = tr("连接音乐服务失败，请检查网络后重试。");
        else if (!document.isObject())
            error = tr("音乐服务返回了无法识别的数据。");
        reply->deleteLater();
        onResult(body, error);
    });
}

namespace {
constexpr int maxCachedResponses = 160;
// Freshness per kind of content. Account-specific lists change when the user acts, so
// they stay short; public catalog pages barely change within a session.
constexpr int catalogMaxAge = 15 * 60;
constexpr int detailMaxAge = 10 * 60;
}

void Library::fetch(const QString &route, const QUrlQuery &parameters, int maxAgeSeconds, Response onResult)
{
    const QString key = route + u'?' + parameters.toString(QUrl::FullyEncoded);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto cached = m_responseCache.constFind(key);
    if (cached == m_responseCache.cend() && maxAgeSeconds > 0) {
        // Not in memory yet: pick up what an earlier session saved.
        const auto stored = m_store.load(key);
        if (stored.valid)
            cached = m_responseCache.insert(key, {stored.body, stored.storedAt});
    }
    const bool hasCopy = !m_bypassCache && cached != m_responseCache.cend() && maxAgeSeconds > 0;
    if (hasCopy && now - cached->storedAt <= maxAgeSeconds) {
        const QJsonObject body = cached->body;
        // Stay asynchronous so callers observe the same ordering as a network reply.
        QTimer::singleShot(0, this, [onResult = std::move(onResult), body] { onResult(body, {}); });
        return;
    }
    // Stale but present: answer at once with the saved copy, refresh quietly, and only call
    // back again if the content actually changed.
    QJsonObject staleBody;
    if (hasCopy) {
        staleBody = cached->body;
        QTimer::singleShot(0, this, [onResult, staleBody] { onResult(staleBody, {}); });
    }
    auto pending = m_inflight.find(key);
    if (pending != m_inflight.end()) {
        if (!hasCopy)
            pending->append(std::move(onResult));
        return;
    }
    m_inflight.insert(key, {});
    if (!hasCopy)
        m_inflight[key].append(onResult);
    get(route, parameters, [this, key, maxAgeSeconds, hasCopy, staleBody, onResult](const QJsonObject &body, const QString &error) {
        if (error.isEmpty()) {
            const qint64 storedAt = QDateTime::currentSecsSinceEpoch();
            m_responseOrder.removeOne(key);
            m_responseOrder.append(key);
            m_responseCache.insert(key, {body, storedAt});
            while (m_responseOrder.size() > maxCachedResponses)
                m_responseCache.remove(m_responseOrder.takeFirst());
            if (maxAgeSeconds > 0)
                m_store.save(key, body, storedAt);
        }
        const auto callbacks = m_inflight.take(key);
        for (const auto &callback : callbacks)
            callback(body, error);
        if (hasCopy && error.isEmpty() && body != staleBody) {
            m_revalidating = true;
            onResult(body, {});
            m_revalidating = false;
        }
    });
}

void Library::dropCachedResponses()
{
    m_responseCache.clear();
    m_responseOrder.clear();
    m_prefetched.clear();
    // Saved responses are per account; switch the disk scope to whoever is signed in now.
    m_store.setScope(m_profile.value("userId").toString());
}

// Warm the cache for a playlist the pointer is resting on, so opening it is instant.
void Library::prefetchCollection(const QVariantMap &playlist)
{
    const auto id = playlist.value("id").toString();
    const auto kind = playlist.value("kind").toString();
    if (id.isEmpty() || !backendReady() || !QStringList{"playlist", "chart", "album", "artist"}.contains(kind))
        return;
    const QString key = kind + u':' + id;
    if (m_prefetched.contains(key))
        return;
    m_prefetched.insert(key);
    QString route;
    QUrlQuery args;
    collectionRequest(playlist, 0, route, args);
    fetch(route, args, detailMaxAge, [](const QJsonObject &, const QString &) {});
    if (kind == "playlist" || kind == "chart")
        fetch("/playlist/detail", {{"id", id}}, 60, [](const QJsonObject &, const QString &) {});
    else if (kind == "artist")
        fetch("/artist/detail", {{"id", id}}, detailMaxAge, [](const QJsonObject &, const QString &) {});
}

// Warm the catalog sections reachable from the current page.
void Library::prefetchSection(const QString &section)
{
    if (!backendReady())
        return;
    const auto ignore = [](const QJsonObject &, const QString &) {};
    if (section == "essential")
        fetch("/essential/overview", {}, catalogMaxAge, ignore);
    else if (section == "featured")
        fetch("/top/playlist", {{"limit", "60"}, {"cat", encoded(m_playlistCategory)}}, catalogMaxAge, ignore);
    else if (section == "charts")
        fetch("/toplist/groups", {}, catalogMaxAge, ignore);
    else if (section == "podcasts")
        fetch("/dj/recommend", {}, catalogMaxAge, ignore);
}

void Library::refreshDiscovery(bool force)
{
    if (!backendReady()) {
        start();
        return;
    }
    // Every fetch below is issued synchronously, so the flag covers exactly this round.
    m_bypassCache = force;
    const auto restore = qScopeGuard([this] { m_bypassCache = false; });
    const int revision = ++m_discoveryRevision;
    m_pendingDiscovery = localAccountSupported() ? 6 : 4;
    fetch("/banner", {{"type", "0"}}, catalogMaxAge, [this, revision](const QJsonObject &body, const QString &error) {
        if (revision != m_discoveryRevision || !error.isEmpty())
            return;
        m_banners.clear();
        for (const auto &value : body.value("banners").toArray()) {
            const auto item = value.toObject();
            const int type = item.value("targetType").toInt();
            const auto id = idOf(QJsonObject{{"id", item.value("targetId")}});
            const QString kind = type == 1 ? "song" : type == 10 ? "album" : type == 1000 ? "playlist" : QString{};
            const auto picture = text(item, "imageUrl").isEmpty() ? text(item, "bigImageUrl") : text(item, "imageUrl");
            if (!kind.isEmpty() && !id.isEmpty() && !picture.isEmpty())
                m_banners.append(QVariantMap{{"id", id}, {"kind", kind}, {"title", text(item, "typeTitle")},
                                             {"art", QUrl(picture)}});
        }
        emit discoveryChanged();
    });
    m_recentListen.clear();
    if (loggedIn() && localAccountSupported()) {
        get("/account/recent-listen", {}, [this, revision](const QJsonObject &body, const QString &error) {
            if (revision != m_discoveryRevision || !error.isEmpty())
                return;
            m_recentListen.clear();
            for (const auto &value : body.value("data").toObject().value("resources").toArray()) {
                const auto item = value.toObject();
                const auto type = text(item, "resourceType");
                const auto id = idOf(QJsonObject{{"id", item.value("resourceId")}});
                const QString kind = type == "list" ? "playlist" : type == "album" ? "album" : QString{};
                const auto covers = item.value("coverUrlList").toArray();
                if (!kind.isEmpty() && !id.isEmpty())
                    m_recentListen.append(QVariantMap{
                        {"id", id}, {"kind", kind}, {"title", text(item, "title")}, {"tag", text(item, "tag")},
                        {"art", coverUrl(covers.isEmpty() ? QString{} : covers.first().toString())}});
            }
            emit discoveryChanged();
        });
    }
    m_discoveryBusy = true;
    m_discoveryError.clear();
    emit discoveryChanged();

    fetchRecommendedPlaylists(loggedIn() && localAccountSupported() ? "/account/recommendations" : "/personalized", revision);
    fetch("/personalized/newsong", {{"limit", "12"}}, catalogMaxAge, [this, revision](const QJsonObject &body, const QString &error) {
        if (revision != m_discoveryRevision)
            return;
        if (error.isEmpty())
            m_freshTracks = tracksFrom(body.value("result").toArray());
        finishDiscoveryRequest(revision, error);
    });
    fetch("/toplist/groups", {}, catalogMaxAge, [this, revision](const QJsonObject &body, const QString &error) {
        if (revision != m_discoveryRevision)
            return;
        if (error.isEmpty()) {
            m_featuredCharts.clear();
            for (const auto &group : chartGroupsFrom(body.value("data").toArray())) {
                if (group.toMap().value("kind") == "chartRows") {
                    m_featuredCharts = group.toMap().value("items").toList();
                    break;
                }
            }
        }
        finishDiscoveryRequest(revision, error);
    });
    fetch("/album/newest", {}, catalogMaxAge, [this, revision](const QJsonObject &body, const QString &error) {
        if (revision != m_discoveryRevision)
            return;
        if (error.isEmpty())
            m_newAlbums = albumsFrom(body.value("albums").toArray());
        finishDiscoveryRequest(revision, error);
    });

    if (localAccountSupported()) {
        get("/homepage/blocks", {}, [this, revision](const QJsonObject &body, const QString &error) {
            if (revision != m_discoveryRevision)
                return;
            if (error.isEmpty())
                m_homeShelves = shelvesFrom(body.value("data").toObject().value("blocks").toArray(), false);
            finishDiscoveryRequest(revision, error);
        });
        get("/homepage/radar", {}, [this, revision](const QJsonObject &body, const QString &error) {
            if (revision != m_discoveryRevision)
                return;
            if (error.isEmpty())
                m_radarShelves = shelvesFrom(body.value("data").toObject().value("blocks").toArray(), true);
            finishDiscoveryRequest(revision, error);
        });
    }

}

void Library::fetchRecommendedPlaylists(const QString &route, int revision)
{
    const bool personalized = route == "/account/recommendations";
    const QUrlQuery query = personalized ? QUrlQuery{} : QUrlQuery{{"limit", "13"}};
    get(route, query, [this, revision, personalized](const QJsonObject &body, const QString &error) {
        if (revision != m_discoveryRevision)
            return;
        const auto list = playlistsFrom(body.value(personalized ? "recommend" : "result").toArray());
        if (personalized && (!error.isEmpty() || list.isEmpty())) {
            fetchRecommendedPlaylists("/personalized", revision);
            return;
        }
        if (error.isEmpty())
            m_recommendations = list;
        finishDiscoveryRequest(revision, error);
    });
}

void Library::finishDiscoveryRequest(int revision, const QString &error)
{
    if (revision != m_discoveryRevision)
        return;
    if (m_revalidating) {
        emit discoveryChanged();
        return;
    }
    if (!error.isEmpty() && m_discoveryError.isEmpty())
        m_discoveryError = error;
    m_discoveryBusy = --m_pendingDiscovery > 0;
    emit discoveryChanged();
    if (!m_discoveryBusy) {
        // Home is ready: quietly warm the other top-level catalog pages.
        QTimer::singleShot(800, this, [this] {
            for (const auto &section : {QStringLiteral("essential"), QStringLiteral("featured"),
                                        QStringLiteral("charts"), QStringLiteral("podcasts")})
                prefetchSection(section);
        });
    }
}

void Library::search(const QString &term, const QString &type)
{
    const QString cleaned = term.trimmed();
    static const QHash<QString, QString> keys{
        {"1", "songs"}, {"1000", "playlists"}, {"100", "artists"}, {"10", "albums"}, {"1009", "djRadios"}};
    if (cleaned.isEmpty() || !keys.contains(type))
        return;

    const int revision = ++m_searchRevision;
    m_searchTerm = cleaned;
    m_searchType = type;
    m_searchResults.clear();
    m_searchItems.clear();
    m_searchError.clear();
    m_searchBusy = true;
    emit searchChanged();
    const QString query = encoded(cleaned);
    get("/cloudsearch", {{"keywords", query}, {"type", type}, {"limit", type == "1" ? "60" : "36"}},
        [this, revision, type](const QJsonObject &body, const QString &error) {
            if (revision != m_searchRevision)
                return;
            m_searchBusy = false;
            m_searchError = error;
            const auto rows = body.value("result").toObject().value(keys.value(type)).toArray();
            if (type == "1")
                m_searchResults = tracksFrom(rows);
            else if (type == "1000")
                m_searchItems = playlistsFrom(rows);
            else if (type == "100")
                m_searchItems = simpleItems(rows, "artist");
            else if (type == "10")
                m_searchItems = albumsFrom(rows);
            else
                m_searchItems = radiosFrom(rows);
            emit searchChanged();
        });
}

void Library::suggest(const QString &term)
{
    const auto cleaned = term.trimmed();
    const int revision = ++m_suggestRevision;
    if (cleaned.isEmpty()) {
        m_searchSuggestions.clear();
        emit suggestionsChanged();
        return;
    }
    get("/search/suggest", {{"keywords", encoded(cleaned)}}, [this, revision](const QJsonObject &body, const QString &error) {
        if (revision != m_suggestRevision || !error.isEmpty())
            return;
        const auto result = body.value("result").toObject();
        QStringList words;
        auto add = [&words](const QString &word) {
            if (!word.isEmpty() && !words.contains(word) && words.size() < 10)
                words.append(word);
        };
        for (const auto &entry : result.value("artists").toArray())
            add(text(entry.toObject(), "name"));
        for (const auto &entry : result.value("songs").toArray()) {
            const auto song = entry.toObject();
            const auto artists = song.value("artists").toArray();
            const auto artist = artists.isEmpty() ? QString{} : text(artists.first().toObject(), "name");
            add(artist.isEmpty() ? text(song, "name") : artist + u' ' + text(song, "name"));
        }
        for (const auto &key : {"albums", "playlists"}) {
            for (const auto &entry : result.value(QLatin1String(key)).toArray())
                add(text(entry.toObject(), "name"));
        }
        m_searchSuggestions = words;
        emit suggestionsChanged();
    });
}

void Library::loadHotSearches()
{
    if (!m_hotSearches.isEmpty())
        return;
    get("/search/hot", {}, [this](const QJsonObject &body, const QString &error) {
        if (!error.isEmpty())
            return;
        m_hotSearches.clear();
        for (const auto &entry : body.value("data").toArray()) {
            const auto word = text(entry.toObject(), "searchWord");
            if (!word.isEmpty() && m_hotSearches.size() < 10)
                m_hotSearches.append(word);
        }
        emit suggestionsChanged();
    });
}

void Library::openPlaylist(const QVariantMap &playlist, bool playNow)
{
    if (playlist.value("id").toString().isEmpty())
        return;
    const int revision = ++m_playlistRevision;
    m_playlist = playlist;
    m_playlistTracks.clear();
    m_playlistModel.setTracks({});
    m_playlistModel.setFilter({});
    m_playlistOffset = 0;
    m_playlistHasMore = false;
    m_playlistError.clear();
    m_playlistBusy = true;
    emit playlistChanged();
    m_playlistTab = QStringLiteral("tracks");
    m_playlistTabItems.clear();
    m_playlistTabText.clear();
    m_playlistTabBusy = false;
    emit playlistTabChanged();
    requestCollection(playlist, revision, playNow);
    loadCollectionDetail(playlist, revision);
}

// Fills in what a card or a sidebar row does not carry: owner, counts, description.
void Library::loadCollectionDetail(const QVariantMap &playlist, int revision)
{
    const auto kind = playlist.value("kind").toString();
    const auto id = playlist.value("id").toString();
    if (kind == "playlist" || kind == "chart") {
        fetch("/playlist/detail", {{"id", id}}, 60, [this, revision](const QJsonObject &body, const QString &error) {
            const auto detail = body.value("playlist").toObject();
            if (revision != m_playlistRevision || !error.isEmpty() || detail.isEmpty())
                return;
            const auto creator = detail.value("creator").toObject();
            const auto creatorId = idOf(QJsonObject{{"id", creator.value("userId")}});
            if (m_playlist.value("title").toString().isEmpty())
                m_playlist["title"] = text(detail, "name");
            if (m_playlist.value("art").toUrl().isEmpty())
                m_playlist["art"] = coverUrl(text(detail, "coverImgUrl"));
            m_playlist["intro"] = text(detail, "description");
            m_playlist["creatorName"] = text(creator, "nickname");
            m_playlist["creatorAvatar"] = coverUrl(text(creator, "avatarUrl"));
            m_playlist["createTime"] = dateText(detail.value("createTime").toInteger());
            m_playlist["playCount"] = detail.value("playCount").toInteger();
            m_playlist["subscribedCount"] = detail.value("subscribedCount").toInteger();
            m_playlist["commentCount"] = detail.value("commentCount").toInteger();
            m_playlist["subscribed"] = detail.value("subscribed").toBool();
            m_playlist["trackCount"] = detail.value("trackCount").toInt();
            m_playlist["specialType"] = detail.value("specialType").toInt();
            m_playlist["owned"] = loggedIn() && creatorId == m_profile.value("userId").toString();
            m_playlist["detailed"] = true;
            emit playlistChanged();
        });
    } else if (kind == "artist") {
        fetch("/artist/detail", {{"id", id}}, detailMaxAge, [this, revision](const QJsonObject &body, const QString &error) {
            const auto artist = body.value("data").toObject().value("artist").toObject();
            if (revision != m_playlistRevision || !error.isEmpty() || artist.isEmpty())
                return;
            QStringList names;
            for (const auto &alias : artist.value("alias").toArray())
                names.append(alias.toString());
            const auto avatar = text(artist, "avatar");
            m_playlist["title"] = text(artist, "name");
            m_playlist["art"] = coverUrl(avatar.isEmpty() ? text(artist, "cover") : avatar);
            m_playlist["intro"] = names.join(u'/');
            m_playlist["albumCount"] = artist.value("albumSize").toInt();
            m_playlist["detailed"] = true;
            emit playlistChanged();
        });
    }
}

void Library::loadPlaylistTab(const QString &tab)
{
    if (tab == m_playlistTab)
        return;
    m_playlistTab = tab;
    m_playlistTabItems.clear();
    m_playlistTabText.clear();
    m_playlistTabBusy = tab != "tracks";
    emit playlistTabChanged();
    if (tab == "tracks")
        return;

    const int revision = m_playlistRevision;
    const auto id = m_playlist.value("id").toString();
    static const QHash<QString, QString> routes{
        {"comments", "/playlist/comments"}, {"subscribers", "/playlist/subscribers"},
        {"albums", "/artist/albums"}, {"similar", "/artist/similar"}, {"intro", "/artist/desc"}};
    QUrlQuery query{{"id", id}};
    if (tab == "comments" || tab == "subscribers" || tab == "albums")
        query.addQueryItem("limit", tab == "comments" ? "30" : "60");
    fetch(routes.value(tab), query, tab == "comments" ? 120 : detailMaxAge, [this, revision, tab](const QJsonObject &body, const QString &error) {
        if (revision != m_playlistRevision || tab != m_playlistTab)
            return;
        m_playlistTabBusy = false;
        if (!error.isEmpty()) {
            m_playlistTabText = error;
        } else if (tab == "comments") {
            const auto hot = commentsFrom(body.value("hotComments").toArray());
            const auto latest = commentsFrom(body.value("comments").toArray());
            if (!hot.isEmpty()) {
                m_playlistTabItems.append(QVariantMap{{"kind", "heading"}, {"title", tr("精彩评论")}});
                m_playlistTabItems.append(hot);
            }
            if (!latest.isEmpty()) {
                m_playlistTabItems.append(QVariantMap{
                    {"kind", "heading"}, {"title", tr("最新评论 %1").arg(body.value("total").toInt())}});
                m_playlistTabItems.append(latest);
            }
        } else if (tab == "subscribers") {
            m_playlistTabItems = peopleFrom(body.value("subscribers").toArray());
        } else if (tab == "albums") {
            m_playlistTabItems = albumsFrom(body.value("hotAlbums").toArray());
        } else if (tab == "similar") {
            m_playlistTabItems = simpleItems(body.value("artists").toArray(), "artist");
        } else if (tab == "intro") {
            QStringList parts{text(body, "briefDesc")};
            for (const auto &entry : body.value("introduction").toArray()) {
                const auto section = entry.toObject();
                parts.append(text(section, "ti") + u'\n' + text(section, "txt"));
            }
            parts.removeAll(QString{});
            m_playlistTabText = parts.join("\n\n");
        }
        emit playlistTabChanged();
    });
}

void Library::togglePlaylistSubscribe()
{
    const auto id = m_playlist.value("id").toString();
    if (!loggedIn() || id.isEmpty() || m_playlist.value("kind") != "playlist" || m_playlist.value("owned").toBool())
        return;
    const bool subscribe = !m_playlist.value("subscribed").toBool();
    get("/account/playlist/subscribe", {{"id", id}, {"subscribe", subscribe ? "true" : "false"}},
        [this, id, subscribe](const QJsonObject &, const QString &error) {
            if (!error.isEmpty()) {
                emit notification(subscribe ? tr("收藏歌单失败，请重试。") : tr("取消收藏失败，请重试。"));
                return;
            }
            if (m_playlist.value("id").toString() == id) {
                const auto count = m_playlist.value("subscribedCount").toLongLong();
                m_playlist["subscribed"] = subscribe;
                m_playlist["subscribedCount"] = qMax(qint64{0}, count + (subscribe ? 1 : -1));
                emit playlistChanged();
            }
            emit notification(subscribe ? tr("已收藏歌单。") : tr("已取消收藏。"));
            loadPersonalLists();
        }, true);
}

void Library::addToPlaylist(const QString &playlistId, const QString &trackId)
{
    if (!loggedIn() || playlistId.isEmpty() || trackId.isEmpty())
        return;
    get("/account/playlist/tracks", {{"op", "add"}, {"pid", playlistId}, {"track", trackId}},
        [this, playlistId](const QJsonObject &body, const QString &error) {
            for (auto it = m_responseCache.begin(); it != m_responseCache.end();) {
                if (it.key().contains(QStringLiteral("id=") + playlistId)) {
                    m_store.remove(it.key());
                    it = m_responseCache.erase(it);
                }
                else
                    ++it;
            }
            if (body.value("code").toInt() == 502)
                emit notification(tr("歌曲已在该歌单中。"));
            else
                emit notification(error.isEmpty() ? tr("已收藏到歌单。") : tr("收藏到歌单失败，请重试。"));
        }, true);
}

void Library::removeFromPlaylist(const QString &trackId)
{
    const auto id = m_playlist.value("id").toString();
    if (!loggedIn() || trackId.isEmpty() || id.isEmpty() || !m_playlist.value("owned").toBool())
        return;
    get("/account/playlist/tracks", {{"op", "del"}, {"pid", id}, {"track", trackId}},
        [this, id, trackId](const QJsonObject &, const QString &error) {
            if (!error.isEmpty()) {
                emit notification(tr("从歌单删除失败，请重试。"));
                return;
            }
            for (auto it = m_responseCache.begin(); it != m_responseCache.end();) {
                if (it.key().contains(QStringLiteral("id=") + id)) {
                    m_store.remove(it.key());
                    it = m_responseCache.erase(it);
                }
                else
                    ++it;
            }
            if (m_playlist.value("id").toString() == id) {
                m_playlistModel.removeId(trackId);
                m_playlistTracks.removeIf([&trackId](const QVariant &track) {
                    return track.toMap().value("id").toString() == trackId;
                });
                m_playlist["trackCount"] = qMax(0, m_playlist.value("trackCount").toInt() - 1);
                emit playlistChanged();
            }
            emit notification(tr("已从歌单删除。"));
        }, true);
}

void Library::loadMorePlaylist()
{
    if (m_playlistBusy || !m_playlistHasMore)
        return;
    m_playlistBusy = true;
    m_playlistError.clear();
    emit playlistChanged();
    requestCollection(m_playlist, m_playlistRevision, false, m_playlistOffset);
}

void Library::collectionRequest(const QVariantMap &playlist, int offset, QString &route, QUrlQuery &args) const
{
    const QString id = playlist.value("id").toString();
    const QString kind = playlist.value("kind").toString();
    const bool paged = kind == "playlist" || kind == "chart" || kind == "radio";
    const int pageSize = kind == "chart" ? 200 : kind == "radio" ? 50 : 100;
    args = QUrlQuery{{"id", id}};
    route = "/playlist/track/all";
    if (kind == "radio") {
        route = "/dj/program";
        args = QUrlQuery{{"rid", id}};
    } else if (kind == "program") {
        route = "/dj/program/detail";
    } else if (kind == "album") {
        route = "/album";
    } else if (kind == "artist") {
        route = "/artist/songs";
        args.addQueryItem("limit", "100");
    } else if (kind == "vipChart") {
        route = "/song/detail";
        args = QUrlQuery{{"ids", playlist.value("songIds").toString()}};
    } else if (kind == "voiceList") {
        route = "/voice/list";
        args = QUrlQuery{{"voiceListId", id}};
        args.addQueryItem("limit", "100");
    }
    if (paged) {
        args.addQueryItem("limit", QString::number(pageSize));
        args.addQueryItem("offset", QString::number(offset));
    }
}

void Library::requestCollection(const QVariantMap &playlist, int revision, bool playNow, int offset)
{
    const QString kind = playlist.value("kind").toString();
    const bool paged = kind == "playlist" || kind == "chart" || kind == "radio";
    const int pageSize = kind == "chart" ? 200 : kind == "radio" ? 50 : 100;
    QString route;
    QUrlQuery args;
    collectionRequest(playlist, offset, route, args);
    // The user's own playlists change when songs are added or removed; keep those fresh.
    const bool personal = playlist.value("owned").toBool() || playlist.value("specialType").toInt() == 5
        || std::any_of(m_createdPlaylists.cbegin(), m_createdPlaylists.cend(), [&playlist](const QVariant &item) {
               return item.toMap().value("id") == playlist.value("id");
           });
    fetch(route, args, personal ? 0 : detailMaxAge, [this, revision, kind, paged, pageSize, offset, playNow](const QJsonObject &body, const QString &error) {
        if (revision != m_playlistRevision)
            return;
        m_playlistBusy = false;
        m_playlistError = error;
        if (!error.isEmpty()) {
            emit playlistChanged();
            return;
        }
        QVariantList tracks;
        QJsonArray rows;
        if (kind == "radio") {
            rows = body.value("programs").toArray();
            tracks = episodesFrom(rows);
        } else if (kind == "program") {
            const auto program = body.value("program").toObject();
            tracks = episodesFrom(QJsonArray{program});
            tracks.append(tracksFrom(program.value("songs").toArray()));
        } else if (kind == "voiceList") {
            const auto payload = body.value("data").toObject();
            rows = payload.value("voices").toArray();
            if (rows.isEmpty())
                rows = payload.value("list").toArray();
            tracks = episodesFrom(rows);
        } else {
            rows = body.value("songs").toArray();
            tracks = tracksFrom(rows);
            if (kind == "album") {
                const auto album = body.value("album").toObject();
                if (m_playlist.value("title").toString().isEmpty())
                    m_playlist["title"] = text(album, "name");
                if (m_playlist.value("art").toUrl().isEmpty())
                    m_playlist["art"] = coverUrl(text(album, "picUrl"));
                m_playlist["creatorName"] = text(album.value("artist").toObject(), "name");
                m_playlist["createTime"] = dateText(album.value("publishTime").toInteger());
                m_playlist["intro"] = text(album, "description");
                m_playlist["detailed"] = true;
            }
        }

        if (m_revalidating && offset == 0) {
            // Fresher copy of the first page replaced a saved one: start over from it.
            m_playlistTracks.clear();
        }
        QSet<QString> existing;
        for (const auto &track : std::as_const(m_playlistTracks))
            existing.insert(track.toMap().value("id").toString());
        bool advanced = false;
        QVariantList added;
        for (const auto &track : std::as_const(tracks)) {
            const auto id = track.toMap().value("id").toString();
            if (!existing.contains(id)) {
                m_playlistTracks.append(track);
                added.append(track);
                existing.insert(id);
                advanced = true;
            }
        }
        if (m_revalidating && offset == 0)
            m_playlistModel.setTracks(m_playlistTracks);
        else
            m_playlistModel.append(added);
        m_playlistOffset = offset + rows.size();
        const int total = m_playlist.value("trackCount").toInt();
        m_playlistHasMore = paged && advanced && rows.size() >= pageSize
                             && (total <= 0 || m_playlistOffset < total);
        emit playlistChanged();
        if (playNow && !m_revalidating && !m_playlistTracks.isEmpty())
            playAll(m_playlistTracks);
    });
}

QVariantMap Library::currentTrack() const
{
    if (m_queueIndex < 0 || m_queueIndex >= m_queue.size())
        return {};
    return m_queue.at(m_queueIndex).toMap();
}

void Library::loadCurrentLyrics(bool retry)
{
    const auto id = currentTrack().value("id").toString();
    if (id == m_lyricsTrackId && (!retry || m_lyricsBusy))
        return;
    m_lyricsTrackId = id;
    const int revision = ++m_lyricsRevision;
    m_lyrics.clear();
    m_lyricsError.clear();
    m_lyricsBusy = !id.isEmpty();
    emit lyricsChanged();
    if (id.isEmpty())
        return;

    get("/lyric/new", {{"id", id}}, [this, revision](const QJsonObject &body, const QString &error) {
        if (revision != m_lyricsRevision)
            return;
        m_lyricsBusy = false;
        m_lyricsError = error;
        if (error.isEmpty()) {
            const auto yrc = parseYrc(body.value("yrc").toObject().value("lyric").toString());
            const auto ytlrc = parseLyrics(body.value("ytlrc").toObject().value("lyric").toString());
            const auto tlyric = parseLyrics(body.value("tlyric").toObject().value("lyric").toString());
            const auto &transTimed = !ytlrc.timed.isEmpty() ? ytlrc.timed : tlyric.timed;

            auto matchTranslation = [&transTimed](qint64 time) -> QString {
                if (transTimed.isEmpty())
                    return {};
                if (const auto exact = transTimed.value(time); !exact.isEmpty())
                    return exact;
                auto it = transTimed.lowerBound(time - 250);
                QString best;
                qint64 bestDiff = 260;
                for (; it != transTimed.cend() && it.key() <= time + 250; ++it) {
                    const qint64 diff = qAbs(it.key() - time);
                    if (diff < bestDiff) {
                        bestDiff = diff;
                        best = it.value();
                    }
                }
                return best;
            };

            if (!yrc.isEmpty()) {
                for (const auto &line : yrc) {
                    QVariantList wordList;
                    wordList.reserve(line.words.size());
                    for (const auto &w : line.words) {
                        wordList.append(QVariantMap{
                            {"time", w.time},
                            {"duration", w.duration},
                            {"offset", w.offset},
                            {"length", w.length}
                        });
                    }
                    m_lyrics.append(QVariantMap{
                        {"time", line.time},
                        {"duration", line.duration},
                        {"text", line.text},
                        {"translation", matchTranslation(line.time)},
                        {"words", wordList}
                    });
                }
            } else {
                const auto original = parseLyrics(body.value("lrc").toObject().value("lyric").toString());
                for (auto it = original.timed.cbegin(); it != original.timed.cend(); ++it)
                    m_lyrics.append(QVariantMap{
                        {"time", it.key()},
                        {"duration", qint64{0}},
                        {"text", it.value()},
                        {"translation", matchTranslation(it.key())},
                        {"words", QVariantList{}}
                    });
                if (m_lyrics.isEmpty()) {
                    for (const auto &line : original.plain)
                        m_lyrics.append(QVariantMap{
                            {"time", -1},
                            {"duration", qint64{0}},
                            {"text", line},
                            {"translation", QString{}},
                            {"words", QVariantList{}}
                        });
                }
            }
        }
        emit lyricsChanged();
    });
}

QVariantList Library::bufferedRanges() const
{
    QVariantList result;
    auto add = [&result](qint64 first, qint64 end, qint64 total) {
        if (total <= 0)
            return;
        first = qBound(qint64{0}, first, total);
        end = qBound(first, end, total);
        if (first < end)
            result.append(QVariantMap{{"start", qreal(first) / total}, {"end", qreal(end) / total}});
    };
    if (m_playingAudio.stream) {
        const auto &stream = m_playingAudio.stream;
        for (const auto &[first, end] : stream->bufferedByteRanges())
            add(first, end, stream->size());
    } else if (m_playingAudio.file) {
        add(0, 1, 1);
    } else if (!m_media.source().isEmpty()) {
        const auto intervals = m_media.bufferedTimeRange().intervals();
        for (const auto &interval : intervals)
            add(interval.start(), interval.end(), playbackDuration());
    }
    return result;
}

void Library::playCollection(const QVariantList &tracks, int index)
{
    if (index < 0 || index >= tracks.size())
        return;
    m_roamPlaying = false;
    m_queue = tracks;
    m_queueIndex = index;
    resetShuffle();
    playIndex(index);
}

void Library::playAll(const QVariantList &tracks)
{
    if (tracks.isEmpty())
        return;
    m_roamPlaying = false;
    m_queue = tracks;
    m_skipUnavailable = true;
    m_skipsRemaining = 8;
    m_skipDirection = 1;
    const int first = m_playMode == 2 ? QRandomGenerator::global()->bounded(int(tracks.size())) : 0;
    m_queueIndex = first;
    resetShuffle();
    startQueueIndex(first);
}

void Library::playNewMusic()
{
    playAll(m_freshTracks);
}

void Library::playIndex(int index)
{
    m_skipUnavailable = false;
    m_skipDirection = 1;
    startQueueIndex(index);
}

void Library::startQueueIndex(int index)
{
    if (index < 0 || index >= m_queue.size())
        return;
    m_queueIndex = index;
    m_streamRetries = 0;
    m_streamBitrate = normalStreamBitrate;
    emit queueChanged();
    emit likesChanged();
    requestTrackSource(-1);
}

void Library::resolveAudio(const QString &id, int bitrate, AudioCache::SourceReady ready)
{
    get("/song/url", {{"id", id}, {"br", QString::number(bitrate)}},
        [ready = std::move(ready)](const QJsonObject &body, const QString &error) {
            const auto rows = body.value("data").toArray();
            const auto track = rows.isEmpty() ? QJsonObject{} : rows.first().toObject();
            const auto type = text(track, "type").toLower();
            const QStringList extensions{"mp3", "flac", "m4a", "aac", "wav", "ogg", "opus", "aiff"};
            const auto extension = extensions.contains(type) ? "." + type : QStringLiteral(".audio");
            const auto md5 = text(track, "md5").toLower();
            static const QRegularExpression hashFormat(QStringLiteral("^[a-f0-9]{32}$"));
            ready({QUrl(text(track, "url")), hashFormat.match(md5).hasMatch() ? md5.toLatin1() : QByteArray{},
                   qMax(qint64{0}, track.value("size").toInteger()), extension,
                   track.value("freeTrialInfo").isObject()}, error);
        });
}

void Library::prefetchNextTrack()
{
    const int target = stepIndex(1, Step::Automatic);
    if (target < 0 || target == m_queueIndex)
        return;
    const auto nextId = m_queue.at(target).toMap().value("id").toString();
    m_audioCache.prefetch(nextId, normalStreamBitrate);
}

void Library::clearPreparedAudio()
{
    if (m_audioCache.preparing()) {
        ++m_playbackRevision;
        m_playbackBusy = false;
        m_recoveryScheduled = false;
        if (m_media.source().isEmpty())
            m_playbackError = tr("账号状态已更新，点播放重新获取音源。");
        emit playbackChanged();
    }
    m_audioCache.clearPending();
}

void Library::requestTrackSource(qint64 resumePosition, bool playWhenReady, bool keepCurrent)
{
    const int revision = ++m_playbackRevision;
    const bool switching = keepCurrent && !m_media.source().isEmpty();
    m_resumePosition = switching ? -1 : resumePosition;
    m_playbackBusy = true;
    m_wantsPlayback = playWhenReady;
    m_playbackError.clear();
    if (!switching) {
        m_preview = false;
        m_recoveryScheduled = true;
        m_media.setSource({});
        m_audioCache.releaseStream();
        m_playingAudio = {};
        m_positionBase = 0;
        m_durationBeforeSeek = 0;
        emit playbackDurationChanged();
        m_recoveryScheduled = false;
        emit playbackSeekableChanged();
    }
    emit playbackPositionChanged();
    emit playbackChanged();

    const bool progressive = !switching && resumePosition < 0 && playWhenReady;
    m_audioCache.prepare(currentTrack().value("id").toString(), m_streamBitrate, progressive,
        [this, revision, switching](AudioCache::Asset asset, const QString &error) {
            if (revision != m_playbackRevision || (switching && m_recoveryScheduled))
                return;
            m_playbackBusy = false;
            if (!asset.url.isValid()) {
                if (switching && !m_media.source().isEmpty() && m_playbackError.isEmpty()) {
                    if (m_playingAudio.requestedBitrate)
                        m_streamBitrate = m_playingAudio.requestedBitrate;
                    emit notification(tr("备用音源暂不可用，继续使用当前音源。"));
                    prefetchNextTrack();
                    emit playbackChanged();
                    return;
                }
                const int skipTo = m_wantsPlayback && m_skipUnavailable && m_skipsRemaining > 0
                    ? stepIndex(m_skipDirection, Step::Skip) : -1;
                if (skipTo >= 0) {
                    --m_skipsRemaining;
                    startQueueIndex(skipTo);
                    return;
                }
                m_playbackError = !error.isEmpty() ? error
                                  : loggedIn() ? tr("该歌曲暂未提供可用播放地址。")
                                               : tr("访客暂无此歌曲的播放权限。可以扫码登录或换一首。");
                m_wantsPlayback = false;
                emit playbackChanged();
                return;
            }
            m_streamBitrate = asset.requestedBitrate;
            if (switching && !asset.md5.isEmpty() && asset.md5 == m_playingAudio.md5) {
                m_playingAudio.requestedBitrate = asset.requestedBitrate;
                prefetchNextTrack();
                emit playbackChanged();
                return;
            }
            if (switching)
                m_resumePosition = playbackPosition();
            m_positionBase = 0;
            m_durationBeforeSeek = 0;
            emit playbackDurationChanged();
            const bool resume = m_resumePosition >= 0;
            m_playbackBusy = resume;
            m_preview = asset.preview;
            // Keep the old file alive until Qt has switched sources.
            const auto previous = std::exchange(m_playingAudio, std::move(asset));
            m_media.setSource(m_playingAudio.url);
            if (switching)
                m_audioCache.releaseStream();
            if (m_playingAudio.stream) {
                connect(m_playingAudio.stream.get(), &AudioStream::bufferedBytesChanged, this, [this] {
                    if (!m_bufferUpdate.isActive())
                        m_bufferUpdate.start();
                });
                connect(m_playingAudio.stream.get(), &AudioStream::completed, this, [this] {
                    emit bufferedRangesChanged();
                    emit playbackSeekableChanged();
                    if (m_media.isPlaying())
                        prefetchNextTrack();
                });
            }
            emit bufferedRangesChanged();
            emit playbackSeekableChanged();
            if (!resume && !m_recoveryScheduled && m_wantsPlayback)
                m_media.play();
            emit playbackPositionChanged();
            emit playbackChanged();
        });
}

void Library::handlePlaybackError(QMediaPlayer::Error error)
{
    const auto source = m_media.source();
    if (error == QMediaPlayer::NoError || source.isEmpty() || m_recoveryScheduled)
        return;

    const auto position = playbackPosition();
    const bool networkSource = source.scheme() == "https" || source.scheme() == "http";
    const bool transient = error == QMediaPlayer::NetworkError || error == QMediaPlayer::ResourceError
        || (error == QMediaPlayer::FormatError && position > 0);
    constexpr int maximumRetries = 2;
    const bool retry = m_wantsPlayback && networkSource && transient && m_streamRetries < maximumRetries;
    const int skipTo = !retry && m_wantsPlayback && m_skipUnavailable && m_skipsRemaining > 0
        ? stepIndex(m_skipDirection, Step::Skip) : -1;
    const bool skip = skipTo >= 0;
    m_resumePosition = position;
    m_recoveryScheduled = true;
    m_playbackBusy = retry || skip;
    if (!retry && !skip) {
        m_wantsPlayback = false;
        m_playbackError = tr("音频中断，可从当前位置重试或切换音源。");
    }
    qWarning() << "Audio stream interrupted at" << position << "ms, media error" << error
               << (retry ? "reconnecting" : skip ? "advancing to next track" : "stopping");
    emit playbackPositionChanged();
    emit playbackChanged();

    // Unload after the backend's error callback returns. One broken stream can emit many errors.
    const int revision = m_playbackRevision;
    QTimer::singleShot(0, this, [this, revision, retry, skip, skipTo] {
        if (revision != m_playbackRevision)
            return;
        if (skip) {
            --m_skipsRemaining;
            startQueueIndex(skipTo);
            return;
        }
        m_media.setSource({});
        m_audioCache.releaseStream();
        m_playingAudio = {};
        emit bufferedRangesChanged();
        emit playbackSeekableChanged();
        if (!retry) {
            m_recoveryScheduled = false;
            return;
        }
        const int delay = ++m_streamRetries == 1 ? 250 : 800;
        if (m_streamRetries > 1)
            m_streamBitrate = fallbackStreamBitrate;
        QTimer::singleShot(delay, this, [this, revision] {
            if (revision == m_playbackRevision)
                requestTrackSource(m_resumePosition);
        });
    });
}

void Library::togglePlayback()
{
    if (m_playbackBusy)
        return;
    if (m_queueIndex < 0) {
        playNewMusic();
        return;
    }
    if (m_resumePosition >= 0) {
        m_streamRetries = 0;
        requestTrackSource(m_resumePosition);
    } else if (m_media.playbackState() == QMediaPlayer::PlayingState) {
        m_wantsPlayback = false;
        m_media.pause();
    } else if (m_media.source().isEmpty() || !m_playbackError.isEmpty()) {
        playIndex(m_queueIndex);
    } else {
        if (m_media.mediaStatus() == QMediaPlayer::EndOfMedia) {
            if (m_positionBase > 0) {
                requestTrackSource(-1);
                return;
            }
            m_media.setPosition(0);
        }
        m_wantsPlayback = true;
        m_media.play();
    }
}

void Library::switchAudioStream()
{
    if (m_queueIndex < 0 || m_playbackBusy)
        return;
    m_streamBitrate = m_streamBitrate == fallbackStreamBitrate ? normalStreamBitrate : fallbackStreamBitrate;
    m_streamRetries = 0;
    requestTrackSource(playbackPosition(), m_wantsPlayback || !m_playbackError.isEmpty(), m_playbackError.isEmpty());
}

bool Library::canSeekPlayback() const
{
    return m_media.isSeekable() || (m_playingAudio.stream && m_playingAudio.stream->isComplete());
}

void Library::seekPlayback(qint64 position)
{
    if (m_playbackBusy || !canSeekPlayback())
        return;
    const auto originalDuration = playbackDuration();
    if (originalDuration > 0)
        position = qBound(qint64{0}, position, originalDuration - 1);
    const auto stream = m_playingAudio.stream;
    if (!stream) {
        m_media.setPosition(position);
        return;
    }

    if (!stream->isComplete()) {
        const auto offset = stream->offsetForTime(position);
        if (offset < 0 || (position >= m_positionBase && stream->bufferedEnd(offset) > offset)) {
            m_media.setPosition(qMax(qint64{0}, position - m_positionBase));
            return;
        }
        // Use a fresh view of the same sparse cache, beginning at the seek target.
        // This avoids a sequential read through the gap in the old HTTP response.
        m_positionBase = position;
        m_durationBeforeSeek = originalDuration;
        m_playingAudio.url = stream->url(offset);
    } else {
        m_positionBase = 0;
        m_durationBeforeSeek = 0;
        m_playingAudio.url = QUrl::fromLocalFile(m_playingAudio.file->fileName());
        m_playingAudio.stream.reset();
    }

    m_resumePosition = position;
    m_playbackBusy = true;
    m_recoveryScheduled = true;
    m_media.setSource({});
    m_recoveryScheduled = false;
    m_media.setSource(m_playingAudio.url);
    emit playbackDurationChanged();
    emit playbackSeekableChanged();
    emit playbackPositionChanged();
    emit playbackChanged();
}

// Play modes: 0 list loop, 1 single loop, 2 shuffle, 3 sequential (stop after the last track).
// Shuffle walks a shuffled permutation of the queue, so "previous" returns to the track
// that actually played before and nothing repeats until every track has played once.
void Library::resetShuffle(bool keepCurrentFirst)
{
    m_shuffleOrder.clear();
    for (int index = 0; index < m_queue.size(); ++index) {
        if (!keepCurrentFirst || index != m_queueIndex)
            m_shuffleOrder.append(index);
    }
    std::shuffle(m_shuffleOrder.begin(), m_shuffleOrder.end(), *QRandomGenerator::global());
    if (keepCurrentFirst && m_queueIndex >= 0 && m_queueIndex < m_queue.size())
        m_shuffleOrder.prepend(m_queueIndex);
}

int Library::stepIndex(int direction, Step step)
{
    const int count = m_queue.size();
    if (count == 0 || m_queueIndex < 0 || m_queueIndex >= count)
        return -1;
    if (m_roamPlaying) {
        const int target = m_queueIndex + direction;
        return target >= 0 && target < count ? target : -1;
    }
    if (step == Step::Automatic && m_playMode == 1)
        return m_queueIndex;
    if (count == 1)
        return step == Step::Manual && m_playMode != 3 ? 0 : -1;

    const bool wrap = m_playMode != 3 || step == Step::Manual;
    if (m_playMode == 2) {
        if (m_shuffleOrder.size() != count)
            resetShuffle();
        int position = m_shuffleOrder.indexOf(m_queueIndex);
        if (position < 0) {
            resetShuffle();
            position = 0;
        }
        int next = position + direction;
        if (next >= count) {
            // A full round has played: start a fresh shuffle that avoids an immediate repeat.
            resetShuffle(false);
            if (m_shuffleOrder.first() == m_queueIndex)
                m_shuffleOrder.swapItemsAt(0, count - 1);
            m_shuffleOrder.removeOne(m_queueIndex);
            m_shuffleOrder.prepend(m_queueIndex);
            next = 1;
        } else if (next < 0) {
            next = count - 1;
        }
        return m_shuffleOrder.at(next);
    }
    int target = m_queueIndex + direction;
    if (target >= count)
        target = wrap ? 0 : -1;
    else if (target < 0)
        target = wrap ? count - 1 : -1;
    return target;
}

bool Library::hasPrevious() const
{
    return !m_roamPlaying ? m_queue.size() > 1 || (m_queueIndex >= 0 && m_playMode != 3)
                          : m_queueIndex > 0;
}

bool Library::hasNext() const
{
    if (m_roamPlaying)
        return m_queueIndex >= 0;
    if (m_queueIndex < 0)
        return false;
    return m_playMode == 3 ? m_queueIndex + 1 < m_queue.size() : !m_queue.isEmpty();
}

void Library::previous()
{
    // Like the web client: always go to the previous track in the current play order.
    const int target = stepIndex(-1, Step::Manual);
    if (target >= 0 && target != m_queueIndex) {
        m_skipUnavailable = true;
        m_skipsRemaining = 8;
        m_skipDirection = -1;
        startQueueIndex(target);
    }
    else if (canSeekPlayback())
        seekPlayback(0);
}

void Library::next()
{
    advance(false);
}

void Library::advance(bool automatic)
{
    if (m_queue.isEmpty())
        return;
    if (m_roamPlaying && m_queueIndex + 1 >= m_queue.size()) {
        if (!m_playbackBusy)
            extendRoamQueue();
        return;
    }
    const int target = stepIndex(1, automatic ? Step::Automatic : Step::Manual);
    if (target < 0) {
        // Sequential mode finished the list.
        emit playbackChanged();
        return;
    }
    if (target == m_queueIndex && automatic && m_positionBase == 0 && m_playingAudio.url.isValid()
        && m_media.isSeekable()) {
        // Single loop: replay the loaded audio without fetching it again.
        m_media.setPosition(0);
        m_wantsPlayback = true;
        m_media.play();
        emit playbackPositionChanged();
        return;
    }
    m_skipUnavailable = true;
    m_skipsRemaining = 8;
    m_skipDirection = 1;
    startQueueIndex(target);
}

void Library::cyclePlayMode()
{
    setPlayMode((m_playMode + 1) % 4);
}

void Library::setPlayMode(int mode)
{
    if (mode < 0 || mode > 3 || mode == m_playMode)
        return;
    m_playMode = mode;
    QSettings().setValue("playMode", m_playMode);
    if (m_playMode == 2)
        resetShuffle();
    emit playModeChanged();
    emit queueChanged();
    prefetchNextTrack();
    static const char *const names[]{"列表循环", "单曲循环", "随机播放", "顺序播放"};
    emit notification(QString::fromUtf8(names[m_playMode]));
}

void Library::playNext(const QVariantMap &track)
{
    const auto id = track.value("id").toString();
    if (id.isEmpty())
        return;
    if (m_queue.isEmpty() || m_queueIndex < 0) {
        playAll({track});
        return;
    }
    if (currentTrack().value("id").toString() == id)
        return;
    for (int index = 0; index < m_queue.size(); ++index) {
        if (m_queue.at(index).toMap().value("id").toString() == id) {
            m_queue.removeAt(index);
            if (index < m_queueIndex)
                --m_queueIndex;
            break;
        }
    }
    m_queue.insert(m_queueIndex + 1, track);
    if (m_playMode == 2 && !m_shuffleOrder.isEmpty()) {
        const int inserted = m_queueIndex + 1;
        for (auto &index : m_shuffleOrder) {
            if (index >= inserted)
                ++index;
        }
        m_shuffleOrder.insert(m_shuffleOrder.indexOf(m_queueIndex) + 1, inserted);
    }
    emit queueChanged();
    prefetchNextTrack();
    emit notification(tr("已添加到下一首播放。"));
}

void Library::removeFromQueue(int index)
{
    if (index < 0 || index >= m_queue.size() || index == m_queueIndex)
        return;
    m_queue.removeAt(index);
    if (index < m_queueIndex)
        --m_queueIndex;
    m_shuffleOrder.removeOne(index);
    for (auto &entry : m_shuffleOrder) {
        if (entry > index)
            --entry;
    }
    emit queueChanged();
}

void Library::clearQueue()
{
    if (m_queue.isEmpty())
        return;
    const auto current = currentTrack();
    m_queue.clear();
    m_queueIndex = -1;
    if (!current.isEmpty()) {
        m_queue.append(current);
        m_queueIndex = 0;
    }
    m_roamPlaying = false;
    resetShuffle();
    emit queueChanged();
    emit playbackChanged();
}

void Library::playSongById(const QString &id)
{
    if (id.isEmpty())
        return;
    get("/song/detail", {{"ids", id}}, [this](const QJsonObject &body, const QString &error) {
        const auto tracks = tracksFrom(body.value("songs").toArray());
        if (!error.isEmpty() || tracks.isEmpty()) {
            emit notification(tr("歌曲暂时无法播放。"));
            return;
        }
        const auto track = tracks.first().toMap();
        if (m_queue.isEmpty() || m_queueIndex < 0 || m_roamPlaying) {
            playAll({track});
            return;
        }
        const auto trackId = track.value("id").toString();
        m_queue.removeIf([&](const QVariant &item) {
            return item.toMap().value("id").toString() == trackId
                   && currentTrack().value("id").toString() != trackId;
        });
        int index = -1;
        for (int row = 0; row < m_queue.size(); ++row) {
            if (m_queue.at(row).toMap().value("id").toString() == currentTrack().value("id").toString())
                index = row;
        }
        m_queueIndex = index;
        if (currentTrack().value("id").toString() == trackId) {
            playIndex(m_queueIndex);
            return;
        }
        m_queue.insert(m_queueIndex + 1, track);
        m_queueIndex += 1;
        resetShuffle();
        playIndex(m_queueIndex);
    });
}

void Library::loadSimilar()
{
    const auto id = currentTrack().value("id").toString();
    if (id == m_similarTrackId)
        return;
    m_similarTrackId = id;
    m_similarSongs.clear();
    m_similarPlaylists.clear();
    emit similarChanged();
    if (id.isEmpty())
        return;
    get("/simi/song", {{"id", id}}, [this, id](const QJsonObject &body, const QString &error) {
        if (id != m_similarTrackId || !error.isEmpty())
            return;
        m_similarSongs = tracksFrom(body.value("songs").toArray());
        emit similarChanged();
    });
    get("/simi/playlist", {{"id", id}}, [this, id](const QJsonObject &body, const QString &error) {
        if (id != m_similarTrackId || !error.isEmpty())
            return;
        m_similarPlaylists = playlistsFrom(body.value("playlists").toArray());
        emit similarChanged();
    });
}

QVariantList Library::outputDevices() const
{
    QVariantList devices;
    for (const auto &device : QMediaDevices::audioOutputs()) {
        devices.append(QVariantMap{
            {"id", QString::fromLatin1(device.id().toHex())},
            {"label", device.description()}
        });
    }
    return devices;
}

QString Library::outputName() const
{
    const auto output = m_audio.device();
    return output.isNull() ? tr("未发现音频输出设备") : output.description();
}

void Library::selectOutput(const QString &id)
{
    m_outputId = id;
    QSettings().setValue("audioOutput", id);
    applyOutput();
}

void Library::applyOutput()
{
    QAudioDevice selected;
    if (!m_outputId.isEmpty()) {
        for (const auto &device : QMediaDevices::audioOutputs()) {
            if (QString::fromLatin1(device.id().toHex()) == m_outputId) {
                selected = device;
                break;
            }
        }
    }
    m_audio.setDevice(selected.isNull() ? QMediaDevices::defaultAudioOutput() : selected);
    emit outputChanged();
}

void Library::toggleMute()
{
    if (m_audio.volume() == 0) {
        m_audio.setVolume(0.55);
        m_audio.setMuted(false);
    } else {
        m_audio.setMuted(!m_audio.isMuted());
    }
}

void Library::setLoginStatus(const QString &message, bool busy)
{
    m_loginStatus = message;
    m_loginBusy = busy;
    emit loginChanged();
}

void Library::beginQrLogin()
{
    closeQrLogin();
    if (!localAccountSupported() || !backendReady()) {
        setLoginStatus(tr("本地服务未就绪，请先重试连接。"), false);
        return;
    }
    const int revision = m_loginRevision;
    setLoginStatus(tr("正在生成二维码…"), true);
    get("/account/qr", {}, [this, revision](const QJsonObject &body, const QString &error) {
        if (revision != m_loginRevision)
            return;
        const QString image = text(body, "image");
        if (!error.isEmpty() || !image.startsWith("data:image/png;base64,")) {
            setLoginStatus(tr("二维码获取失败，请刷新重试。"), false);
            return;
        }
        m_loginImage = QUrl(image);
        setLoginStatus(tr("用网易云音乐 App 扫码"), false);
        m_loginPoll.start(3000);
    });
}

void Library::closeQrLogin()
{
    ++m_loginRevision;
    m_loginPoll.stop();
    m_loginImage.clear();
    setLoginStatus({}, false);
}

void Library::pollLogin()
{
    const int revision = m_loginRevision;
    get("/account/qr/check", {}, [this, revision](const QJsonObject &body, const QString &error) {
        if (revision != m_loginRevision)
            return;
        if (!error.isEmpty()) {
            setLoginStatus(tr("暂时无法确认扫码状态，正在重试。"), false);
            m_loginPoll.start(6500);
            return;
        }
        switch (body.value("phase").toInt()) {
        case 801:
            setLoginStatus(tr("用网易云音乐 App 扫码"), false);
            m_loginPoll.start(3000);
            break;
        case 802:
            setLoginStatus(tr("已扫码，请在手机上确认登录"), false);
            m_loginPoll.start(3000);
            break;
        case 803:
            m_loginImage.clear();
            setLoginStatus(tr("正在读取账号…"), true);
            loadSession(true);
            break;
        default:
            m_loginImage.clear();
            setLoginStatus(tr("二维码已过期，点刷新重新生成。"), false);
            break;
        }
    });
}

void Library::loadSession(bool afterLogin, bool initial)
{
    if (!localAccountSupported()) {
        if (initial)
            refreshDiscovery();
        return;
    }
    if (m_sessionLoading)
        return;

    m_sessionLoading = true;
    get("/account/session", {}, [this, afterLogin, initial](const QJsonObject &body, const QString &error) {
        m_sessionLoading = false;
        if (!error.isEmpty()) {
            if (afterLogin)
                setLoginStatus(tr("已确认扫码，暂时无法读取账号。请重试。"), false);
            if (initial)
                refreshDiscovery();
            return;
        }
        const auto profile = body.value("profile").toObject();
        const bool switched = profile.toVariantMap().value("userId") != m_profile.value("userId");
        if (switched) {
            clearPreparedAudio();
            dropCachedResponses();
        }
        m_profile = profile.toVariantMap();
        if (switched)
            m_store.setScope(m_profile.value("userId").toString());
        QSettings().setValue("lastUserId", m_profile.value("userId").toString());
        if (loggedIn()) {
            m_profile["avatarUrl"] = coverUrl(text(profile, "avatarUrl"));
            closeQrLogin();
            loadPersonalLists();
            loadLikedSongs();
            if (switched || initial)
                refreshDiscovery();
            if (m_sectionNeedsLogin)
                browse(m_section);
        } else {
            if (initial)
                refreshDiscovery();
            if (afterLogin)
                setLoginStatus(tr("登录会话尚未生效，请重新扫码。"), false);
        }
        emit accountChanged();
    });
}

void Library::loadPersonalLists()
{
    if (!loggedIn() || m_personalListsBusy)
        return;
    m_personalListsBusy = true;
    const int revision = ++m_personalRevision;
    get("/account/playlists", {{"limit", "100"}}, [this, revision](const QJsonObject &body, const QString &error) {
        if (revision != m_personalRevision || !loggedIn())
            return;
        m_personalListsBusy = false;
        m_personalListsLoaded = error.isEmpty();
        if (!error.isEmpty()) {
            if (m_section == "liked" || m_section == "collections" || m_section == "created")
                finishSection(m_sectionRevision, {}, {}, error);
            return;
        }
        const QString owner = m_profile.value("userId").toString();
        m_createdPlaylists.clear();
        m_savedPlaylists.clear();
        m_likedPlaylist.clear();
        for (const auto &row : playlistsFrom(body.value("playlist").toArray())) {
            const auto item = row.toMap();
            if (item.value("ownerId").toString() != owner) {
                m_savedPlaylists.append(item);
                continue;
            }
            if (item.value("specialType").toInt() == 5) {
                m_likedPlaylist = item;
                continue;
            }
            m_createdPlaylists.append(item);
        }
        emit personalListsChanged();
        if (m_section == "liked" || m_section == "collections" || m_section == "created")
            browse(m_section);
    });
}

void Library::logout()
{
    closeQrLogin();
    get("/account/logout", {}, [this](const QJsonObject &, const QString &error) {
        if (!error.isEmpty()) {
            emit notification(tr("退出登录失败，请重试。"));
            return;
        }
        ++m_personalRevision;
        clearPreparedAudio();
        dropCachedResponses();
        m_profile.clear();
        m_likedIds.clear();
        ++m_likesRevision;
        m_roamPlaying = false;
        emit likesChanged();
        emit playbackChanged();
        m_createdPlaylists.clear();
        m_savedPlaylists.clear();
        m_likedPlaylist.clear();
        m_personalListsLoaded = false;
        m_personalListsBusy = false;
        emit accountChanged();
        emit personalListsChanged();
        m_recommendations.clear();
        refreshDiscovery();
        if (!m_section.isEmpty())
            browse(m_section);
    }, true);
}

void Library::createPlaylist(const QString &name)
{
    if (!loggedIn() || name.trimmed().isEmpty() || name.trimmed().size() > 40)
        return;
    get("/account/playlists/create", {{"name", encoded(name.trimmed())}}, [this](const QJsonObject &, const QString &error) {
        if (error.isEmpty()) {
            emit notification(tr("歌单已创建。"));
            loadPersonalLists();
        } else {
            emit notification(tr("创建歌单失败，请重试。"));
        }
    }, true);
}

void Library::rememberCurrentTrack()
{
    const auto track = currentTrack();
    if (track.isEmpty())
        return;
    const auto id = track.value("id");
    m_recentTracks.removeIf([&id](const QVariant &item) { return item.toMap().value("id") == id; });
    m_recentTracks.prepend(track);
    if (m_recentTracks.size() > 50)
        m_recentTracks.removeLast();
    QSettings().setValue("recentTracks", m_recentTracks);
}

void Library::selectPlaylistCategory(const QString &category)
{
    if (category == m_playlistCategory || !m_playlistCategories.contains(category))
        return;
    m_playlistCategory = category;
    emit categoriesChanged();
    browse("featured");
}

void Library::loadPlaylistCategories()
{
    if (m_categoriesBusy || m_playlistCategories.size() > 1)
        return;
    m_categoriesBusy = true;
    get("/playlist/catlist", {}, [this](const QJsonObject &body, const QString &error) {
        m_categoriesBusy = false;
        if (!error.isEmpty())
            return;
        QStringList categories{QStringLiteral("全部")};
        const auto items = body.value("sub").toArray();
        const auto groupNames = body.value("categories").toObject();
        QMap<int, QStringList> grouped;
        for (const auto &value : items) {
            const auto name = text(value.toObject(), "name");
            if (!name.isEmpty() && !categories.contains(name)) {
                categories.append(name);
                grouped[value.toObject().value("category").toInt()].append(name);
            }
        }
        m_playlistCategoryGroups.clear();
        for (auto it = grouped.cbegin(); it != grouped.cend(); ++it)
            m_playlistCategoryGroups.append(QVariantMap{
                {"title", groupNames.value(QString::number(it.key())).toString()}, {"items", it.value()}});
        m_playlistCategories = categories;
        emit categoriesChanged();
    });
}

void Library::browse(const QString &section)
{
    // The input can alias m_section when a session refreshes its current page.
    m_section = section;
    const int revision = ++m_sectionRevision;
    m_sectionItems.clear();
    m_sectionShelves.clear();
    m_sectionTracks.clear();
    m_sectionError.clear();
    const bool guestPage = section == "essential" || section == "officialPlaylists" || section == "essentialPodcasts"
        || section == "artists" || section == "featured" || section == "charts" || section == "albums"
        || section == "newTracks" || section == "podcasts" || section.startsWith("podcast")
        || section == "vip" || section == "vipCharts" || section == "recent" || section.startsWith("scene");
    m_sectionNeedsLogin = !guestPage && !loggedIn();
    m_sectionBusy = !m_sectionNeedsLogin;
    emit sectionChanged();
    if (m_sectionNeedsLogin)
        return;

    QString route;
    QUrlQuery query;
    if (section == "essential") {
        route = "/essential/overview";
    } else if (section == "officialPlaylists") {
        route = "/essential/official";
    } else if (section == "essentialPodcasts") {
        route = "/essential/podcasts";
    } else if (section == "artists") {
        const QHash<QString, QString> areas{{"华语", "7"}, {"欧美", "96"}, {"日本", "8"}, {"韩国", "16"}};
        const QHash<QString, QString> types{{"男歌手", "1"}, {"女歌手", "2"}, {"乐队组合", "3"}};
        route = "/artists";
        query.addQueryItem("area", areas.value(m_curatedArea, m_curatedArea == "其他" ? "0" : "-1"));
        query.addQueryItem("type", types.value(m_artistType, "-1"));
        query.addQueryItem("initial", m_artistInitial == "热门" ? "-1" : m_artistInitial == "#" ? "0" : m_artistInitial);
        query.addQueryItem("limit", "60");
    } else if (section == "featured") {
        route = "/top/playlist";
        query.addQueryItem("limit", "60");
        query.addQueryItem("cat", encoded(m_playlistCategory));
        loadPlaylistCategories();
    } else if (section == "charts") {
        route = "/toplist/groups";
    } else if (section == "albums") {
        const QHash<QString, QString> areas{{"华语", "ZH"}, {"欧美", "EA"}, {"日本", "JP"}, {"韩国", "KR"}};
        route = m_curatedArea == "全部" ? "/album/newest" : "/top/album";
        if (route == "/top/album") {
            query.addQueryItem("area", areas.value(m_curatedArea));
            query.addQueryItem("limit", "25");
        }
    } else if (section == "newTracks") {
        const QHash<QString, QString> areas{{"全部", "0"}, {"华语", "7"}, {"欧美", "96"}, {"日本", "8"}, {"韩国", "16"}};
        route = "/top/song";
        query.addQueryItem("type", areas.value(m_curatedArea, "0"));
    } else if (section == "vip") {
        route = "/vip/playlists";
    } else if (section == "vipCharts") {
        route = "/vip/charts";
    } else if (section == "vipSongs") {
        route = "/account/vip/songs";
    } else if (section == "daily") {
        route = "/account/daily";
    } else if (section == "sceneTags") {
        route = "/scene/tags";
    } else if (section == "scenePlaylists") {
        route = "/scene/playlists";
        query.addQueryItem("id", m_sceneCategory);
    } else if (section == "podcasts") {
        route = "/dj/recommend";
    } else if (section == "podcastPrograms") {
        route = "/dj/program/recommend";
        query.addQueryItem("limit", "24");
    } else if (section == "podcastCategories") {
        route = "/dj/categories";
    } else if (section == "podcastCategory") {
        route = "/dj/by-category";
        query.addQueryItem("type", m_podcastCategory);
    } else if (section == "podcastTop" || section == "podcastNew") {
        route = "/dj/toplist";
        query.addQueryItem("type", section == "podcastTop" ? "hot" : "new");
        query.addQueryItem("limit", "40");
    } else if (section == "recent") {
        if (!loggedIn()) {
            finishSection(revision, {}, m_recentTracks, {});
            return;
        }
        route = "/account/recent";
        query.addQueryItem("limit", "50");
    } else if (section == "myPodcasts") {
        route = "/account/radios";
        query.addQueryItem("limit", "50");
    } else if (section == "following") {
        route = "/account/events";
        query.addQueryItem("pagesize", "25");
    } else if (section == "followingPeople") {
        route = "/account/follows";
        query.addQueryItem("limit", "50");
    } else if (section == "roam") {
        route = "/account/fm";
    } else if (section == "liked" || section == "collections" || section == "created") {
        if (!m_personalListsLoaded) {
            loadPersonalLists();
            return;
        }
        if (section == "collections" || section == "created") {
            finishSection(revision, section == "created" ? m_createdPlaylists : m_savedPlaylists, {}, {});
            return;
        }
        if (m_likedPlaylist.isEmpty()) {
            finishSection(revision, {}, {}, {});
            return;
        }
        route = "/playlist/track/all";
        query.addQueryItem("id", m_likedPlaylist.value("id").toString());
        query.addQueryItem("limit", "100");
    } else {
        static const QHash<QString, QString> accountRoutes{
            {"recentPlaylists", "/account/recent/playlists"},
            {"recentAlbums", "/account/recent/albums"},
            {"recentRadios", "/account/recent/radios"},
            {"recentVoices", "/account/recent/voices"},
            {"recentVideos", "/account/recent/videos"},
            {"createdVoices", "/account/created-voices"},
            {"savedVoices", "/account/podcasts/saved"},
            {"boughtVoices", "/account/podcasts/bought"},
            {"likedVoices", "/account/podcasts/liked"},
            {"myRecentVoices", "/account/recent/voices"},
            {"savedAlbums", "/account/collections/albums"},
            {"savedArtists", "/account/collections/artists"},
            {"savedVideos", "/account/collections/videos"},
        };
        route = accountRoutes.value(section);
        if (route.isEmpty()) {
            finishSection(revision, {}, {}, {});
            return;
        }
        query.addQueryItem("limit", "50");
    }

    const auto selected = m_section;
    // Shared catalog pages are cached; anything that reflects the user's own activity is not.
    static const QSet<QString> live{"roam", "liked", "recent", "following", "followingPeople", "daily",
        "myPodcasts", "savedVoices", "createdVoices", "boughtVoices", "likedVoices", "myRecentVoices",
        "savedAlbums", "savedArtists", "savedVideos", "vipSongs"};
    const bool fresh = live.contains(selected) || selected.startsWith("recent");
    fetch(route, query, fresh ? 0 : catalogMaxAge, [this, selected, revision](const QJsonObject &body, const QString &error) {
        if (revision != m_sectionRevision)
            return;
        QVariantList items;
        QVariantList tracks;
        if (selected == "essential") {
            const auto playlists = playlistsFrom(body.value("official").toArray());
            const auto songs = tracksFrom(body.value("latest").toArray()).mid(0, 9);
            const auto podcasts = essentialPrograms(body.value("podcasts").toArray()).mid(0, 6);
            auto addShelf = [this](const QString &title, const QString &kind, const QString &route, const QVariantList &items) {
                if (!items.isEmpty()) {
                    m_sectionShelves.append(QVariantMap{{"title", title}, {"kind", kind},
                                                        {"route", route}, {"items", items}});
                }
            };
            addShelf(tr("官方歌单"), "playlists", "officialPlaylists", playlists);
            addShelf(tr("最新音乐"), "tracks", "newTracks", songs);
            addShelf(tr("热门播客"), "programs", "essentialPodcasts", podcasts);
        } else if (selected == "featured" || selected == "officialPlaylists") {
            items = playlistsFrom(body.value("playlists").toArray());
        } else if (selected == "essentialPodcasts") {
            items = essentialPrograms(body.value("data").toObject().value("recommendVoiceVOS").toArray());
        } else if (selected == "artists") {
            items = simpleItems(body.value("artists").toArray(), "artist");
        } else if (selected == "charts") {
            m_sectionShelves = chartGroupsFrom(body.value("data").toArray());
        } else if (selected == "albums") {
            auto albums = body.value("albums").toArray();
            if (albums.isEmpty())
                albums = body.value("weekData").toArray();
            if (albums.isEmpty())
                albums = body.value("monthData").toArray();
            items = albumsFrom(albums);
        } else if (selected == "newTracks") {
            tracks = tracksFrom(body.value("data").toArray());
        } else if (selected == "vip") {
            items = simpleItems(itemArray(body.value("data")), "playlist");
        } else if (selected == "vipCharts") {
            items = vipChartsFrom(itemArray(body.value("data")));
        } else if (selected == "vipSongs") {
            tracks = tracksFrom(itemArray(body.value("data")));
        } else if (selected == "daily") {
            const auto songs = body.value("data").toObject().value("dailySongs").toArray();
            tracks = tracksFrom(songs.isEmpty() ? body.value("recommend").toArray() : songs);
        } else if (selected == "sceneTags") {
            for (const auto &entry : body.value("tags").toArray()) {
                const auto tag = entry.toObject();
                if (tag.value("category").toInt() != 2)
                    continue;
                const auto raw = tag.value("playlistTag").toObject();
                const auto id = idOf(raw);
                const auto title = text(raw, "name");
                if (!id.isEmpty() && !title.isEmpty())
                    items.append(QVariantMap{{"id", id}, {"title", title}, {"kind", "sceneCategory"}, {"art", QUrl{}}});
            }
        } else if (selected == "scenePlaylists") {
            items = playlistsFrom(body.value("playlists").toArray());
        } else if (selected == "podcasts" || selected == "myPodcasts" || selected == "podcastCategory") {
            items = radiosFrom(body.value("djRadios").toArray());
        } else if (selected == "podcastTop" || selected == "podcastNew") {
            items = radiosFrom(body.value("toplist").toArray());
        } else if (selected == "podcastPrograms") {
            items = programsFrom(body.value("programs").toArray());
        } else if (selected == "podcastCategories") {
            items = simpleItems(body.value("categories").toArray(), "podcastCategory");
        } else if (selected == "following") {
            const auto events = body.value("data").toObject().value("events").toArray();
            items = eventsFrom(events.isEmpty() ? body.value("event").toArray() : events);
        } else if (selected == "followingPeople") {
            items = peopleFrom(body.value("follow").toArray());
        } else if (selected == "roam") {
            tracks = tracksFrom(body.value("data").toArray());
        } else if (selected == "liked") {
            tracks = tracksFrom(body.value("songs").toArray());
        } else if (selected == "recent") {
            tracks = m_recentTracks;
            for (const auto &row : tracksFrom(body.value("data").toObject().value("list").toArray())) {
                const auto id = row.toMap().value("id");
                const bool exists = std::any_of(tracks.cbegin(), tracks.cend(), [&id](const QVariant &item) {
                    return item.toMap().value("id") == id;
                });
                if (!exists)
                    tracks.append(row);
            }
        } else if (selected == "recentPlaylists") {
            items = playlistsFrom(recentObjects(body));
        } else if (selected == "recentAlbums") {
            items = albumsFrom(recentObjects(body));
        } else if (selected == "recentRadios") {
            items = radiosFrom(recentObjects(body));
        } else if (selected == "recentVoices" || selected == "myRecentVoices") {
            items = programsFrom(recentObjects(body));
        } else if (selected == "recentVideos") {
            items = simpleItems(recentObjects(body), "video");
        } else if (selected == "createdVoices" || selected == "savedVoices") {
            items = simpleItems(itemArray(body.value("data")), "voiceList");
        } else if (selected == "boughtVoices") {
            items = voicePurchasesFrom(itemArray(body.value("data")));
        } else if (selected == "likedVoices") {
            items = programsFrom(itemArray(body.value("data")));
        } else if (selected == "savedAlbums") {
            items = albumsFrom(body.value("data").toArray());
        } else if (selected == "savedArtists") {
            items = simpleItems(body.value("data").toArray(), "artist");
        } else if (selected == "savedVideos") {
            items = simpleItems(body.value("data").toArray(), "video");
        }
        finishSection(revision, items, tracks, error);
    });
}

void Library::finishSection(int revision, const QVariantList &items, const QVariantList &tracks, const QString &error)
{
    if (revision != m_sectionRevision)
        return;
    m_sectionItems = items;
    m_sectionTracks = tracks;
    m_sectionError = error;
    m_sectionBusy = false;
    emit sectionChanged();
}

QVariantList Library::homeShelves() const
{
    QVariantList shelves = m_homeShelves;
    shelves.append(m_radarShelves);
    return shelves;
}

void Library::selectCuratedArea(const QString &area)
{
    if (area == m_curatedArea || !QStringList{"全部", "华语", "欧美", "日本", "韩国", "其他"}.contains(area))
        return;
    m_curatedArea = area;
    emit categoriesChanged();
    if (m_section == "newTracks" || m_section == "albums" || m_section == "artists")
        browse(m_section);
}

void Library::selectArtistType(const QString &type)
{
    if (type == m_artistType || !QStringList{"全部", "男歌手", "女歌手", "乐队组合"}.contains(type))
        return;
    m_artistType = type;
    emit categoriesChanged();
    if (m_section == "artists")
        browse(m_section);
}

void Library::browsePodcastCategory(const QVariantMap &category)
{
    m_podcastCategory = category.value("id").toString();
    m_podcastCategoryName = category.value("title").toString();
    if (!m_podcastCategory.isEmpty())
        browse("podcastCategory");
}

void Library::browseSceneCategory(const QVariantMap &category)
{
    m_sceneCategory = category.value("id").toString();
    m_sceneName = category.value("title").toString();
    if (!m_sceneCategory.isEmpty())
        browse("scenePlaylists");
}

bool Library::currentLiked() const
{
    return m_likedIds.contains(currentTrack().value("id").toString());
}

void Library::loadLikedSongs()
{
    if (!loggedIn())
        return;
    get("/account/likes", {}, [this](const QJsonObject &body, const QString &error) {
        if (!loggedIn() || !error.isEmpty())
            return;
        m_likedIds.clear();
        for (const auto &id : body.value("ids").toArray()) {
            const auto value = idOf(QJsonObject{{"id", id}});
            if (!value.isEmpty())
                m_likedIds.insert(value);
        }
        m_playlistModel.likesChanged();
        ++m_likesRevision;
        emit likesChanged();
    });
}

void Library::toggleLike()
{
    toggleLikeTrack(currentTrack().value("id").toString());
}

bool Library::isLiked(const QString &id) const
{
    return m_likedIds.contains(id);
}

void Library::toggleLikeTrack(const QString &id)
{
    if (!loggedIn() || id.isEmpty())
        return;
    const bool liked = !m_likedIds.contains(id);
    get("/account/like", {{"id", id}, {"like", liked ? "true" : "false"}},
        [this, id, liked](const QJsonObject &, const QString &error) {
            if (!error.isEmpty()) {
                emit notification(tr("更新喜欢状态失败，请重试。"));
                return;
            }
            if (!loggedIn())
                return;
            if (liked)
                m_likedIds.insert(id);
            else
                m_likedIds.remove(id);
            m_playlistModel.likesChanged(id);
            ++m_likesRevision;
            emit likesChanged();
            emit notification(liked ? tr("已添加到我喜欢的音乐。") : tr("已取消喜欢。"));
        }, true);
}

void Library::startRoam()
{
    if (!loggedIn())
        return;
    if (m_roamPlaying && m_queueIndex >= 0) {
        if (m_media.playbackState() != QMediaPlayer::PlayingState && !m_playbackBusy)
            togglePlayback();
        return;
    }
    get("/account/fm", {}, [this](const QJsonObject &body, const QString &error) {
        const auto songs = tracksFrom(body.value("data").toArray());
        if (!error.isEmpty() || songs.isEmpty()) {
            emit notification(error.isEmpty() ? tr("漫游暂时没有歌曲。") : error);
            return;
        }
        m_queue = songs;
        m_roamPlaying = true;
        m_skipUnavailable = true;
        m_skipsRemaining = 8;
        m_skipDirection = 1;
        startQueueIndex(0);
    });
}

void Library::selectArtistInitial(const QString &initial)
{
    static const QRegularExpression letter(QStringLiteral("^[A-Z]$"));
    if (initial == m_artistInitial || !(initial == "热门" || initial == "#" || letter.match(initial).hasMatch()))
        return;
    m_artistInitial = initial;
    emit categoriesChanged();
    if (m_section == "artists")
        browse(m_section);
}

QVariantList Library::radarPlaylists() const
{
    for (const auto &shelf : m_radarShelves) {
        const auto items = shelf.toMap().value("items").toList();
        if (shelf.toMap().value("kind") == "playlists" && !items.isEmpty())
            return items;
    }
    return {};
}

void Library::playSection(int startIndex)
{
    if (startIndex < 0 || startIndex >= m_sectionTracks.size())
        return;
    if (m_section != "roam") {
        if (startIndex == 0)
            playAll(m_sectionTracks);
        else
            playCollection(m_sectionTracks, startIndex);
        return;
    }
    m_queue = m_sectionTracks;
    m_roamPlaying = true;
    m_skipUnavailable = true;
    m_skipsRemaining = 8;
    m_skipDirection = 1;
    startQueueIndex(startIndex);
}

void Library::extendRoamQueue()
{
    if (!loggedIn() || !m_roamPlaying)
        return;
    const int revision = m_playbackRevision;
    m_playbackBusy = true;
    emit playbackChanged();
    get("/account/fm", {}, [this, revision](const QJsonObject &body, const QString &error) {
        if (revision != m_playbackRevision)
            return;
        m_playbackBusy = false;
        if (!m_roamPlaying) {
            emit playbackChanged();
            return;
        }
        auto songs = tracksFrom(body.value("data").toArray());
        if (songs.isEmpty() || !error.isEmpty()) {
            m_roamPlaying = false;
            m_playbackError = error.isEmpty() ? tr("漫游暂时没有更多歌曲。") : error;
            emit playbackChanged();
            return;
        }
        const auto nextIndex = m_queue.size();
        m_queue.append(songs);
        m_skipUnavailable = true;
        m_skipsRemaining = 8;
        m_skipDirection = 1;
        startQueueIndex(nextIndex);
    });
}

void Library::dislikeRoamTrack()
{
    const auto id = currentTrack().value("id").toString();
    if (!m_roamPlaying || id.isEmpty() || m_playbackBusy)
        return;
    get("/account/fm/trash", {{"id", id}}, [this, id](const QJsonObject &, const QString &error) {
        if (!error.isEmpty()) {
            emit notification(tr("提交不喜欢失败，请重试。"));
            return;
        }
        if (m_roamPlaying && currentTrack().value("id").toString() == id)
            next();
    }, true);
}
