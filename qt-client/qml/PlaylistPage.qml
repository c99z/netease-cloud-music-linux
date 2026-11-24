pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

// Playlist, chart, album, artist, podcast and program detail.
Item {
    id: page
    required property Library library
    signal loginRequested()
    signal collectionChosen(var data)

    readonly property var info: library.playlist
    readonly property string kind: info.kind || "playlist"
    readonly property bool listLike: kind === "playlist" || kind === "chart"
    readonly property bool artist: kind === "artist"
    readonly property bool episodes: kind === "radio" || kind === "voiceList"
    readonly property string tab: library.playlistTab
    property string filter
    readonly property TrackModel songsModel: library.playlistModel
    onFilterChanged: songsModel.filter = filter
    readonly property var tabs: listLike ? [
        { key: "tracks", label: "歌曲", count: info.trackCount || library.playlistTracks.length || "" },
        { key: "comments", label: "评论", count: info.commentCount ? Theme.count(info.commentCount) : "" },
        { key: "subscribers", label: "收藏者", count: info.subscribedCount ? Theme.count(info.subscribedCount) : "" }
    ] : artist ? [
        { key: "tracks", label: "歌曲" },
        { key: "albums", label: "专辑", count: info.albumCount || "" },
        { key: "intro", label: "歌手详情" },
        { key: "similar", label: "相似歌手" }
    ] : [
        { key: "tracks", label: episodes ? "节目" : "歌曲", count: library.playlistTracks.length || "" }
    ]

    property string shownId
    Connections {
        target: page.library
        function onPlaylistChanged() {
            if (page.library.playlist.id !== page.shownId) {
                page.shownId = page.library.playlist.id || ""
                page.filter = ""
                songs.pinnedTop = true
                songs.positionViewAtBeginning()
                other.contentY = 0
            }
        }
    }

    ColumnLayout {
        id: head
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 26
            Item {
                Layout.preferredWidth: 180
                Layout.preferredHeight: 180
                Cover {
                    anchors.fill: parent
                    edge: 10
                    round: page.artist
                    source: page.info.art || ""
                }
                Row {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 9
                    spacing: 3
                    visible: !page.artist && Number(page.info.playCount) > 0
                    Symbol {
                        width: 12
                        height: 12
                        anchors.verticalCenter: parent.verticalCenter
                        name: "headphones"
                        tint: "white"
                    }
                    Label {
                        text: Theme.count(page.info.playCount)
                        color: "white"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        style: Text.Raised
                        styleColor: "#66000000"
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    text: page.info.title || ""
                    textFormat: Text.PlainText
                    color: Theme.text
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: String(page.info.intro || (page.listLike ? "" : page.info.description) || "").replace(/\s+/g, " ")
                    textFormat: Text.PlainText
                    color: Theme.secondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    ToolTip.visible: introHover.hovered && truncated
                    ToolTip.text: String(page.info.intro || "").slice(0, 600)
                    ToolTip.delay: 500
                    HoverHandler { id: introHover }
                }
                RowLayout {
                    spacing: 8
                    visible: !!page.info.creatorName || !!page.info.createTime
                    Cover {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        round: true
                        visible: !!page.info.creatorAvatar && String(page.info.creatorAvatar).length > 0
                        source: page.info.creatorAvatar || ""
                    }
                    Label {
                        text: page.info.creatorName || ""
                        textFormat: Text.PlainText
                        color: Theme.secondary
                        font.pixelSize: 12
                    }
                    Label {
                        visible: !!page.info.createTime
                        text: (page.info.createTime || "") + (page.kind === "album" ? "发行" : "创建")
                        color: Theme.muted
                        font.pixelSize: 12
                    }
                }
                Item { Layout.fillHeight: true; Layout.minimumHeight: 6 }
                RowLayout {
                    spacing: 10
                    PillButton {
                        primary: true
                        symbol: "play"
                        text: "播放全部"
                        enabled: page.library.playlistTracks.length > 0
                        onClicked: page.library.playAll(page.library.playlistTracks)
                    }
                    PillButton {
                        visible: page.kind === "playlist" && page.info.detailed === true
                                 && page.info.owned !== true && page.info.specialType !== 5
                        symbol: page.info.subscribed ? "collected" : "collect"
                        filledSymbol: false
                        text: page.info.subscribed ? "已收藏" + (page.info.subscribedCount ? " " + Theme.count(page.info.subscribedCount) : "")
                                                    : page.info.subscribedCount ? Theme.count(page.info.subscribedCount) : "收藏"
                        onClicked: {
                            if (page.library.loggedIn)
                                page.library.togglePlaylistSubscribe()
                            else
                                page.loginRequested()
                        }
                    }
                    PillButton {
                        symbol: "link"
                        filledSymbol: false
                        text: "复制链接"
                        visible: ["playlist", "chart", "album", "artist"].includes(page.kind)
                        onClicked: {
                            const path = page.kind === "chart" ? "playlist" : page.kind
                            clipboard.text = "https://music.163.com/" + path + "?id=" + page.info.id
                            clipboard.selectAll()
                            clipboard.copy()
                            page.library.notification("链接已复制。")
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 22
            Layout.bottomMargin: 6
            TabStrip {
                Layout.fillWidth: true
                tabs: page.tabs
                current: page.tab
                onChosen: (key) => page.library.loadPlaylistTab(key)
            }
            TextField {
                id: filterField
                visible: page.tab === "tracks" && page.library.playlistTracks.length > 0
                Layout.preferredWidth: activeFocus || text.length ? 190 : 84
                Layout.preferredHeight: 30
                Layout.rightMargin: 14
                leftPadding: 30
                placeholderText: "搜索"
                placeholderTextColor: Theme.muted
                color: Theme.text
                font.pixelSize: 12
                selectByMouse: true
                text: page.filter
                onTextEdited: page.filter = text
                Accessible.name: "在歌单内搜索"
                Behavior on Layout.preferredWidth { NumberAnimation { duration: 120 } }
                background: Rectangle {
                    radius: 15
                    color: Theme.field
                    border.color: filterField.activeFocus ? Theme.accent : Theme.line
                }
                Symbol {
                    x: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 13
                    height: 13
                    name: "search"
                    tint: Theme.muted
                }
            }
        }
    }
    TextEdit { id: clipboard; visible: false }

    TrackList {
        id: songs
        anchors.fill: parent
        visible: page.tab === "tracks"
        library: page.library
        topContent: page.tab === "tracks" ? head : null
        tracks: page.songsModel
        currentId: page.library.currentTrack.id || ""
        playing: page.library.media.playbackState === MediaPlayer.PlayingState
        removable: page.kind === "playlist" && page.info.owned === true
        onActivated: (index) => page.library.playCollection(page.songsModel.visibleTracks(), index)
        onLoginRequested: page.loginRequested()
        onArtistChosen: (data) => page.collectionChosen(data)
        onAlbumChosen: (data) => page.collectionChosen(data)
        onNearEnd: {
            if (page.library.playlistHasMore && !page.library.playlistBusy)
                page.library.loadMorePlaylist()
        }
        footer: Item {
            width: songs.width
            height: status.visible ? status.implicitHeight : more.visible ? 44 : 12
            StatusView {
                id: status
                width: parent.width
                visible: page.songsModel.count === 0
                busy: page.library.playlistBusy
                symbol: "music"
                title: busy ? "正在加载…" : page.library.playlistError.length ? "暂时无法加载"
                     : page.filter.length ? "没有匹配的歌曲" : "暂无内容"
                detail: page.library.playlistError
                actionText: page.library.playlistError.length ? "重试" : ""
                onAction: page.library.openPlaylist(page.library.playlist, false)
            }
            Label {
                id: more
                anchors.centerIn: parent
                visible: !status.visible && page.library.playlistHasMore
                text: page.library.playlistBusy ? "正在加载更多…" : "继续滚动加载更多"
                color: Theme.muted
                font.pixelSize: 12
            }
        }
    }

    Flickable {
        id: other
        anchors.fill: parent
        visible: page.tab !== "tracks"
        clip: true
        contentWidth: width
        contentHeight: column.implicitHeight + 20
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column
            width: other.width - 14
            spacing: 10
            Item {
                id: slot
                width: parent.width
                height: head.parent === slot ? head.implicitHeight : 0
                readonly property bool wanted: page.tab !== "tracks"
                onWantedChanged: adopt()
                Component.onCompleted: adopt()
                function adopt() {
                    if (wanted) {
                        head.parent = slot
                        head.width = Qt.binding(() => slot.width)
                    }
                }
            }
            StatusView {
                width: parent.width
                visible: page.library.playlistTabBusy
                         || (page.library.playlistTabItems.length === 0 && page.library.playlistTabText.length === 0)
                busy: page.library.playlistTabBusy
                symbol: "comment"
                title: busy ? "正在加载…" : "暂无内容"
            }
            Label {
                width: Math.min(parent.width, 860)
                visible: text.length > 0
                text: page.library.playlistTabText
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.secondary
                font.pixelSize: 13
                lineHeight: 1.5
            }
            CollectionGrid {
                width: parent.width
                visible: page.tab === "albums" || page.tab === "similar"
                singleRow: false
                look: page.tab === "similar" ? "round" : "plain"
                entries: visible ? page.library.playlistTabItems : []
                onChosen: (data) => page.collectionChosen(data)
                onPlayed: (data) => page.collectionChosen(data)
            }
            Repeater {
                model: page.tab === "comments" || page.tab === "subscribers" ? page.library.playlistTabItems : []
                delegate: Item {
                    id: entry
                    required property var modelData
                    readonly property bool heading: modelData.kind === "heading"
                    width: column.width
                    height: heading ? 34 : body.implicitHeight + 18
                    Label {
                        visible: entry.heading
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.modelData.title || ""
                        color: Theme.text
                        font.pixelSize: 15
                        font.weight: Font.Medium
                    }
                    RowLayout {
                        id: body
                        visible: !entry.heading
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 12
                        Cover {
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            Layout.alignment: Qt.AlignTop
                            round: true
                            source: entry.modelData.art || ""
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                Layout.fillWidth: true
                                text: entry.modelData.title || ""
                                textFormat: Text.PlainText
                                color: Theme.dark ? "#8fa8d8" : "#507daf"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: entry.modelData.text || entry.modelData.description || "这个人很懒, 什么也没留下"
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                                color: Theme.text
                                font.pixelSize: 13
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: entry.modelData.reply || ""
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                                color: Theme.secondary
                                font.pixelSize: 12
                                padding: 8
                                background: Rectangle { radius: 6; color: Theme.hover }
                            }
                            RowLayout {
                                visible: entry.modelData.kind === "comment"
                                spacing: 14
                                Label {
                                    text: entry.modelData.time || ""
                                    color: Theme.muted
                                    font.pixelSize: 11
                                }
                                Label {
                                    visible: Number(entry.modelData.likes) > 0
                                    text: "赞 " + Theme.count(entry.modelData.likes)
                                    color: Theme.muted
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                    Rectangle {
                        visible: !entry.heading
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.line
                        opacity: .6
                    }
                }
            }
        }
    }
}
