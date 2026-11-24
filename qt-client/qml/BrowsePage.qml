pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

ColumnLayout {
    id: page
    required property Library library
    signal playlistChosen(var data)
    signal playlistPlayed(var data)
    signal loginRequested()
    signal postChosen(string author, string message)

    readonly property string sectionKey: library.section
    readonly property bool podcastPage: sectionKey.startsWith("podcast")
    readonly property bool recentPage: sectionKey.startsWith("recent")
    readonly property bool myPodcastPage: ["myPodcasts", "savedVoices", "createdVoices",
        "boughtVoices", "likedVoices", "myRecentVoices"].includes(sectionKey)
    readonly property bool vipPage: ["vip", "vipCharts", "vipSongs"].includes(sectionKey)
    readonly property bool collectionPage: ["collections", "savedAlbums", "savedArtists", "savedVideos"].includes(sectionKey)
    readonly property bool followingPage: ["following", "followingPeople"].includes(sectionKey)
    readonly property bool essentialChild: ["officialPlaylists", "newTracks", "albums", "essentialPodcasts"].includes(sectionKey)
    readonly property bool curated: essentialChild || vipPage || ["essential", "featured", "charts", "artists"].includes(sectionKey)
    readonly property bool shelved: ["essential", "charts"].includes(sectionKey)
    readonly property string tabKey: essentialChild ? "essential" : vipPage ? "vip"
        : sectionKey === "podcastCategory" ? "podcastCategories" : sectionKey
    readonly property bool gallery: ["officialPlaylists", "artists", "featured", "albums", "savedAlbums", "savedArtists",
        "collections", "created", "podcasts", "podcastCategory", "podcastTop", "podcastNew", "myPodcasts",
        "recentPlaylists", "recentAlbums", "recentRadios", "vip", "vipCharts", "scenePlaylists"].includes(sectionKey)
    readonly property string galleryLook: ["artists", "savedArtists"].includes(sectionKey) ? "round"
        : ["albums", "savedAlbums", "recentAlbums", "podcasts", "podcastCategory", "podcastTop", "podcastNew",
           "myPodcasts", "recentRadios"].includes(sectionKey) ? "plain" : "poster"
    readonly property var tabs: curated ? [
        { key: "essential", label: "精选" }, { key: "featured", label: "歌单广场" },
        { key: "charts", label: "排行榜" }, { key: "artists", label: "歌手" },
        { key: "vip", label: "VIP" }
    ] : podcastPage ? [
        { key: "podcasts", label: "推荐播客" }, { key: "podcastPrograms", label: "推荐节目" },
        { key: "podcastCategories", label: "分类" }, { key: "podcastTop", label: "热门榜" },
        { key: "podcastNew", label: "新晋榜" }
    ] : recentPage ? [
        { key: "recent", label: "单曲" }, { key: "recentRadios", label: "播客" },
        { key: "recentVoices", label: "声音" }, { key: "recentPlaylists", label: "歌单" },
        { key: "recentAlbums", label: "专辑" }, { key: "recentVideos", label: "视频" }
    ] : myPodcastPage ? [
        { key: "myPodcasts", label: "收藏播客" }, { key: "savedVoices", label: "收藏播单" },
        { key: "createdVoices", label: "创建播客" }, { key: "boughtVoices", label: "购买播客" },
        { key: "likedVoices", label: "喜欢的声音" }, { key: "myRecentVoices", label: "最近收听" }
    ] : collectionPage ? [
        { key: "savedAlbums", label: "专辑" }, { key: "collections", label: "歌单" },
        { key: "savedArtists", label: "歌手" }, { key: "savedVideos", label: "视频" }
    ] : sectionKey === "scenePlaylists" ? [
        { key: "sceneTags", label: "选择场景" }, { key: "scenePlaylists", label: library.sceneName }
    ] : followingPage ? [
        { key: "following", label: "动态" }, { key: "followingPeople", label: "关注的人" }
    ] : []
    readonly property var areas: ["全部", "华语", "欧美", "日本", "韩国"]
    readonly property var quickCategories: ["全部", "华语", "流行", "摇滚", "民谣", "电子", "轻音乐"]
    readonly property var titles: ({
        daily: "每日推荐", roam: "漫游", following: "关注", liked: "我喜欢的音乐",
        created: "创建的歌单", sceneTags: "场景歌单"
    })
    readonly property string heading: followingPage ? "关注" : titles[sectionKey] || ""
    spacing: 10

    component ChipRow: Flow {
        id: chips
        property var options: []
        property string current
        signal picked(string option)
        Layout.fillWidth: true
        spacing: 10
        Repeater {
            model: chips.options
            delegate: PillButton {
                required property string modelData
                chip: true
                text: modelData
                selected: chips.current === modelData
                onClicked: chips.picked(modelData)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 2
        visible: page.heading.length > 0 || page.library.sectionTracks.length > 0
        spacing: 10
        Label {
            Layout.fillWidth: true
            text: page.heading
            color: Theme.text
            font.pixelSize: 20
            font.weight: Font.Bold
        }
        PillButton {
            visible: page.library.sectionTracks.length > 0
            primary: true
            symbol: "play"
            text: "播放全部"
            onClicked: page.library.playSection(0)
        }
    }

    TabStrip {
        Layout.fillWidth: true
        visible: page.tabs.length > 0
        tabs: page.tabs
        current: page.tabKey
        onChosen: (key) => {
            if (page.sectionKey !== key || page.library.sectionError.length)
                page.library.browse(key)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: page.sectionKey === "featured"
        spacing: 10
        Repeater {
            model: page.quickCategories
            delegate: PillButton {
                required property string modelData
                chip: true
                text: modelData === "全部" ? "推荐" : modelData
                selected: page.library.playlistCategory === modelData
                onClicked: page.library.selectPlaylistCategory(modelData)
            }
        }
        PillButton {
            id: moreCategories
            chip: true
            text: page.quickCategories.includes(page.library.playlistCategory)
                  ? "更多分类" : page.library.playlistCategory
            selected: !page.quickCategories.includes(page.library.playlistCategory)
            onClicked: categoryPopup.opened ? categoryPopup.close() : categoryPopup.open()
            Popup {
                id: categoryPopup
                y: parent.height + 8
                x: -160
                width: 470
                padding: 16
                property int group: 0
                background: Rectangle {
                    radius: 10
                    color: Theme.panel
                    border.color: Theme.line
                }
                contentItem: ColumnLayout {
                    spacing: 14
                    TabStrip {
                        Layout.fillWidth: true
                        textSize: 13
                        tabs: page.library.playlistCategoryGroups.map((group, index) =>
                                  ({ key: String(index), label: group.title }))
                        current: String(categoryPopup.group)
                        onChosen: (key) => categoryPopup.group = Number(key)
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10
                        Repeater {
                            model: page.library.playlistCategoryGroups.length > categoryPopup.group
                                   ? page.library.playlistCategoryGroups[categoryPopup.group].items : []
                            delegate: PillButton {
                                required property string modelData
                                chip: true
                                text: modelData
                                selected: page.library.playlistCategory === modelData
                                onClicked: {
                                    page.library.selectPlaylistCategory(modelData)
                                    categoryPopup.close()
                                }
                            }
                        }
                    }
                }
            }
        }
        Item { Layout.fillWidth: true }
    }

    ChipRow {
        visible: ["artists", "newTracks", "albums"].includes(page.sectionKey)
        options: page.sectionKey === "artists" ? page.areas.concat(["其他"]) : page.areas
        current: page.library.curatedArea
        onPicked: (option) => page.library.selectCuratedArea(option)
    }
    ChipRow {
        visible: page.sectionKey === "artists"
        options: ["全部", "男歌手", "女歌手", "乐队组合"]
        current: page.library.artistType
        onPicked: (option) => page.library.selectArtistType(option)
    }
    Flow {
        Layout.fillWidth: true
        visible: page.sectionKey === "artists"
        spacing: 2
        Repeater {
            model: ["热门"].concat("ABCDEFGHIJKLMNOPQRSTUVWXYZ#".split(""))
            delegate: AbstractButton {
                id: letter
                required property string modelData
                implicitWidth: Math.max(24, caption.implicitWidth + 10)
                implicitHeight: 24
                hoverEnabled: true
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                focusPolicy: Qt.TabFocus
                Accessible.name: modelData
                onClicked: page.library.selectArtistInitial(modelData)
                background: Rectangle {
                    radius: 5
                    color: Theme.interactive("transparent", letter.hovered, letter.down)
                    border.width: letter.visualFocus ? 1 : 0
                    border.color: Theme.accent
                }
                contentItem: Label {
                    id: caption
                    text: letter.modelData
                    color: page.library.artistInitial === letter.modelData ? Theme.accent : Theme.secondary
                    font.pixelSize: 12
                    font.weight: page.library.artistInitial === letter.modelData ? Font.Bold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
    ChipRow {
        visible: page.vipPage || ["newTracks", "albums"].includes(page.sectionKey)
        readonly property var keys: page.vipPage
            ? ({ "会员歌单": "vip", "会员榜单": "vipCharts", "推荐歌曲": "vipSongs" })
            : ({ "新歌": "newTracks", "新碟": "albums" })
        options: Object.keys(keys)
        current: options.find(name => keys[name] === page.sectionKey) || ""
        onPicked: (option) => page.library.browse(keys[option])
    }

    Flickable {
        id: overview
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: page.shelved && page.library.sectionShelves.length > 0
        clip: true
        contentWidth: width
        contentHeight: shelves.implicitHeight + 24
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        HomeShelves {
            id: shelves
            width: overview.width - 14
            shelves: page.shelved ? page.library.sectionShelves : []
            currentId: page.library.currentTrack.id || ""
            onCollectionChosen: (entry) => page.playlistChosen(entry)
            onCollectionPlayed: (entry) => page.playlistPlayed(entry)
            onSectionChosen: (section) => page.library.browse(section)
            onTracksChosen: (tracks, index) => {
                if (index < 0)
                    page.library.playAll(tracks)
                else
                    page.library.playCollection(tracks, index)
            }
        }
    }

    StatusView {
        Layout.fillWidth: true
        Layout.topMargin: 32
        visible: page.library.sectionBusy || page.library.sectionNeedsLogin
                 || (page.library.sectionItems.length === 0 && page.library.sectionTracks.length === 0
                     && page.library.sectionShelves.length === 0)
        busy: page.library.sectionBusy
        symbol: page.library.sectionNeedsLogin ? "user" : "disc"
        title: busy ? "正在加载…" : page.library.sectionNeedsLogin ? "登录后查看"
                : page.library.sectionError.length ? "暂时无法加载" : "暂无内容"
        detail: page.library.sectionError
        actionText: busy ? "" : page.library.sectionNeedsLogin ? "扫码登录"
                    : page.library.sectionError.length ? "重试" : ""
        onAction: {
            if (page.library.sectionNeedsLogin)
                page.loginRequested()
            else
                page.library.browse(page.library.section)
        }
    }

    TrackList {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: tracks.length > 0
        library: page.library
        tracks: page.library.sectionTracks
        currentId: page.library.currentTrack.id || ""
        playing: page.library.media.playbackState === MediaPlayer.PlayingState
        onLoginRequested: page.loginRequested()
        onArtistChosen: (data) => page.playlistChosen(data)
        onAlbumChosen: (data) => page.playlistChosen(data)
        onActivated: (index) => page.library.playCollection(tracks, index)
    }

    CollectionGrid {
        id: galleryGrid
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.rightMargin: 14
        visible: page.gallery && page.library.sectionItems.length > 0
        lazy: true
        singleRow: false
        look: page.galleryLook
        minimumColumnWidth: look === "round" ? 150 : 168
        entries: page.gallery ? page.library.sectionItems : []
        onChosen: (data) => page.playlistChosen(data)
        onPlayed: (data) => page.playlistPlayed(data)
    }

    ListView {
        id: entries
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: !page.gallery && !page.shelved && count > 0
        clip: true
        reuseItems: true
        spacing: page.sectionKey === "following" ? 10 : 2
        boundsBehavior: Flickable.StopAtBounds
        model: page.gallery || page.shelved ? [] : page.library.sectionItems
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        delegate: AbstractButton {
            id: entry
            required property var modelData
            readonly property bool post: page.sectionKey === "following"
            width: entries.width - 14
            height: post ? 84 : 62
            padding: post ? 14 : 8
            hoverEnabled: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            focusPolicy: Qt.TabFocus
            Accessible.name: modelData.displayTitle || modelData.title || ""
            background: Rectangle {
                radius: 10
                color: entry.post || entry.hovered ? Theme.panel : "transparent"
                border.color: entry.visualFocus ? Theme.accent : entry.hovered && !Theme.dark ? Theme.line : "transparent"
            }
            onClicked: {
                if (modelData.kind === "person") {
                    Qt.openUrlExternally("https://music.163.com/#/user/home?id=" + encodeURIComponent(modelData.id))
                } else if (modelData.kind === "podcastCategory") {
                    page.library.browsePodcastCategory(modelData)
                } else if (modelData.kind === "sceneCategory") {
                    page.library.browseSceneCategory(modelData)
                } else if (modelData.kind === "track") {
                    page.library.playAll([modelData])
                } else if (modelData.kind === "video") {
                    const path = /^[0-9]+$/.test(modelData.id) ? "mv" : "video"
                    Qt.openUrlExternally("https://music.163.com/#/" + path + "?id=" + encodeURIComponent(modelData.id))
                } else if (modelData.kind === "post") {
                    page.postChosen(modelData.displayTitle, modelData.subtitle)
                } else {
                    page.playlistChosen(modelData)
                }
            }
            contentItem: RowLayout {
                spacing: 12
                Cover {
                    Layout.preferredWidth: entry.post ? 52 : 46
                    Layout.preferredHeight: Layout.preferredWidth
                    edge: 6
                    round: entry.modelData.kind === "person"
                    visible: entry.modelData.kind !== "sceneCategory" && entry.modelData.kind !== "podcastCategory"
                             || String(entry.modelData.art || "").length > 0
                    source: entry.modelData.art || ""
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label {
                        Layout.fillWidth: true
                        text: entry.modelData.displayTitle || entry.modelData.title || ""
                        textFormat: Text.PlainText
                        color: entry.post ? (Theme.dark ? "#8fa8d8" : "#507daf") : Theme.text
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: entry.modelData.subtitle || entry.modelData.description || (entry.modelData.trackCount
                              ? entry.modelData.trackCount + (entry.modelData.kind === "radio" ? " 期节目" : " 首歌曲") : "")
                        visible: text.length > 0
                        color: entry.post ? Theme.text : Theme.muted
                        font.pixelSize: entry.post ? 13 : 12
                        elide: Text.ElideRight
                    }
                }
                Symbol {
                    Layout.preferredWidth: 15
                    Layout.preferredHeight: 15
                    name: entry.modelData.kind === "track" ? "play" : "chevron"
                    tint: Theme.muted
                    visible: entry.hovered
                }
            }
        }
    }
    Item {
        Layout.fillHeight: true
        visible: page.library.sectionItems.length === 0 && page.library.sectionTracks.length === 0
                     && page.library.sectionShelves.length === 0
    }
}
