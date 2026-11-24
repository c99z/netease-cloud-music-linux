pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

// Full-window player: turntable on the left, lyrics or similar picks on the right.
Rectangle {
    id: page
    required property Library library
    signal closeRequested()
    signal collectionChosen(var data)
    signal loginRequested()

    readonly property var track: library.currentTrack
    readonly property var lines: library.lyrics
    readonly property int activeLine: lyricIndex(library.playbackPosition)
    readonly property bool playing: library.media.playbackState === MediaPlayer.PlayingState && !library.playbackBusy
    property bool followLyrics: true
    property string panel: "lyrics"
    color: Theme.stage

    gradient: Gradient {
        GradientStop { position: 0; color: "#4a3f3b" }
        GradientStop { position: 1; color: "#272120" }
    }

    function lyricIndex(position: real): int {
        if (!lines.length || lines[0].time < 0)
            return -1
        let low = 0
        let high = lines.length
        while (low < high) {
            const mid = Math.floor((low + high) / 2)
            if (lines[mid].time <= position)
                low = mid + 1
            else
                high = mid
        }
        return low - 1
    }

    function centerOnPlayback() {
        if (!visible || !followLyrics || !lyrics.count)
            return
        lyrics.positionViewAtIndex(Math.max(0, activeLine), ListView.Center)
    }

    onActiveLineChanged: Qt.callLater(centerOnPlayback)
    onLinesChanged: followLyrics = true
    onVisibleChanged: {
        if (visible) {
            followLyrics = true
            library.loadCurrentLyrics()
            if (panel === "similar")
                library.loadSimilar()
            Qt.callLater(centerOnPlayback)
        }
    }
    onPanelChanged: {
        if (panel === "similar")
            library.loadSimilar()
    }

    Connections {
        target: page.library
        enabled: page.visible
        function onQueueChanged() {
            page.library.loadCurrentLyrics()
            if (page.panel === "similar")
                page.library.loadSimilar()
        }
    }

    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: (wheel) => wheel.accepted = true }

    IconButton {
        x: 22
        y: 18
        symbol: "down"
        tint: "#d8d0cb"
        onDark: true
        text: "收起"
        onClicked: page.closeRequested()
    }
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 60
        anchors.leftMargin: 70
        anchors.rightMargin: 120
        TapHandler { onDoubleTapped: page.Window.window.visibility === Window.Maximized ? page.Window.window.showNormal() : page.Window.window.showMaximized() }
        DragHandler {
            target: null
            onActiveChanged: if (active) page.Window.window.startSystemMove()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: 70
        anchors.bottomMargin: 80 + 20
        anchors.leftMargin: Math.max(30, parent.width * .07)
        anchors.rightMargin: Math.max(30, parent.width * .07)
        spacing: Math.max(30, parent.width * .05)

        Item {
            id: deck
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(parent.width * .42, height)
            readonly property real disc: Math.min(width, height) * .74

            Rectangle {
                id: record
                width: deck.disc
                height: width
                radius: width / 2
                anchors.centerIn: parent
                anchors.verticalCenterOffset: deck.disc * .06
                color: "#3a3230"
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: parent.width * .035
                    radius: width / 2
                    color: "#121212"
                    border.color: "#2a2a2a"
                    border.width: 1
                    Repeater {
                        model: 6
                        delegate: Rectangle {
                            required property int index
                            anchors.centerIn: parent
                            width: parent.width * (0.92 - index * .045)
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.color: "#1f1f1f"
                        }
                    }
                    Cover {
                        id: label
                        anchors.centerIn: parent
                        width: parent.width * .64
                        height: width
                        round: true
                        source: page.track.art || ""
                        RotationAnimator on rotation {
                            from: 0
                            to: 360
                            duration: 26000
                            loops: Animation.Infinite
                            running: page.playing && page.visible
                        }
                    }
                }
            }
            // Tone arm: pivots above the record and swings onto it while playing.
            Item {
                id: arm
                readonly property real length: record.width * .5
                x: record.x + record.width * .74
                y: record.y - record.width * .12
                width: 1
                height: 1
                rotation: page.playing ? 14 : -12
                Behavior on rotation { NumberAnimation { duration: 450; easing.type: Easing.OutCubic } }
                Rectangle {
                    x: -2.5
                    width: 5
                    height: arm.length
                    radius: 2.5
                    color: "#ededed"
                }
                Rectangle {
                    x: -record.width * .028
                    y: arm.length - 4
                    width: record.width * .056
                    height: record.width * .09
                    radius: 3
                    color: "#f5f5f5"
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 4
                        width: parent.width * .45
                        height: 3
                        radius: 1.5
                        color: "#bdbdbd"
                    }
                }
                Rectangle {
                    x: -width / 2
                    y: -width / 2
                    width: record.width * .085
                    height: width
                    radius: width / 2
                    color: "#f2f2f2"
                    border.color: "#d0d0d0"
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * .38
                        height: width
                        radius: width / 2
                        color: "#c4c4c4"
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: 640
            spacing: 10

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 10
                text: page.track.title || Qt.application.displayName
                textFormat: Text.PlainText
                color: "#f3ece8"
                font.pixelSize: 24
                font.weight: Font.Medium
                elide: Text.ElideRight
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 18
                Label {
                    Layout.maximumWidth: implicitWidth
                    Layout.fillWidth: true
                    visible: !!page.track.album
                    text: "专辑：" + (page.track.album || "")
                    textFormat: Text.PlainText
                    color: albumHover.hovered && page.track.albumId ? "#f3ece8" : "#a89f9a"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    HoverHandler { id: albumHover; cursorShape: page.track.albumId ? Qt.PointingHandCursor : Qt.ArrowCursor }
                    TapHandler {
                        enabled: !!page.track.albumId
                        onTapped: {
                            page.collectionChosen({ id: page.track.albumId, kind: "album", title: page.track.album, art: page.track.art })
                            page.closeRequested()
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: page.track.artist ? "歌手：" + page.track.artist : ""
                    textFormat: Text.PlainText
                    color: artistHover.hovered && page.track.artistId ? "#f3ece8" : "#a89f9a"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    HoverHandler { id: artistHover; cursorShape: page.track.artistId ? Qt.PointingHandCursor : Qt.ArrowCursor }
                    TapHandler {
                        enabled: !!page.track.artistId
                        onTapped: {
                            page.collectionChosen({ id: page.track.artistId, kind: "artist",
                                                    title: String(page.track.artist).split(" / ")[0], art: "" })
                            page.closeRequested()
                        }
                    }
                }
            }
            Rectangle {
                Layout.topMargin: 4
                implicitWidth: switcher.implicitWidth + 6
                implicitHeight: 28
                radius: 14
                color: "#33ffffff"
                RowLayout {
                    id: switcher
                    anchors.centerIn: parent
                    spacing: 0
                    Repeater {
                        model: [{ key: "lyrics", label: "歌词" }, { key: "similar", label: "相似推荐" }]
                        delegate: AbstractButton {
                            id: choice
                            required property var modelData
                            implicitHeight: 22
                            implicitWidth: caption.implicitWidth + 20
                            hoverEnabled: true
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            onClicked: page.panel = modelData.key
                            background: Rectangle {
                                radius: 11
                                color: page.panel === choice.modelData.key ? "#55ffffff"
                                     : Theme.interactive("transparent", choice.hovered, choice.down, true)
                            }
                            contentItem: Label {
                                id: caption
                                text: choice.modelData.label
                                color: "#f3ece8"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: 10

                ListView {
                    id: lyrics
                    anchors.fill: parent
                    visible: page.panel === "lyrics" && count > 0
                    model: page.lines
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: 4
                    onCountChanged: Qt.callLater(page.centerOnPlayback)
                    onMovementStarted: page.followLyrics = false
                    onMovementEnded: resume.restart()
                    Timer { id: resume; interval: 3000; onTriggered: { page.followLyrics = true; page.centerOnPlayback() } }
                    header: Item { height: Math.max(0, lyrics.height / 2 - 24) }
                    footer: Item { height: Math.max(0, lyrics.height / 2 - 24) }
                    delegate: AbstractButton {
                        id: lyric
                        required property int index
                        required property var modelData
                        readonly property bool activeLine: index === page.activeLine
                        readonly property bool canJump: modelData.time >= 0
                            && page.library.canSeekPlayback && !page.library.playbackBusy
                        width: lyrics.width
                        padding: 6
                        hoverEnabled: true
                        HoverHandler { cursorShape: lyric.canJump ? Qt.PointingHandCursor : Qt.ArrowCursor }
                        Accessible.name: modelData.text
                        onClicked: {
                            if (canJump) {
                                page.followLyrics = true
                                page.library.seekPlayback(modelData.time)
                            }
                        }
                        background: Rectangle {
                            radius: 6
                            color: Theme.interactive("transparent", lyric.hovered && lyric.canJump, lyric.down && lyric.canJump, true)
                        }
                        contentItem: ColumnLayout {
                            spacing: 3
                            Label {
                                Layout.fillWidth: true
                                text: lyric.modelData.text
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                                color: lyric.activeLine ? "#ffffff" : "#9a908b"
                                font.pixelSize: lyric.activeLine ? 19 : 15
                                font.weight: lyric.activeLine ? Font.Medium : Font.Normal
                                Behavior on font.pixelSize { NumberAnimation { duration: 150 } }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: lyric.modelData.translation
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                                color: lyric.activeLine ? "#e8dfda" : "#7f7672"
                                font.pixelSize: 13
                            }
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: page.panel === "lyrics" && lyrics.count === 0
                    text: page.library.lyricsBusy ? "正在加载歌词…" : page.library.lyricsError.length ? "歌词加载失败" : "纯音乐，请欣赏"
                    color: "#a89f9a"
                    font.pixelSize: 14
                }

                Flickable {
                    anchors.fill: parent
                    visible: page.panel === "similar"
                    clip: true
                    contentHeight: similar.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ColumnLayout {
                        id: similar
                        width: parent.width
                        spacing: 14
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: lists.implicitHeight + 28
                            visible: page.library.similarPlaylists.length > 0
                            radius: 10
                            color: "#22ffffff"
                            ColumnLayout {
                                id: lists
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 10
                                Label { text: "相关歌单"; color: "#f3ece8"; font.pixelSize: 15; font.weight: Font.Medium }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    Repeater {
                                        model: page.library.similarPlaylists.slice(0, 3)
                                        delegate: AbstractButton {
                                            id: list
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 1
                                            Layout.preferredHeight: width + 34
                                            hoverEnabled: true
                                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                                            onClicked: {
                                                page.collectionChosen(modelData)
                                                page.closeRequested()
                                            }
                                            background: Item {}
                                            contentItem: ColumnLayout {
                                                spacing: 6
                                                Cover {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: width
                                                    edge: 8
                                                    source: list.modelData.art
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: list.modelData.title
                                                    textFormat: Text.PlainText
                                                    color: list.hovered ? "white" : "#d8d0cb"
                                                    font.pixelSize: 12
                                                    elide: Text.ElideRight
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: songsBox.implicitHeight + 28
                            visible: page.library.similarSongs.length > 0
                            radius: 10
                            color: "#22ffffff"
                            ColumnLayout {
                                id: songsBox
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 4
                                RowLayout {
                                    spacing: 8
                                    Label { text: "相似歌曲"; color: "#f3ece8"; font.pixelSize: 15; font.weight: Font.Medium }
                                    PillButton {
                                        implicitHeight: 22
                                        leftPadding: 8
                                        symbol: "play"
                                        text: "播放"
                                        onClicked: page.library.playAll(page.library.similarSongs)
                                    }
                                }
                                Repeater {
                                    model: page.library.similarSongs
                                    delegate: AbstractButton {
                                        id: song
                                        required property int index
                                        required property var modelData
                                        Layout.fillWidth: true
                                        implicitHeight: 48
                                        hoverEnabled: true
                                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                                        onClicked: page.library.playCollection(page.library.similarSongs, index)
                                        background: Rectangle { radius: 6; color: Theme.interactive("transparent", song.hovered, song.down, true) }
                                        contentItem: RowLayout {
                                            spacing: 10
                                            Cover {
                                                Layout.preferredWidth: 36
                                                Layout.preferredHeight: 36
                                                Layout.leftMargin: 4
                                                edge: 4
                                                source: song.modelData.art
                                            }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 2
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: song.modelData.title
                                                    textFormat: Text.PlainText
                                                    color: "#f3ece8"
                                                    font.pixelSize: 13
                                                    elide: Text.ElideRight
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: song.modelData.artist
                                                    textFormat: Text.PlainText
                                                    color: "#a89f9a"
                                                    font.pixelSize: 11
                                                    elide: Text.ElideRight
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        Label {
                            visible: page.library.similarSongs.length === 0 && page.library.similarPlaylists.length === 0
                            text: "暂无相似推荐"
                            color: "#a89f9a"
                            font.pixelSize: 14
                        }
                    }
                }
            }
        }
    }
}
