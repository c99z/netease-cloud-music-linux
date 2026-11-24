pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 推荐: banners, recently played, daily picks, playlists, charts and song shelves.
Flickable {
    id: page
    required property Library library
    signal choosePlaylist(var playlist, bool playNow)
    signal navigate(string key)
    signal roamRequested()

    readonly property var picks: [
        { kind: "daily", title: library.loggedIn ? "每日推荐｜为你更新的歌曲" : "每日推荐｜登录后查看",
          heading: "每日推荐", glyph: "history", badge: Qt.formatDate(new Date(), "dd"), tone: "#d9a400" },
        { kind: "roam", title: "私人漫游｜开启无限漫游", heading: "私人漫游", glyph: "disc", tone: "#8a5a44" }
    ].concat(library.radarPlaylists.slice(0, 6))
    readonly property string greeting: {
        const hour = new Date().getHours()
        return hour < 6 ? "夜深了" : hour < 11 ? "上午好" : hour < 13 ? "中午好" : hour < 18 ? "下午好" : "晚上好"
    }

    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight + 30
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    function pick(entry: var, playNow: bool) {
        if (entry.kind === "daily")
            page.navigate("daily")
        else if (entry.kind === "roam")
            page.roamRequested()
        else
            page.choosePlaylist(entry, playNow)
    }

    ColumnLayout {
        id: column
        x: 40
        width: Math.max(0, page.width - 80)
        spacing: 26

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 190
            Layout.topMargin: 4
            spacing: 18
            visible: page.library.banners.length > 0 || page.library.recentListen.length > 0

            Item {
                id: carousel
                Layout.fillHeight: true
                Layout.preferredWidth: 487
                Layout.maximumWidth: page.library.recentListen.length ? column.width * .46 : 487
                Layout.fillWidth: true
                visible: page.library.banners.length > 0
                property int index: 0
                readonly property var item: page.library.banners.length
                    ? page.library.banners[index % page.library.banners.length] : ({})
                Timer {
                    interval: 5000
                    repeat: true
                    running: page.visible && page.library.banners.length > 1 && !bannerHover.hovered
                    onTriggered: carousel.index = (carousel.index + 1) % page.library.banners.length
                }
                Cover {
                    anchors.fill: parent
                    edge: 12
                    source: carousel.item.art || ""
                }
                HoverHandler { id: bannerHover; cursorShape: carousel.item.id ? Qt.PointingHandCursor : Qt.ArrowCursor }
                TapHandler {
                    onTapped: {
                        const item = carousel.item
                        if (item.kind === "song")
                            page.library.playSongById(item.id)
                        else if (item.id)
                            page.choosePlaylist({ id: item.id, kind: item.kind, title: "", art: "" }, false)
                    }
                }
                Label {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    text: carousel.item.title || ""
                    visible: text.length > 0
                    color: "white"
                    font.pixelSize: 11
                    leftPadding: 8
                    rightPadding: 8
                    topPadding: 3
                    bottomPadding: 3
                    background: Rectangle { color: Theme.accent; radius: 6 }
                }
                Row {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 10
                    spacing: 5
                    Repeater {
                        model: page.library.banners.length
                        delegate: Rectangle {
                            required property int index
                            width: 6
                            height: 6
                            radius: 3
                            color: index === carousel.index || dotHover.hovered ? "white" : "#80ffffff"
                            HoverHandler { id: dotHover; margin: 4; cursorShape: Qt.PointingHandCursor }
                            TapHandler { margin: 4; onTapped: carousel.index = parent.index }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 520
                visible: page.library.recentListen.length > 0
                radius: 12
                color: Theme.panel
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    RowLayout {
                        spacing: 6
                        Label {
                            text: page.greeting + "，"
                            color: Theme.text
                            font.pixelSize: 19
                            font.weight: Font.Medium
                        }
                        Label {
                            Layout.alignment: Qt.AlignBottom
                            Layout.bottomMargin: 2
                            text: "猜你最近喜欢听"
                            color: Theme.text
                            font.pixelSize: 13
                            font.weight: Font.Medium
                        }
                    }
                    RowLayout {
                        id: recentRow
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 12
                        readonly property int shown: Math.max(1, Math.min(page.library.recentListen.length,
                                                                          Math.floor((width + 12) / 118)))
                        Repeater {
                            model: page.library.recentListen.slice(0, recentRow.shown)
                            delegate: AbstractButton {
                                id: recent
                                required property var modelData
                                Layout.fillHeight: true
                                Layout.preferredWidth: height
                                hoverEnabled: true
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                focusPolicy: Qt.TabFocus
                                Accessible.name: modelData.title
                                onClicked: page.choosePlaylist(modelData, false)
                                background: Item {}
                                contentItem: Item {
                                    Cover {
                                        anchors.fill: parent
                                        edge: 8
                                        source: recent.modelData.art
                                    }
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 8
                                        gradient: Gradient {
                                            GradientStop { position: 0; color: "#8a000000" }
                                            GradientStop { position: .5; color: "#00000000" }
                                        }
                                    }
                                    Label {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 7
                                        text: recent.modelData.title
                                        textFormat: Text.PlainText
                                        color: "white"
                                        font.pixelSize: 11
                                        wrapMode: Text.Wrap
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        anchors.left: parent.left
                                        anchors.bottom: parent.bottom
                                        anchors.margins: 7
                                        text: recent.modelData.tag || ""
                                        visible: text.length > 0
                                        color: "white"
                                        font.pixelSize: 9
                                        leftPadding: 4
                                        rightPadding: 4
                                        background: Rectangle { radius: 3; color: "#66000000" }
                                    }
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 32
                                        height: 32
                                        radius: 16
                                        color: "#f2ffffff"
                                        visible: recent.hovered
                                        Symbol {
                                            anchors.centerIn: parent
                                            anchors.horizontalCenterOffset: 1
                                            width: 16
                                            height: 16
                                            name: "play"
                                            tint: Theme.accent
                                        }
                                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                                        TapHandler { onTapped: page.choosePlaylist(recent.modelData, true) }
                                    }
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            SectionTitle {
                text: "精选推荐"
                IconButton {
                    id: refreshButton
                    symbol: "refresh"
                    text: "刷新发现页"
                    iconSize: 16
                    enabled: !page.library.discoveryBusy
                    onClicked: page.library.refreshDiscovery(true)
                    RotationAnimator {
                        target: refreshButton.contentItem
                        from: 0
                        to: 360
                        duration: 900
                        loops: Animation.Infinite
                        running: page.library.discoveryBusy
                        onStopped: refreshButton.contentItem.rotation = 0
                    }
                }
            }
            CollectionGrid {
                Layout.fillWidth: true
                entries: page.picks
                showPlays: false
                onChosen: (entry) => page.pick(entry, false)
                onPlayed: (entry) => page.pick(entry, true)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: page.library.recommendations.length > 0
            spacing: 12
            SectionTitle {
                text: "推荐歌单"
                linked: true
                onOpened: page.navigate("featured")
            }
            CollectionGrid {
                Layout.fillWidth: true
                entries: page.library.recommendations
                onChosen: (entry) => page.choosePlaylist(entry, false)
                onPlayed: (entry) => page.choosePlaylist(entry, true)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: page.library.featuredCharts.length > 0
            spacing: 12
            SectionTitle {
                text: "榜单精选"
                linked: true
                onOpened: page.navigate("charts")
            }
            RankingStrip {
                Layout.fillWidth: true
                charts: page.library.featuredCharts
                onChosen: (entry) => page.choosePlaylist(entry, false)
                onPlayed: (entry) => page.choosePlaylist(entry, true)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: page.library.radarPlaylists.length > 0
            spacing: 12
            SectionTitle { text: (page.library.profile.nickname || "我") + "的雷达歌单" }
            CollectionGrid {
                Layout.fillWidth: true
                entries: page.library.radarPlaylists
                onChosen: (entry) => page.choosePlaylist(entry, false)
                onPlayed: (entry) => page.choosePlaylist(entry, true)
            }
        }

        HomeShelves {
            Layout.fillWidth: true
            visible: shelves.length > 0
            // The radar playlists already have their own section above.
            shelves: page.library.homeShelves.filter(shelf => {
                const radar = page.library.radarPlaylists
                return !(shelf.kind === "playlists" && radar.length && shelf.items.length
                         && shelf.items[0].id === radar[0].id)
            })
            currentId: page.library.currentTrack.id || ""
            onCollectionChosen: (entry) => page.choosePlaylist(entry, false)
            onCollectionPlayed: (entry) => page.choosePlaylist(entry, true)
            onTracksChosen: (tracks, index) => {
                if (index < 0)
                    page.library.playAll(tracks)
                else
                    page.library.playCollection(tracks, index)
            }
        }

        StatusView {
            Layout.fillWidth: true
            visible: page.library.discoveryError.length > 0
                     || (page.library.recommendations.length === 0 && page.library.featuredCharts.length === 0)
            busy: page.library.discoveryBusy
            title: busy ? "正在加载…" : page.library.discoveryError.length ? "部分内容暂时无法加载" : "暂无推荐"
            detail: page.library.discoveryError
            actionText: busy ? "" : "重试"
            onAction: page.library.refreshDiscovery()
        }
    }
}
