pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The song table used by playlists, search, the queue and personal pages.
ListView {
    id: list
    // Either a plain array of tracks or a TrackModel (rows are then created on demand and
    // updated in place).
    required property var tracks
    readonly property bool usesModel: tracks !== null && typeof tracks === "object" && !Array.isArray(tracks)
                                      && tracks.visibleTracks !== undefined
    property Library library
    property string currentId
    property bool playing: false
    property bool removable: false        // rows belong to a playlist the user owns
    property bool queueMode: false        // rows are the play queue
    property bool showHeader: true
    property Item topContent: null        // page header that scrolls with the rows
    signal activated(int index)
    signal loginRequested()
    signal artistChosen(var artist)
    signal albumChosen(var album)
    signal nearEnd()

    readonly property int likesRevision: library ? library.likesRevision : 0
    readonly property bool wide: width > 640
    model: tracks
    // Rows are mouse targets; keep keyboard focus where the user put it (e.g. the
    // playlist search field in the header) when rows are added or filtered.
    focus: false
    activeFocusOnTab: false
    keyNavigationEnabled: false
    currentIndex: -1
    highlightFollowsCurrentItem: false
    clip: true
    reuseItems: true
    boundsBehavior: Flickable.StopAtBounds
    spacing: 0
    cacheBuffer: 600
    ScrollBar.vertical: ScrollBar { policy: list.interactive ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }
    // Stay glued to the top while the header above the rows is still growing.
    property bool pinnedTop: true
    property int lastCount: 0
    function pinTop() {
        if (pinnedTop)
            contentY = originY
    }
    onMovementEnded: pinnedTop = contentY <= originY + 2
    onOriginYChanged: pinTop()
    onContentHeightChanged: Qt.callLater(pinTop)
    onCountChanged: {
        if (lastCount === 0 && count > 0) {
            pinnedTop = true
            Qt.callLater(pinTop)
        }
        lastCount = count
    }
    onAtYEndChanged: {
        if (atYEnd && count > 0)
            list.nearEnd()
    }

    function liked(id: string): bool {
        return likesRevision >= 0 && !!library && library.isLiked(id)
    }
    function requestLike(id: string) {
        if (!library)
            return
        if (library.loggedIn)
            library.toggleLikeTrack(id)
        else
            list.loginRequested()
    }
    function openMenu(track: var, index: int, item: Item, x: real, y: real) {
        menu.track = track
        menu.index = index
        menu.popup(item, x, y)
    }

    component MenuRow: MenuItem {
        id: entry
        implicitHeight: 32
        implicitWidth: 190
        contentItem: Label {
            leftPadding: 6
            text: entry.text
            color: entry.enabled ? Theme.text : Theme.muted
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 5
            color: Theme.interactive("transparent", entry.highlighted, entry.down)
        }
        arrow: Symbol {
            x: entry.width - width - 8
            y: (entry.height - height) / 2
            width: 11
            height: 11
            visible: !!entry.subMenu
            name: "chevron"
            tint: Theme.muted
        }
    }

    Menu {
        id: menu
        property var track: ({})
        property int index: -1
        padding: 5
        delegate: MenuRow {}
        background: Rectangle {
            implicitWidth: 200
            radius: 8
            color: Theme.panel
            border.color: Theme.line
        }
        MenuRow { text: "播放"; onTriggered: list.activated(menu.index) }
        MenuRow {
            text: "下一首播放"
            visible: !!list.library
            height: visible ? implicitHeight : 0
            onTriggered: list.library.playNext(menu.track)
        }
        MenuSeparator {
            contentItem: Rectangle { implicitHeight: 1; color: Theme.line }
        }
        MenuRow {
            text: list.liked(menu.track.id || "") ? "取消喜欢" : "喜欢"
            onTriggered: list.requestLike(menu.track.id)
        }
        Menu {
            id: collectMenu
            title: "收藏到歌单"
            enabled: !!list.library && list.library.loggedIn && list.library.createdPlaylists.length > 0
            padding: 5
            background: Rectangle {
                implicitWidth: 210
                radius: 8
                color: Theme.panel
                border.color: Theme.line
            }
            Instantiator {
                model: list.library ? list.library.createdPlaylists : []
                delegate: MenuRow {
                    required property var modelData
                    text: modelData.title
                    onTriggered: list.library.addToPlaylist(modelData.id, menu.track.id)
                }
                onObjectAdded: (index, object) => collectMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => collectMenu.removeItem(object)
            }
        }
        MenuRow {
            text: "复制链接"
            onTriggered: {
                clipboard.text = "https://music.163.com/song?id=" + menu.track.id
                clipboard.selectAll()
                clipboard.copy()
                if (list.library)
                    list.library.notification("链接已复制。")
            }
        }
        MenuSeparator {
            visible: removeRow.visible
            height: visible ? implicitHeight : 0
            contentItem: Rectangle { implicitHeight: 1; color: Theme.line }
        }
        MenuRow {
            id: removeRow
            text: list.queueMode ? "从播放列表删除" : "从歌单中删除"
            visible: list.removable || list.queueMode
            height: visible ? implicitHeight : 0
            onTriggered: {
                if (list.queueMode)
                    list.library.removeFromQueue(menu.index)
                else
                    list.library.removeFromPlaylist(menu.track.id)
            }
        }
    }
    TextEdit { id: clipboard; visible: false }

    header: Column {
        width: list.width
        Item {
            id: slot
            width: parent.width
            height: list.topContent ? list.topContent.implicitHeight : 0
            onHeightChanged: Qt.callLater(list.pinTop)
            Component.onCompleted: adopt()
            function adopt() {
                if (list.topContent) {
                    list.topContent.parent = slot
                    list.topContent.width = Qt.binding(() => slot.width)
                }
            }
            Connections {
                target: list
                function onTopContentChanged() { slot.adopt() }
            }
        }
        RowLayout {
            width: parent.width - 14
            height: list.showHeader ? 34 : 0
            visible: height > 0 && list.count > 0
            spacing: 10
            Label {
                Layout.preferredWidth: 38
                text: "#"
                color: Theme.muted
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 12
            }
            Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 320
                text: "标题"
                color: Theme.muted
                font.pixelSize: 12
            }
            Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 190
                visible: list.wide
                text: "专辑"
                color: Theme.muted
                font.pixelSize: 12
            }
            Label {
                Layout.preferredWidth: 36
                text: "喜欢"
                color: Theme.muted
                font.pixelSize: 12
            }
            Label {
                Layout.preferredWidth: 52
                text: "时长"
                color: Theme.muted
                font.pixelSize: 12
            }
        }
    }

    function trackAt(index: int): var {
        return usesModel ? tracks.at(index) : tracks[index]
    }

    delegate: Item {
        id: row
        required property int index
        required property var modelData
        // Supplied by TrackModel; plain arrays fall back to the library lookup.
        property var liked: undefined
        readonly property bool isLiked: liked !== undefined ? liked : list.liked(modelData.id)
        readonly property bool isCurrent: modelData.id === list.currentId
        readonly property bool active: hover.hovered
        width: list.width - 14
        height: 56

        Rectangle {
            anchors.fill: parent
            radius: 8
            color: row.active ? Theme.panel : "transparent"
            border.width: row.active && !Theme.dark ? 1 : 0
            border.color: Theme.line
        }
        HoverHandler { id: hover }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: list.activated(row.index)
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: (point) => list.openMenu(row.modelData, row.index, row, point.position.x, point.position.y)
        }

        RowLayout {
            anchors.fill: parent
            spacing: 10
            Item {
                Layout.preferredWidth: 38
                Layout.fillHeight: true
                Label {
                    anchors.centerIn: parent
                    visible: !hover.hovered && !row.isCurrent
                    text: String(row.index + 1).padStart(2, "0")
                    color: Theme.muted
                    font.pixelSize: 12
                }
                IconButton {
                    anchors.centerIn: parent
                    width: 28
                    height: 28
                    iconSize: 15
                    visible: hover.hovered || row.isCurrent
                    symbol: row.isCurrent && list.playing && !hover.hovered ? "music" : "play"
                    tint: row.isCurrent ? Theme.accent : Theme.secondary
                    text: "播放"
                    onClicked: list.activated(row.index)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 320
                spacing: 10
                Cover {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    edge: 5
                    source: row.modelData.art
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                        text: row.modelData.title
                        color: row.isCurrent ? Theme.accent : Theme.text
                        font.pixelSize: 14
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SongTag { label: "SQ"; tone: Theme.gold; visible: row.modelData.sq === true }
                        SongTag { label: "VIP"; visible: row.modelData.vip === true }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                            text: row.modelData.artist
                            color: artistHover.hovered && row.modelData.artistId ? Theme.text : Theme.muted
                            font.pixelSize: 12
                            HoverHandler {
                                id: artistHover
                                cursorShape: row.modelData.artistId ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
                            TapHandler {
                                enabled: !!row.modelData.artistId
                                onTapped: list.artistChosen({ id: row.modelData.artistId, kind: "artist",
                                    title: String(row.modelData.artist).split(" / ")[0], art: "" })
                            }
                        }
                    }
                }
                RowLayout {
                    visible: hover.hovered && list.width > 520
                    spacing: 0
                    IconButton {
                        symbol: "collect"
                        iconSize: 16
                        text: "收藏到歌单"
                        onClicked: {
                            if (list.library && !list.library.loggedIn) {
                                list.loginRequested()
                                return
                            }
                            menu.track = row.modelData
                            menu.index = row.index
                            collectMenu.popup(this, 0, height)
                        }
                    }
                    IconButton {
                        symbol: "more"
                        iconSize: 16
                        text: "更多"
                        onClicked: list.openMenu(row.modelData, row.index, this, 0, height)
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 190
                visible: list.wide
                textFormat: Text.PlainText
                text: row.modelData.album || ""
                color: albumHover.hovered && row.modelData.albumId ? Theme.text : Theme.secondary
                elide: Text.ElideRight
                font.pixelSize: 12
                HoverHandler {
                    id: albumHover
                    cursorShape: row.modelData.albumId ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
                TapHandler {
                    enabled: !!row.modelData.albumId
                    onTapped: list.albumChosen({ id: row.modelData.albumId, kind: "album",
                        title: row.modelData.album, art: row.modelData.art })
                }
            }
            Item {
                Layout.preferredWidth: 36
                Layout.fillHeight: true
                IconButton {
                    readonly property bool on: row.isLiked
                    anchors.verticalCenter: parent.verticalCenter
                    x: -6
                    width: 30
                    height: 30
                    iconSize: 17
                    symbol: "heart"
                    filled: on
                    tint: on ? Theme.accent : Theme.muted
                    text: on ? "取消喜欢" : "喜欢"
                    onClicked: list.requestLike(row.modelData.id)
                }
            }
            Label {
                Layout.preferredWidth: 52
                text: Theme.time(row.modelData.duration)
                color: Theme.muted
                font.pixelSize: 12
            }
        }
    }
}
