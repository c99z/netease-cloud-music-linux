pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtMultimedia
import QtCore

// Floating desktop lyric window supporting 3 manually switchable styles:
// - "vinyl": 黑胶胶囊 (B) — spinning vinyl disc cover, equalizer/controls swap, bottom hairline progress
// - "stage": 舞台双轨 (C) — left accent rail, dual-track alternating lines, hover top bar
// - "card":  唱片铭牌 (D) — Theme.dark-adaptive frosted card with album cover & timestamp
// Plus persistent style, font size, backdropOpacity, and contentOpacity controls.
Window {
    id: strip
    required property Library library
    property bool shown: false
    property bool locked: false
    property int fontSize: 22
    property string styleMode: "vinyl" // "vinyl" | "stage" | "card"
    property real backdropOpacity: 0.78
    property real contentOpacity: 0.94
    signal lockedByUser()

    readonly property var lines: library.lyrics
    readonly property bool timed: lines.length > 0 && lines[0].time >= 0
    readonly property bool playing: library.media.playbackState === MediaPlayer.PlayingState
                                    && !library.playbackBusy && library.playbackError.length === 0
    readonly property int current: lineAt(clock.position)
    readonly property var track: library.currentTrack
    readonly property bool hasTrack: !!track.id
    readonly property bool liked: library.likesRevision >= 0 && library.currentLiked
    readonly property real duration: library.playbackDuration || track.duration || 0
    readonly property bool lightSurface: styleMode === "card" && !Theme.dark && backdropOpacity > 0.25
    readonly property bool settingsOpen: settingsPopup.visible
    readonly property bool hovering: !locked && (hover.hovered || toolbarHover.hovered || settingsOpen)

    readonly property var styleOptions: [
        { key: "vinyl", label: "黑胶胶囊", shortLabel: "B · 胶囊" },
        { key: "stage", label: "舞台双轨", shortLabel: "C · 双轨" },
        { key: "card",  label: "唱片铭牌", shortLabel: "D · 铭牌" }
    ]

    function styleLabel(key: string): string {
        for (const item of styleOptions) {
            if (item.key === key)
                return item.label
        }
        return "黑胶胶囊"
    }

    function cycleStyle() {
        const order = ["vinyl", "stage", "card"]
        const idx = order.indexOf(styleMode)
        styleMode = order[(idx + 1) % order.length]
    }

    width: {
        const cap = Math.min(820, Math.max(540, Screen.width * .48))
        if (styleMode === "stage")
            return Math.min(860, Math.max(580, cap + 30))
        return cap
    }
    height: Math.max(110, Math.ceil(shell.implicitHeight + (settingsPopup.visible ? settingsPopup.height + 14 : 0) + 16))
    visible: shown
    transientParent: null
    color: "transparent"
    title: "桌面歌词"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
           | (locked ? Qt.WindowTransparentForInput : 0)

    Settings {
        category: "desktopLyrics"
        property alias shown: strip.shown
        property alias locked: strip.locked
        property alias fontSize: strip.fontSize
        property alias styleMode: strip.styleMode
        property alias backdropOpacity: strip.backdropOpacity
        property alias contentOpacity: strip.contentOpacity
        property alias x: strip.x
        property alias y: strip.y
    }

    Component.onCompleted: {
        if (!["vinyl", "stage", "card"].includes(styleMode))
            styleMode = "vinyl"
        clock.sync()
        if ((x <= 0 && y <= 0) || x > Screen.desktopAvailableWidth || y > Screen.desktopAvailableHeight) {
            x = Screen.virtualX + (Screen.width - width) / 2
            y = Screen.virtualY + Screen.desktopAvailableHeight - height - 70
        }
    }

    onVisibleChanged: {
        if (visible) {
            clock.sync()
            library.loadCurrentLyrics()
        } else {
            settingsPopup.visible = false
        }
    }
    onLockedChanged: {
        if (locked)
            settingsPopup.visible = false
    }
    Connections {
        target: strip.library
        enabled: strip.visible
        function onQueueChanged() { strip.library.loadCurrentLyrics() }
    }

    function lineAt(position: real): int {
        if (!timed)
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

    // playbackPosition arrives in coarse steps; extrapolate between them so the sung
    // portion of the line sweeps smoothly instead of jumping.
    QtObject {
        id: clock
        property real anchor: 0
        property real anchorAt: 0
        property real position: 0
        function sync() {
            anchor = strip.library.playbackPosition
            anchorAt = Date.now()
            position = anchor
        }
    }
    Connections {
        target: strip.library
        enabled: strip.visible
        function onPlaybackPositionChanged() { clock.sync() }
    }
    Timer {
        interval: 16
        repeat: true
        running: strip.visible && strip.playing && strip.timed
        onRunningChanged: clock.sync()
        onTriggered: clock.position = clock.anchor + (Date.now() - clock.anchorAt)
    }

    readonly property string firstText: {
        if (!track.id)
            return Qt.application.displayName
        if (current >= 0)
            return lines[current].text
        if (timed)
            return lines[0].time > 0 && clock.position < lines[0].time ? track.title + " - " + track.artist : lines[0].text
        return track.title + " - " + track.artist
    }
    readonly property string secondText: {
        if (!timed)
            return library.lyricsBusy ? "正在加载歌词…" : track.id ? "暂无歌词" : ""
        const line = current >= 0 ? lines[current] : null
        if (line && line.translation)
            return line.translation
        const next = current + 1
        return next < lines.length ? lines[next].text : ""
    }
    readonly property string nextLineText: {
        if (!timed)
            return secondText
        const next = current + 1
        if (next < lines.length && lines[next].text)
            return lines[next].text
        const line = current >= 0 ? lines[current] : null
        return line && line.translation ? line.translation : ""
    }
    readonly property string trackTitleLine: {
        if (!track.id)
            return Qt.application.displayName + " · 等待播放"
        return track.artist ? track.title + " — " + track.artist : track.title
    }
    readonly property var currentWords: current >= 0 && lines[current].words ? lines[current].words : []
    readonly property bool hasWordTiming: currentWords && currentWords.length > 0
    readonly property real progress: {
        if (current < 0)
            return 0
        const line = lines[current]
        const pos = clock.position
        const words = line.words
        const totalChars = line.text ? line.text.length : 0

        // Word-level YRC timing: interpolate inside the active word so sweep follows vocals
        // across pauses, melisma, and fast/slow syllables.
        if (words && words.length > 0 && totalChars > 0) {
            if (pos <= words[0].time)
                return 0
            let sungChars = 0
            for (let i = 0; i < words.length; ++i) {
                const w = words[i]
                const wStart = w.time
                const wEnd = w.duration > 0 ? wStart + w.duration
                           : (i + 1 < words.length ? words[i + 1].time : wStart + 250)
                if (pos >= wEnd) {
                    sungChars = w.offset + w.length
                    continue
                }
                if (pos <= wStart)
                    return Math.max(0, Math.min(1, sungChars / totalChars))
                const frac = wEnd > wStart ? (pos - wStart) / (wEnd - wStart) : 1
                const activeChars = w.offset + w.length * Math.max(0, Math.min(1, frac))
                return Math.max(0, Math.min(1, activeChars / totalChars))
            }
            return 1
        }

        // Line-level LRC fallback: when a song only provides per-line timestamps (no YRC),
        // don't stretch the sweep across long post-line pauses or interludes; keep the line
        // highlighted smoothly over the estimated singing duration instead.
        const start = line.time
        const nextTime = current + 1 < lines.length ? lines[current + 1].time
                       : Math.max(start + 4000, library.playbackDuration)
        const span = Math.max(400, nextTime - start)
        const estimated = Math.max(1200, Math.min(span, totalChars * 280 + 500))
        const end = line.duration > 0 ? Math.min(nextTime, start + line.duration)
                  : (span > estimated + 1500 ? start + estimated : nextTime)
        return end > start ? Math.max(0, Math.min(1, (pos - start) / (end - start))) : 1
    }

    component StripButton: IconButton {
        property bool lightMode: false
        onDark: !lightMode
        tint: lightMode ? "#3b4354" : "#ebe5e2"
        iconSize: 15
        width: 28
        height: 28
    }

    component LyricSlider: Slider {
        id: lSlider
        hoverEnabled: true
        implicitHeight: 16
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        background: Rectangle {
            x: lSlider.leftPadding
            y: lSlider.topPadding + (lSlider.availableHeight - height) / 2
            width: lSlider.availableWidth
            height: 4
            radius: 2
            color: "#33ffffff"
            Rectangle {
                width: lSlider.visualPosition * parent.width
                height: parent.height
                radius: 2
                color: Theme.accent
            }
        }
        handle: Rectangle {
            x: lSlider.leftPadding + lSlider.visualPosition * (lSlider.availableWidth - width)
            y: lSlider.topPadding + (lSlider.availableHeight - height) / 2
            width: 12
            height: 12
            radius: 6
            color: "#ffffff"
            border.color: Theme.accent
            border.width: 2
        }
    }

    // Dual-layer karaoke line with soft shadow readability at low backdrop opacity.
    // Uses FontMetrics to map YRC character progress to exact pixel widths when word timings exist.
    component KaraokeLine: Item {
        id: kLine
        property string text: ""
        property var words: []
        property real position: 0
        property int pixelSize: 22
        property real progress: 0
        property bool active: false
        property color baseColor: "#66ffffff"
        property color sungColor: "#ffffff"
        property color shadowColor: "#a6000000"
        property bool softShadow: true
        property int alignment: Text.AlignLeft

        implicitHeight: Math.ceil(pixelSize * 1.38)
        implicitWidth: baseText.implicitWidth

        TextMetrics {
            id: wholeWordMetrics
            font: baseText.font
            text: sungClip.wholeText
        }
        TextMetrics {
            id: partialWordMetrics
            font: baseText.font
            text: sungClip.nextText
        }

        Text {
            id: baseText
            anchors.fill: parent
            text: kLine.text
            color: kLine.baseColor
            font.family: Theme.font
            font.pixelSize: kLine.pixelSize
            font.weight: Font.DemiBold
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 13
            elide: Text.ElideRight
            horizontalAlignment: kLine.alignment
            verticalAlignment: Text.AlignVCenter
            textFormat: Text.PlainText
            renderType: Text.QtRendering
            style: kLine.softShadow ? Text.Raised : Text.Normal
            styleColor: kLine.shadowColor
        }

        Item {
            id: sungClip
            readonly property real paintedW: Math.min(kLine.width, baseText.paintedWidth)
            readonly property real scaleFit: baseText.implicitWidth > 0 ? paintedW / baseText.implicitWidth : 1
            readonly property real offsetX: kLine.alignment === Text.AlignHCenter
                                          ? Math.max(0, (kLine.width - paintedW) / 2)
                                          : kLine.alignment === Text.AlignRight
                                            ? Math.max(0, kLine.width - paintedW)
                                            : 0
            readonly property var wordSlice: {
                const list = kLine.words
                const pos = kLine.position
                if (!kLine.active || !list || list.length === 0)
                    return null
                if (pos <= list[0].time)
                    return { whole: 0, partialEnd: 0, frac: 0 }
                let whole = 0
                for (let i = 0; i < list.length; ++i) {
                    const w = list[i]
                    const wStart = w.time
                    const wEnd = w.duration > 0 ? wStart + w.duration
                               : (i + 1 < list.length ? list[i + 1].time : wStart + 250)
                    if (pos >= wEnd) {
                        whole = w.offset + w.length
                        continue
                    }
                    if (pos <= wStart)
                        return { whole: whole, partialEnd: whole, frac: 0 }
                    const frac = wEnd > wStart ? Math.max(0, Math.min(1, (pos - wStart) / (wEnd - wStart))) : 1
                    return { whole: w.offset, partialEnd: w.offset + w.length, frac: frac }
                }
                return { whole: kLine.text.length, partialEnd: kLine.text.length, frac: 0 }
            }
            readonly property string wholeText: wordSlice ? kLine.text.slice(0, wordSlice.whole) : ""
            readonly property string nextText: wordSlice ? kLine.text.slice(0, wordSlice.partialEnd) : ""
            readonly property real targetWidth: {
                if (!kLine.active)
                    return 0
                if (wordSlice) {
                    const w0 = wholeWordMetrics.advanceWidth
                    const w1 = partialWordMetrics.advanceWidth
                    const rawPx = w0 + (w1 - w0) * wordSlice.frac
                    return Math.max(0, Math.min(paintedW, rawPx * scaleFit))
                }
                return paintedW * Math.max(0, Math.min(1, kLine.progress))
            }
            x: offsetX
            y: 0
            width: targetWidth
            height: parent.height
            clip: true
            visible: kLine.active && targetWidth > 0

            Text {
                x: -sungClip.offsetX
                y: 0
                width: kLine.width
                height: kLine.height
                text: kLine.text
                color: kLine.sungColor
                font.family: Theme.font
                font.pixelSize: kLine.pixelSize
                font.weight: Font.DemiBold
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: 13
                elide: Text.ElideRight
                horizontalAlignment: kLine.alignment
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.PlainText
                renderType: Text.QtRendering
                style: kLine.softShadow ? Text.Raised : Text.Normal
                styleColor: kLine.shadowColor
            }
        }
    }

    component ToolbarButtons: Row {
        id: tbRow
        property bool compact: false
        property bool lightMode: false
        spacing: compact ? 1 : 2

        StripButton {
            lightMode: tbRow.lightMode
            symbol: "heart"
            filled: strip.liked
            tint: strip.liked ? Theme.accent : (tbRow.lightMode ? "#3b4354" : "#ebe5e2")
            text: strip.liked ? "取消喜欢" : "喜欢"
            enabled: strip.hasTrack
            visible: !tbRow.compact || strip.hasTrack
            onClicked: strip.library.loggedIn ? strip.library.toggleLike() : strip.library.notification("请先在主窗口登录")
        }
        StripButton {
            lightMode: tbRow.lightMode
            symbol: "previous"
            text: "上一首"
            onClicked: strip.library.previous()
        }
        StripButton {
            lightMode: tbRow.lightMode
            symbol: strip.playing ? "pause" : "play"
            text: strip.playing ? "暂停" : "播放"
            featured: true
            width: 28
            height: 28
            onClicked: strip.library.togglePlayback()
        }
        StripButton {
            lightMode: tbRow.lightMode
            symbol: "next"
            text: "下一首"
            onClicked: strip.library.next()
        }
        Item { width: tbRow.compact ? 4 : 6; height: 1 }
        StripButton {
            lightMode: tbRow.lightMode
            symbol: ""
            glyph: "A−"
            text: "缩小字号"
            enabled: strip.fontSize > 16
            onClicked: strip.fontSize = Math.max(16, strip.fontSize - 2)
        }
        StripButton {
            lightMode: tbRow.lightMode
            symbol: ""
            glyph: "A+"
            text: "放大字号"
            enabled: strip.fontSize < 38
            onClicked: strip.fontSize = Math.min(38, strip.fontSize + 2)
        }
        StripButton {
            lightMode: tbRow.lightMode
            symbol: "style"
            selected: false
            text: "切换风格（当前：" + strip.styleLabel(strip.styleMode) + "）"
            onClicked: strip.cycleStyle()
        }
        StripButton {
            lightMode: tbRow.lightMode
            symbol: "opacity"
            selected: settingsPopup.visible
            text: "外观与透明度"
            onClicked: settingsPopup.visible = !settingsPopup.visible
        }
        StripButton {
            lightMode: tbRow.lightMode
            symbol: "lock"
            text: "锁定（点击穿透）"
            onClicked: {
                strip.locked = true
                strip.lockedByUser()
            }
        }
        StripButton {
            lightMode: tbRow.lightMode
            symbol: "close"
            text: "关闭桌面歌词"
            onClicked: strip.shown = false
        }
    }

    // Outer container handles overall resting/hover opacity & window dragging
    Item {
        id: root
        anchors.fill: parent

        Item {
            id: hitArea
            anchors.fill: shell
            HoverHandler { id: hover }
            DragHandler {
                target: null
                enabled: !strip.locked && !settingsPopup.visible
                onActiveChanged: if (active) strip.startSystemMove()
            }
            TapHandler {
                acceptedButtons: Qt.RightButton
                enabled: !strip.locked
                onTapped: settingsPopup.visible = !settingsPopup.visible
            }
        }

        Item {
            id: shell
            anchors.top: parent.top
            anchors.topMargin: 6
            anchors.left: parent.left
            anchors.right: parent.right
            implicitHeight: {
                if (strip.styleMode === "vinyl")
                    return Math.max(76, Math.round(strip.fontSize * 2.55) + 24)
                if (strip.styleMode === "stage")
                    return Math.max(104, Math.round(strip.fontSize * 2.65) + 52)
                return Math.max(88, Math.round(strip.fontSize * 2.45) + 42)
            }
            opacity: strip.hovering ? 1.0 : strip.contentOpacity
            Behavior on opacity { NumberAnimation { duration: 160 } }

            // =================================================================
            // STYLE B: 黑胶唱片舱 · 灵动胶囊 ("vinyl")
            // =================================================================
            Rectangle {
                id: vinylCard
                anchors.fill: parent
                visible: strip.styleMode === "vinyl"
                radius: height / 2
                color: Qt.rgba(22 / 255, 20 / 255, 25 / 255, 0.90 * strip.backdropOpacity)
                border.width: strip.backdropOpacity > 0.04 || strip.hovering ? 1 : 0
                border.color: strip.hovering
                              ? Qt.rgba(1, 1, 1, Math.max(0.22, 0.24 * strip.backdropOpacity))
                              : Qt.rgba(1, 1, 1, 0.14 * strip.backdropOpacity)
                clip: true

                // Warm ambient vinyl glow behind the disc
                Rectangle {
                    x: -14
                    y: -14
                    width: parent.height + 48
                    height: parent.height + 28
                    radius: width / 2
                    visible: strip.backdropOpacity > 0.06
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(252 / 255, 61 / 255, 73 / 255, 0.22 * strip.backdropOpacity) }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }

                // Hover dim scrim when backdrop is transparent so buttons stay clickable & legible
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: "#66141418"
                    opacity: strip.hovering && strip.backdropOpacity < 0.35 ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 16
                    spacing: 14

                    // Spinning vinyl disc with album cover
                    Item {
                        Layout.preferredWidth: Math.min(58, shell.height - 20)
                        Layout.preferredHeight: Layout.preferredWidth
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            id: vinylDisc
                            anchors.fill: parent
                            radius: width / 2
                            color: "#151518"
                            border.width: 1.5
                            border.color: "#2cffffff"

                            RotationAnimator on rotation {
                                from: 0
                                to: 360
                                duration: 16000
                                loops: Animation.Infinite
                                running: strip.visible && strip.playing && strip.styleMode === "vinyl"
                            }

                            Repeater {
                                model: 3
                                delegate: Rectangle {
                                    required property int index
                                    anchors.centerIn: parent
                                    width: parent.width * (0.88 - index * 0.1)
                                    height: width
                                    radius: width / 2
                                    color: "transparent"
                                    border.color: "#1cffffff"
                                }
                            }

                            Cover {
                                anchors.centerIn: parent
                                width: parent.width * 0.62
                                height: width
                                round: true
                                source: strip.track.art || ""
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                width: 6
                                height: 6
                                radius: 3
                                color: "#191718"
                                border.width: 1
                                border.color: "#66ffffff"
                            }
                        }
                    }

                    // Lyric dual lines
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2

                        KaraokeLine {
                            Layout.fillWidth: true
                            text: strip.firstText
                            words: strip.currentWords
                            position: clock.position
                            pixelSize: Math.max(17, Math.round(strip.fontSize * 0.92))
                            progress: strip.progress
                            active: strip.current >= 0
                            baseColor: strip.backdropOpacity > 0.3 ? "#78ffffff" : "#ebffffff"
                            sungColor: strip.backdropOpacity > 0.3 ? "#ffffff" : "#ff636b"
                            shadowColor: "#c0000000"
                            softShadow: true
                            alignment: Text.AlignLeft
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: strip.secondText
                            color: strip.backdropOpacity > 0.3 ? "#b0e6ded9" : "#e8e2df"
                            font.family: Theme.font
                            font.pixelSize: Math.max(12, Math.round(strip.fontSize * 0.56))
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                            renderType: Text.QtRendering
                            style: Text.Raised
                            styleColor: "#b8000000"
                        }
                    }

                    // Right area: breathing EQ at rest -> control strip on hover
                    Item {
                        Layout.preferredWidth: strip.hovering ? vinylControls.implicitWidth + 6 : 28
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignVCenter
                        Behavior on Layout.preferredWidth { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                        Row {
                            id: eqBars
                            anchors.centerIn: parent
                            spacing: 3
                            height: 16
                            opacity: strip.hovering ? 0 : 0.92
                            visible: opacity > 0
                            Behavior on opacity { NumberAnimation { duration: 140 } }

                            Repeater {
                                model: [
                                    { minH: 5, maxH: 14, dur: 460 },
                                    { minH: 7, maxH: 16, dur: 380 },
                                    { minH: 4, maxH: 12, dur: 520 },
                                    { minH: 6, maxH: 15, dur: 410 }
                                ]
                                delegate: Rectangle {
                                    id: eqBar
                                    required property var modelData
                                    width: 3
                                    radius: 1.5
                                    anchors.bottom: parent.bottom
                                    height: strip.playing ? modelData.maxH : 4
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: "#ff8c7a" }
                                        GradientStop { position: 1.0; color: Theme.accent }
                                    }
                                    SequentialAnimation on height {
                                        running: strip.visible && strip.playing && strip.styleMode === "vinyl" && !strip.hovering
                                        loops: Animation.Infinite
                                        NumberAnimation { from: eqBar.modelData.minH; to: eqBar.modelData.maxH; duration: eqBar.modelData.dur; easing.type: Easing.InOutSine }
                                        NumberAnimation { from: eqBar.modelData.maxH; to: eqBar.modelData.minH; duration: eqBar.modelData.dur; easing.type: Easing.InOutSine }
                                    }
                                }
                            }
                        }

                        ToolbarButtons {
                            id: vinylControls
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            compact: true
                            opacity: strip.hovering ? 1 : 0
                            enabled: opacity > 0
                            Behavior on opacity { NumberAnimation { duration: 160 } }
                        }
                    }
                }

                // Bottom hairline progress bar (visible as a subtle accent when song has line-level LRC;
                // tracks exact word-level vocal progress when YRC exists).
                Item {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: parent.height * 0.45
                    anchors.rightMargin: parent.height * 0.45
                    height: 2
                    visible: strip.current >= 0 && (strip.backdropOpacity > 0.1 || strip.hovering)
                    Rectangle {
                        anchors.fill: parent
                        radius: 1
                        color: Qt.rgba(1, 1, 1, 0.12 * Math.max(0.25, strip.backdropOpacity))
                    }
                    Rectangle {
                        height: parent.height
                        width: parent.width * strip.progress
                        radius: 1
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.accent }
                            GradientStop { position: 1.0; color: "#ff8f70" }
                        }
                    }
                }
            }

            // =================================================================
            // STYLE C: 双行交替 · 舞台双轨 ("stage")
            // =================================================================
            Rectangle {
                id: stageCard
                anchors.fill: parent
                visible: strip.styleMode === "stage"
                radius: 16
                color: strip.hovering
                       ? Qt.rgba(18 / 255, 19 / 255, 25 / 255, Math.max(0.76, 0.88 * strip.backdropOpacity))
                       : Qt.rgba(18 / 255, 19 / 255, 25 / 255, 0.72 * strip.backdropOpacity)
                border.width: strip.backdropOpacity > 0.04 || strip.hovering ? 1 : 0
                border.color: strip.hovering
                              ? Qt.rgba(1, 1, 1, 0.20)
                              : Qt.rgba(1, 1, 1, 0.12 * strip.backdropOpacity)
                Behavior on color { ColorAnimation { duration: 160 } }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    anchors.topMargin: 8
                    anchors.bottomMargin: 12
                    spacing: 4

                    // Slim hover topbar: track info on left, controls on right
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        opacity: strip.hovering ? 1 : 0
                        enabled: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 150 } }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 7
                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: Theme.accent
                            }
                            Label {
                                Layout.fillWidth: true
                                text: strip.trackTitleLine
                                color: "#d4ceca"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                textFormat: Text.PlainText
                            }
                        }

                        ToolbarButtons {
                            compact: false
                        }
                    }

                    // Dual alternating lines: active line left-aligned with red bar, next line right-aligned
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                Layout.preferredWidth: 3.5
                                Layout.preferredHeight: Math.max(20, Math.round(strip.fontSize * 0.95))
                                Layout.alignment: Qt.AlignVCenter
                                radius: 2
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#ff7e6e" }
                                    GradientStop { position: 1.0; color: Theme.accent }
                                }
                            }
                            KaraokeLine {
                                Layout.fillWidth: true
                                text: strip.firstText
                                words: strip.currentWords
                                position: clock.position
                                pixelSize: Math.max(18, strip.fontSize)
                                progress: strip.progress
                                active: strip.current >= 0
                                baseColor: strip.backdropOpacity > 0.25 ? "#66ffffff" : "#f0ffffff"
                                sungColor: strip.backdropOpacity > 0.25 ? "#ffffff" : "#ff636b"
                                shadowColor: "#cc000000"
                                softShadow: true
                                alignment: Text.AlignLeft
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                visible: strip.nextLineText.length > 0 && strip.timed && !(strip.current >= 0 && strip.lines[strip.current].translation)
                                implicitWidth: nextTag.implicitWidth + 10
                                implicitHeight: 16
                                radius: 4
                                color: "transparent"
                                border.width: 1
                                border.color: "#2bffffff"
                                Label {
                                    id: nextTag
                                    anchors.centerIn: parent
                                    text: "NEXT"
                                    color: "#80ffffff"
                                    font.pixelSize: 9
                                    font.weight: Font.Bold
                                }
                            }
                            Text {
                                Layout.maximumWidth: stageCard.width - 100
                                visible: text.length > 0
                                text: strip.secondText
                                color: "#cdebe5e2"
                                font.family: Theme.font
                                font.pixelSize: Math.max(13, Math.round(strip.fontSize * 0.68))
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignRight
                                textFormat: Text.PlainText
                                renderType: Text.QtRendering
                                style: Text.Raised
                                styleColor: "#bb000000"
                            }
                        }
                    }
                }
            }

            // =================================================================
            // STYLE D: 唱片室铭牌 · 主题自适应卡 ("card")
            // =================================================================
            Rectangle {
                id: plaqueCard
                anchors.fill: parent
                visible: strip.styleMode === "card"
                radius: 15
                color: {
                    const a = strip.hovering ? Math.max(0.84, 0.94 * strip.backdropOpacity) : 0.92 * strip.backdropOpacity
                    if (strip.lightSurface)
                        return Qt.rgba(252 / 255, 250 / 255, 248 / 255, a)
                    return Qt.rgba(34 / 255, 29 / 255, 28 / 255, a)
                }
                border.width: strip.backdropOpacity > 0.04 || strip.hovering ? 1 : 0
                border.color: strip.lightSurface
                              ? Qt.rgba(40 / 255, 50 / 255, 72 / 255, 0.14)
                              : Qt.rgba(1, 240 / 255, 232 / 255, 0.14 * Math.max(0.4, strip.backdropOpacity))
                Behavior on color { ColorAnimation { duration: 160 } }

                // Left NetEase red accent rail
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.topMargin: 14
                    anchors.bottomMargin: 14
                    width: 3.5
                    radius: 2
                    color: Theme.accent
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 10
                    anchors.bottomMargin: 10
                    spacing: 14

                    Cover {
                        Layout.preferredWidth: Math.min(62, shell.height - 24)
                        Layout.preferredHeight: Layout.preferredWidth
                        Layout.alignment: Qt.AlignVCenter
                        edge: 10
                        source: strip.track.art || ""
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 3

                        // Header: mini badge + track title + hover toolbar
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            spacing: 6

                            Rectangle {
                                implicitWidth: 18
                                implicitHeight: 16
                                radius: 4
                                color: Qt.rgba(252 / 255, 61 / 255, 73 / 255, 0.14)
                                Label {
                                    anchors.centerIn: parent
                                    text: "词"
                                    color: Theme.accent
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: strip.trackTitleLine
                                color: strip.lightSurface ? "#5c6679" : "#b8aca6"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                textFormat: Text.PlainText
                            }

                            ToolbarButtons {
                                compact: false
                                lightMode: strip.lightSurface
                                opacity: strip.hovering ? 1 : 0
                                enabled: opacity > 0
                                Behavior on opacity { NumberAnimation { duration: 150 } }
                            }
                        }

                        KaraokeLine {
                            Layout.fillWidth: true
                            text: strip.firstText
                            words: strip.currentWords
                            position: clock.position
                            pixelSize: Math.max(17, Math.round(strip.fontSize * 0.95))
                            progress: strip.progress
                            active: strip.current >= 0
                            baseColor: strip.lightSurface ? "#55283248" : (strip.backdropOpacity > 0.25 ? "#66f3ece8" : "#f0ffffff")
                            sungColor: strip.lightSurface ? "#1f2738" : (strip.backdropOpacity > 0.25 ? "#ffffff" : "#ff636b")
                            shadowColor: strip.lightSurface ? "transparent" : "#b8000000"
                            softShadow: !strip.lightSurface
                            alignment: Text.AlignLeft
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: strip.secondText
                                color: strip.lightSurface ? "#6b7588" : "#b0a49e"
                                font.family: Theme.font
                                font.pixelSize: Math.max(12, Math.round(strip.fontSize * 0.58))
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                textFormat: Text.PlainText
                                renderType: Text.QtRendering
                                style: strip.lightSurface ? Text.Normal : Text.Raised
                                styleColor: "#a0000000"
                            }
                            Label {
                                visible: strip.duration > 0
                                text: Theme.time(strip.library.playbackPosition) + " / " + Theme.time(strip.duration)
                                color: strip.lightSurface ? "#838c9e" : "#9c908a"
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }

            // Shared hover catcher over the whole toolbar region inside any style
            HoverHandler {
                id: toolbarHover
            }
        }

        // =====================================================================
        // Settings Popover Panel (style switcher + dual opacity sliders)
        // =====================================================================
        Rectangle {
            id: settingsPopup
            visible: false
            width: Math.min(460, strip.width - 24)
            height: settingsCol.implicitHeight + 24
            anchors.top: shell.bottom
            anchors.topMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter
            radius: 14
            color: "#ee1b1b22"
            border.width: 1
            border.color: "#2bffffff"

            // Absorb clicks inside the panel so they don't start a window drag
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
            }

            ColumnLayout {
                id: settingsCol
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                // Style segmented selector
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label {
                        text: "视觉风格"
                        color: "#cfc9c5"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: strip.styleOptions
                            delegate: AbstractButton {
                                id: styleBtn
                                required property var modelData
                                readonly property bool currentStyle: strip.styleMode === modelData.key
                                Layout.fillWidth: true
                                implicitHeight: 28
                                hoverEnabled: true
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                onClicked: strip.styleMode = modelData.key
                                background: Rectangle {
                                    radius: 7
                                    color: styleBtn.currentStyle ? Theme.accent
                                         : styleBtn.hovered ? "#28ffffff" : "#14ffffff"
                                    border.width: 1
                                    border.color: styleBtn.currentStyle ? Theme.accent : "#1affffff"
                                }
                                contentItem: Label {
                                    text: styleBtn.modelData.label
                                    color: styleBtn.currentStyle ? "#ffffff" : "#e2dcd8"
                                    font.pixelSize: 12
                                    font.weight: styleBtn.currentStyle ? Font.DemiBold : Font.Normal
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }

                // Backdrop opacity slider
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        Layout.preferredWidth: 74
                        text: "底托透明"
                        color: "#cfc9c5"
                        font.pixelSize: 12
                    }
                    LyricSlider {
                        Layout.fillWidth: true
                        from: 0.0
                        to: 1.0
                        stepSize: 0.02
                        value: strip.backdropOpacity
                        onMoved: strip.backdropOpacity = value
                    }
                    Label {
                        Layout.preferredWidth: 38
                        horizontalAlignment: Text.AlignRight
                        text: Math.round(strip.backdropOpacity * 100) + "%"
                        color: "#f4efec"
                        font.pixelSize: 12
                    }
                }

                // Overall content opacity slider
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        Layout.preferredWidth: 74
                        text: "静置透明"
                        color: "#cfc9c5"
                        font.pixelSize: 12
                    }
                    LyricSlider {
                        Layout.fillWidth: true
                        from: 0.25
                        to: 1.0
                        stepSize: 0.02
                        value: strip.contentOpacity
                        onMoved: strip.contentOpacity = value
                    }
                    Label {
                        Layout.preferredWidth: 38
                        horizontalAlignment: Text.AlignRight
                        text: Math.round(strip.contentOpacity * 100) + "%"
                        color: "#f4efec"
                        font.pixelSize: 12
                    }
                }

                // Quick presets + close
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: [
                            { label: "纯字悬浮", bg: 0.0, fg: 0.95 },
                            { label: "轻雾通透", bg: 0.55, fg: 0.92 },
                            { label: "稳重护眼", bg: 0.88, fg: 1.0 },
                            { label: "低存在感", bg: 0.35, fg: 0.55 }
                        ]
                        delegate: AbstractButton {
                            id: presetBtn
                            required property var modelData
                            implicitHeight: 24
                            leftPadding: 9
                            rightPadding: 9
                            hoverEnabled: true
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            onClicked: {
                                strip.backdropOpacity = modelData.bg
                                strip.contentOpacity = modelData.fg
                            }
                            background: Rectangle {
                                radius: 12
                                color: presetBtn.hovered ? "#2affffff" : "#14ffffff"
                                border.width: 1
                                border.color: "#1effffff"
                            }
                            contentItem: Label {
                                text: presetBtn.modelData.label
                                color: "#ddd6d2"
                                font.pixelSize: 11
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                    Item { Layout.fillWidth: true }
                    AbstractButton {
                        id: doneBtn
                        implicitHeight: 24
                        leftPadding: 10
                        rightPadding: 10
                        hoverEnabled: true
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        onClicked: settingsPopup.visible = false
                        background: Rectangle {
                            radius: 12
                            color: doneBtn.hovered ? "#33ffffff" : "#1effffff"
                        }
                        contentItem: Label {
                            text: "收起"
                            color: "#ffffff"
                            font.pixelSize: 11
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
    }
}
