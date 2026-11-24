pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: bar
    required property Library library
    property bool queueVisible: false
    property bool stage: false            // drawn over the now-playing page
    property bool desktopLyricsShown: false
    property bool desktopLyricsLocked: false
    property string desktopLyricsStyle: "vinyl"
    signal desktopLyricsRequested()
    signal desktopLyricsStyleSelected(string styleKey)
    signal queueRequested()
    signal nowPlayingRequested()
    signal loginRequested()
    signal collectionChosen(var data)
    readonly property bool playing: library.media.playbackState === MediaPlayer.PlayingState
                                    && !library.playbackBusy && library.playbackError.length === 0
    readonly property bool hasTrack: library.queueIndex >= 0
    readonly property real duration: library.playbackDuration || library.currentTrack.duration || 0
    readonly property color ink: stage ? "#ece6e2" : Theme.text
    readonly property color soft: stage ? "#a89f9a" : Theme.secondary
    readonly property bool liked: library.likesRevision >= 0 && library.currentLiked
    readonly property var modeIcons: ["loop", "loopOne", "shuffle", "sequence"]
    readonly property var modeNames: ["列表循环", "单曲循环", "随机播放", "顺序播放"]

    color: stage ? "#2a2321" : Theme.bar
    implicitHeight: 80
    Rectangle {
        width: parent.width
        height: 1
        color: bar.stage ? "#3a312e" : Theme.line
    }
    Rectangle {
        visible: bar.stage && bar.duration > 0
        width: parent.width * Math.min(1, bar.library.playbackPosition / Math.max(1, bar.duration))
        height: 2
        color: Theme.accent
        opacity: .75
    }

    component PlayerSlider: Slider {
        id: slider
        property bool segmented: false
        property var bufferedRegions: []
        property real playedFraction: position
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        implicitHeight: 16
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        background: Rectangle {
            id: rail
            x: slider.leftPadding
            y: slider.topPadding + (slider.availableHeight - height) / 2
            width: slider.availableWidth
            height: slider.hovered || slider.pressed ? 4 : 3
            radius: 2
            color: bar.stage ? "#4a403c" : Theme.line
            Rectangle {
                visible: !slider.segmented
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: 2
                color: Theme.accent
            }
            Repeater {
                model: slider.bufferedRegions
                delegate: Rectangle {
                    id: segment
                    required property var modelData
                    readonly property real start: modelData.start
                    x: rail.width * start
                    width: rail.width * (modelData.end - start)
                    height: rail.height
                    radius: Math.min(rail.radius, width / 2)
                    color: bar.stage ? "#6b605b" : Theme.bufferedAudio
                }
            }
            Rectangle {
                visible: slider.segmented
                width: rail.width * Math.max(0, Math.min(1, slider.playedFraction))
                height: rail.height
                radius: rail.radius
                color: Theme.accent
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + (slider.availableHeight - height) / 2
            width: 10
            height: 10
            radius: 5
            color: "white"
            border.color: Theme.accent
            border.width: 2
            visible: slider.enabled && (slider.hovered || slider.pressed || slider.visualFocus)
        }
    }

    component BarButton: IconButton {
        tint: bar.soft
        onDark: bar.stage || Theme.dark
        iconSize: 18
    }

    Menu {
        id: collectMenu
        padding: 5
        background: Rectangle {
            implicitWidth: 210
            radius: 8
            color: Theme.panel
            border.color: Theme.line
        }
        Instantiator {
            model: bar.library.createdPlaylists
            delegate: MenuItem {
                id: entry
                required property var modelData
                text: modelData.title
                implicitHeight: 32
                onTriggered: bar.library.addToPlaylist(modelData.id, bar.library.currentTrack.id)
                contentItem: Label {
                    leftPadding: 6
                    text: entry.text
                    color: Theme.text
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle { radius: 5; color: Theme.interactive("transparent", entry.highlighted, entry.down) }
            }
            onObjectAdded: (index, object) => collectMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => collectMenu.removeItem(object)
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 16

        RowLayout {
            Layout.preferredWidth: 330
            Layout.minimumWidth: 200
            Layout.fillWidth: true
            spacing: 12
            visible: !bar.stage || true
            AbstractButton {
                id: coverButton
                Layout.preferredWidth: 54
                Layout.preferredHeight: 54
                enabled: bar.hasTrack
                hoverEnabled: true
                Accessible.name: bar.stage ? "收起播放页" : "打开播放页"
                onClicked: bar.nowPlayingRequested()
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                visible: !bar.stage
                background: Item {}
                contentItem: Item {
                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: "#1d1d1f"
                    }
                    Cover {
                        anchors.fill: parent
                        anchors.margins: 7
                        round: true
                        source: bar.library.currentTrack.art || ""
                        RotationAnimator on rotation {
                            from: 0
                            to: 360
                            duration: 24000
                            loops: Animation.Infinite
                            running: bar.playing
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: "#66000000"
                        visible: coverButton.hovered
                        Symbol {
                            anchors.centerIn: parent
                            width: 18
                            height: 18
                            name: "up"
                            tint: "white"
                        }
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        Layout.maximumWidth: implicitWidth
                        Layout.fillWidth: true
                        text: bar.library.currentTrack.title || Qt.application.displayName
                        textFormat: Text.PlainText
                        color: bar.ink
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: bar.library.playbackBusy || bar.library.media.mediaStatus === MediaPlayer.StalledMedia
                              ? "  正在缓冲…"
                              : bar.hasTrack ? " - " + (bar.library.currentTrack.artist || "") + (bar.library.preview ? "（试听）" : "")
                              : ""
                        textFormat: Text.PlainText
                        color: Theme.muted
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
                RowLayout {
                    spacing: 6
                    visible: bar.hasTrack
                    BarButton {
                        implicitWidth: 26
                        implicitHeight: 26
                        iconSize: 16
                        symbol: "collect"
                        text: "收藏到歌单"
                        onClicked: {
                            if (!bar.library.loggedIn)
                                bar.loginRequested()
                            else if (bar.library.createdPlaylists.length)
                                collectMenu.popup(this, 0, -collectMenu.implicitHeight - 4)
                        }
                    }
                    BarButton {
                        implicitWidth: 26
                        implicitHeight: 26
                        iconSize: 16
                        symbol: "dislike"
                        text: "不喜欢（漫游）"
                        visible: bar.library.roamPlaying
                        enabled: !bar.library.playbackBusy
                        onClicked: bar.library.dislikeRoamTrack()
                    }
                    BarButton {
                        implicitWidth: 26
                        implicitHeight: 26
                        iconSize: 16
                        symbol: "link"
                        text: "复制歌曲链接"
                        onClicked: {
                            clipboard.text = "https://music.163.com/song?id=" + bar.library.currentTrack.id
                            clipboard.selectAll()
                            clipboard.copy()
                            bar.library.notification("链接已复制。")
                        }
                    }
                    Label {
                        Layout.leftMargin: 4
                        visible: bar.stage
                        text: Theme.time(bar.library.playbackPosition) + " / " + Theme.time(bar.duration)
                        color: Theme.muted
                        font.pixelSize: 11
                    }
                }
            }
        }
        TextEdit { id: clipboard; visible: false }

        ColumnLayout {
            Layout.preferredWidth: 440
            Layout.maximumWidth: 520
            Layout.fillWidth: true
            spacing: 2
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 14
                BarButton {
                    symbol: "heart"
                    filled: bar.liked
                    tint: bar.liked ? Theme.accent : bar.soft
                    text: bar.liked ? "取消喜欢" : "喜欢"
                    enabled: bar.hasTrack
                    onClicked: bar.library.loggedIn ? bar.library.toggleLike() : bar.loginRequested()
                }
                BarButton {
                    symbol: "previous"
                    text: "上一首"
                    tint: bar.ink
                    enabled: bar.library.hasPrevious || bar.library.canSeekPlayback
                    onClicked: bar.library.previous()
                }
                IconButton {
                    symbol: bar.playing ? "pause" : "play"
                    text: bar.playing ? "暂停" : bar.library.playbackError.length ? "重试" : "播放"
                    featured: true
                    iconSize: 18
                    enabled: !bar.library.playbackBusy && (bar.hasTrack || bar.library.freshTracks.length > 0)
                    onClicked: bar.library.togglePlayback()
                }
                BarButton {
                    symbol: "next"
                    text: "下一首"
                    tint: bar.ink
                    enabled: bar.library.hasNext && !(bar.library.roamPlaying && bar.library.playbackBusy)
                    onClicked: bar.library.next()
                }
                BarButton {
                    symbol: bar.modeIcons[bar.library.playMode]
                    text: bar.library.roamPlaying ? "漫游中，按推荐顺序播放" : bar.modeNames[bar.library.playMode]
                    tint: bar.library.playMode === 0 || bar.library.roamPlaying ? bar.soft : Theme.accent
                    enabled: !bar.library.roamPlaying
                    onClicked: bar.library.cyclePlayMode()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: !bar.stage
                Label {
                    text: Theme.time(progress.pressed ? progress.value : bar.library.playbackPosition)
                    color: Theme.muted
                    font.pixelSize: 11
                }
                PlayerSlider {
                    id: progress
                    property real pendingSeek: 0
                    segmented: true
                    bufferedRegions: bar.library.bufferedRanges
                    playedFraction: bar.duration > 0 ? (pressed ? value : bar.library.playbackPosition) / bar.duration : 0
                    Layout.fillWidth: true
                    from: 0
                    to: Math.max(1, bar.duration)
                    enabled: !bar.library.playbackBusy && bar.library.canSeekPlayback && bar.duration > 0
                    Accessible.name: "播放进度"
                    Binding on value {
                        value: bar.library.playbackPosition
                        when: !progress.pressed
                        restoreMode: Binding.RestoreNone
                    }
                    onMoved: {
                        if (pressed)
                            pendingSeek = value
                        else
                            bar.library.seekPlayback(value)
                    }
                    onPressedChanged: {
                        if (pressed)
                            pendingSeek = value
                        else if (enabled)
                            bar.library.seekPlayback(pendingSeek)
                    }
                }
                Label {
                    text: Theme.time(bar.duration)
                    color: Theme.muted
                    font.pixelSize: 11
                }
            }
        }

        RowLayout {
            Layout.preferredWidth: 330
            Layout.minimumWidth: 170
            Layout.fillWidth: true
            spacing: 4
            Item { Layout.fillWidth: true }
            BarButton {
                symbol: bar.library.audio.muted || bar.library.audio.volume === 0 ? "muted" : "volume"
                text: bar.library.audio.muted || bar.library.audio.volume === 0 ? "恢复声音" : "静音"
                onClicked: bar.library.toggleMute()
            }
            PlayerSlider {
                Layout.preferredWidth: 86
                from: 0
                to: 1
                value: bar.library.audio.muted ? 0 : bar.library.audio.volume
                Accessible.name: "音量"
                onMoved: {
                    bar.library.audio.volume = value
                    bar.library.audio.muted = false
                }
            }
            BarButton {
                id: lyricsButton
                symbol: ""
                glyph: "词"
                iconSize: 17
                text: !bar.desktopLyricsShown ? "打开桌面歌词（右键切换风格）"
                      : bar.desktopLyricsLocked ? "解锁桌面歌词（右键切换风格）" : "关闭桌面歌词（右键切换风格）"
                selected: bar.desktopLyricsShown
                onClicked: bar.desktopLyricsRequested()
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: lyricsMenu.popup(lyricsButton, -lyricsMenu.implicitWidth + lyricsButton.width, -lyricsMenu.implicitHeight - 6)
                }
            }
            Menu {
                id: lyricsMenu
                padding: 6
                background: Rectangle {
                    implicitWidth: 168
                    radius: 8
                    color: Theme.panel
                    border.color: Theme.line
                }
                Instantiator {
                    model: [
                        { key: "vinyl", label: "黑胶胶囊（风格 B）" },
                        { key: "stage", label: "舞台双轨（风格 C）" },
                        { key: "card",  label: "唱片铭牌（风格 D）" }
                    ]
                    delegate: MenuItem {
                        id: styleEntry
                        required property var modelData
                        text: styleEntry.modelData.label
                        implicitHeight: 32
                        onTriggered: bar.desktopLyricsStyleSelected(styleEntry.modelData.key)
                        contentItem: RowLayout {
                            spacing: 6
                            Label {
                                Layout.fillWidth: true
                                leftPadding: 6
                                text: styleEntry.text
                                color: bar.desktopLyricsStyle === styleEntry.modelData.key ? Theme.accent : Theme.text
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                            }
                            Symbol {
                                Layout.preferredWidth: 13
                                Layout.preferredHeight: 13
                                Layout.rightMargin: 4
                                visible: bar.desktopLyricsStyle === styleEntry.modelData.key
                                name: "check"
                                tint: Theme.accent
                            }
                        }
                        background: Rectangle { radius: 5; color: Theme.interactive("transparent", styleEntry.highlighted, styleEntry.down) }
                    }
                    onObjectAdded: (index, object) => lyricsMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => lyricsMenu.removeItem(object)
                }
            }
            BarButton {
                symbol: "output"
                text: "音频输出 · " + bar.library.outputName
                selected: outputChooser.opened
                onClicked: outputChooser.open()
            }
            BarButton {
                symbol: "queue"
                text: "播放列表"
                selected: bar.queueVisible
                visible: !bar.stage
                onClicked: bar.queueRequested()
            }
        }
    }

    component AudioActionButton: AbstractButton {
        id: deviceButton
        property bool selected: false
        Layout.fillWidth: true
        implicitHeight: 34
        hoverEnabled: true
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        contentItem: RowLayout {
            Label {
                Layout.fillWidth: true
                leftPadding: 8
                text: deviceButton.text
                color: !deviceButton.enabled ? Theme.muted : deviceButton.selected ? Theme.accent : Theme.text
                elide: Text.ElideRight
                font.pixelSize: 12
            }
            Symbol {
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
                Layout.rightMargin: 8
                visible: deviceButton.selected
                name: "check"
                tint: Theme.accent
            }
        }
        background: Rectangle {
            radius: 6
            color: Theme.interactive("transparent", deviceButton.hovered, deviceButton.down)
        }
    }

    Popup {
        id: outputChooser
        x: bar.width - width - 20
        y: -height - 8
        width: 300
        padding: 10
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: Theme.panel
            radius: 10
            border.color: Theme.line
        }
        contentItem: ColumnLayout {
            spacing: 2
            Label {
                Layout.fillWidth: true
                Layout.margins: 8
                text: "音频输出"
                color: Theme.text
                font.pixelSize: 13
                font.bold: true
            }
            AudioActionButton {
                text: "跟随系统默认"
                selected: bar.library.outputId === ""
                onClicked: {
                    bar.library.selectOutput("")
                    outputChooser.close()
                }
            }
            Repeater {
                model: bar.library.outputDevices
                delegate: AudioActionButton {
                    required property var modelData
                    text: modelData.label
                    selected: bar.library.outputId === modelData.id
                    onClicked: {
                        bar.library.selectOutput(modelData.id)
                        outputChooser.close()
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                Layout.preferredHeight: 1
                color: Theme.line
            }
            AudioActionButton {
                text: "切换音源"
                enabled: bar.hasTrack && !bar.library.playbackBusy
                onClicked: {
                    bar.library.switchAudioStream()
                    outputChooser.close()
                }
            }
        }
    }
}
