pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

ApplicationWindow {
    id: window
    required property Library library
    width: 1340
    height: 880
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint
    title: Qt.application.displayName
    color: Theme.window
    font.family: Theme.font
    palette.window: Theme.window
    palette.base: Theme.field
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.placeholderText: Theme.muted
    palette.buttonText: Theme.text
    palette.button: Theme.hover
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.ink
    palette.toolTipBase: Theme.panel
    palette.toolTipText: Theme.text
    // Closing the main window ends the app, even while the desktop lyric strip is still showing.
    onClosing: Qt.quit()

    readonly property int discovery: 0
    readonly property int search: 1
    readonly property int queue: 2
    readonly property int playlist: 3
    readonly property int browsePage: 4
    property int page: discovery
    property bool nowPlayingVisible: false
    property string toastText
    property var history: []
    readonly property string playingId: library.currentTrack.id || ""
    readonly property bool playing: library.media.playbackState === MediaPlayer.PlayingState

    function toggleMaximized() {
        if (window.visibility === Window.Maximized)
            window.showNormal()
        else
            window.showMaximized()
    }

    component TitleDragArea: Item {
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: window.toggleMaximized()
        }
        DragHandler {
            target: null
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (active)
                    window.startSystemMove()
            }
        }
    }

    // Navigation keeps a short history so the title bar's back button works like the web client's.
    function snapshot(): var {
        return { page: window.page, section: window.library.section, playlist: window.library.playlist,
                 term: window.library.searchTerm, searchType: window.library.searchType }
    }
    function remember() {
        const next = window.history.slice(-30)
        next.push(window.snapshot())
        window.history = next
    }
    function goBack() {
        if (window.nowPlayingVisible) {
            window.nowPlayingVisible = false
            return
        }
        if (!window.history.length)
            return
        const next = window.history.slice()
        const state = next.pop()
        window.history = next
        window.page = state.page
        if (state.page === window.browsePage && state.section)
            window.library.browse(state.section)
        else if (state.page === window.playlist && state.playlist && state.playlist.id
                 && state.playlist.id !== window.library.playlist.id)
            window.library.openPlaylist(state.playlist, false)
        else if (state.page === window.search && state.term && state.term !== window.library.searchTerm)
            window.library.search(state.term, state.searchType)
    }

    function searchFor(term: string) {
        const cleaned = term.trim()
        if (!cleaned.length)
            return
        searchInput.text = cleaned
        suggestions.close()
        pages.forceActiveFocus()
        window.nowPlayingVisible = false
        if (window.page !== window.search)
            window.remember()
        window.page = window.search
        window.library.search(cleaned, window.library.searchType === "1" || !window.library.searchTerm.length
                                       ? "1" : window.library.searchType)
    }

    function showQueue() {
        window.nowPlayingVisible = false
        if (window.page === window.queue) {
            window.goBack()
            return
        }
        window.remember()
        window.page = window.queue
    }

    function sidebarKey(key: string): string {
        if (["essential", "officialPlaylists", "essentialPodcasts", "featured", "charts",
            "artists", "newTracks", "albums", "vip", "vipCharts", "vipSongs"].includes(key))
            return "essential"
        if (key.startsWith("podcast"))
            return "podcasts"
        if (key.startsWith("recent"))
            return "recent"
        if (["myPodcasts", "createdVoices", "myRecentVoices", "savedVoices", "boughtVoices", "likedVoices"].includes(key))
            return "myPodcasts"
        if (["collections", "savedAlbums", "savedArtists", "savedVideos"].includes(key))
            return "collections"
        if (key.startsWith("following"))
            return "following"
        return key === "daily" || key.startsWith("scene") ? "recommend" : key
    }

    function showBrowse(key: string) {
        window.nowPlayingVisible = false
        if (window.page !== window.browsePage || window.library.section !== key)
            window.remember()
        window.page = window.browsePage
        window.library.browse(key)
    }

    function showPlaylist(data: var, playNow: bool) {
        window.nowPlayingVisible = false
        if (window.page !== window.playlist || window.library.playlist.id !== data.id)
            window.remember()
        window.page = window.playlist
        window.library.openPlaylist(data, playNow)
    }

    function navigate(key: string) {
        if (key === "recommend") {
            window.nowPlayingVisible = false
            if (window.page !== window.discovery)
                window.remember()
            window.page = window.discovery
        } else if (key === "roam") {
            window.startRoam()
        } else if (key === "liked" && window.library.likedPlaylist.id) {
            window.showPlaylist(window.library.likedPlaylist, false)
        } else if (key === "collections") {
            window.showBrowse("savedAlbums")
        } else {
            window.showBrowse(key)
        }
    }

    function startRoam() {
        if (!window.library.loggedIn) {
            login.open()
            return
        }
        window.library.startRoam()
        window.nowPlayingVisible = true
    }

    readonly property string activeSection: {
        if (window.nowPlayingVisible && window.library.roamPlaying)
            return "roam"
        if (window.page === window.discovery)
            return "recommend"
        if (window.page === window.playlist && window.library.likedPlaylist.id
                && window.library.playlist.id === window.library.likedPlaylist.id)
            return "liked"
        if (window.page === window.browsePage)
            return window.sidebarKey(window.library.section)
        return ""
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: {
            searchInput.forceActiveFocus()
            searchInput.selectAll()
        }
    }
    Shortcut {
        sequence: "Escape"
        enabled: window.nowPlayingVisible
        onActivated: window.nowPlayingVisible = false
    }
    Shortcut {
        sequence: "Alt+Left"
        onActivated: window.goBack()
    }
    Shortcut {
        sequence: "Ctrl+P"
        onActivated: window.library.togglePlayback()
    }

    Dialog {
        id: postDialog
        anchors.centerIn: Overlay.overlay
        width: 420
        modal: true
        standardButtons: Dialog.Close
        contentItem: ScrollView {
            implicitHeight: Math.min(postText.implicitHeight + 16, 320)
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            Label {
                id: postText
                width: 380
                wrapMode: Text.Wrap
                color: Theme.text
                textFormat: Text.PlainText
            }
        }
        background: Rectangle {
            color: Theme.panel
            radius: 10
            border.color: Theme.line
        }
    }

    LoginDialog {
        id: login
        library: window.library
    }

    Dialog {
        id: makePlaylist
        title: "新建歌单"
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 360
        padding: 20
        onOpened: {
            playlistName.text = ""
            playlistName.forceActiveFocus()
        }
        onAccepted: window.library.createPlaylist(playlistName.text)
        background: Rectangle {
            radius: 12
            color: Theme.panel
            border.color: Theme.line
        }
        header: Label {
            text: makePlaylist.title
            color: Theme.text
            font.pixelSize: 17
            font.weight: Font.Bold
            padding: 20
            bottomPadding: 0
        }
        contentItem: TextField {
            id: playlistName
            placeholderText: "输入歌单标题"
            maximumLength: 40
            color: Theme.text
            selectByMouse: true
            background: Rectangle {
                implicitHeight: 36
                radius: 8
                color: Theme.field
                border.color: playlistName.activeFocus ? Theme.accent : Theme.line
            }
            onAccepted: {
                if (text.trim().length)
                    makePlaylist.accept()
            }
        }
        footer: RowLayout {
            spacing: 10
            Item { Layout.fillWidth: true }
            PillButton {
                Layout.bottomMargin: 18
                text: "取消"
                onClicked: makePlaylist.reject()
            }
            PillButton {
                Layout.bottomMargin: 18
                Layout.rightMargin: 20
                primary: true
                text: "创建"
                enabled: playlistName.text.trim().length > 0
                onClicked: makePlaylist.accept()
            }
        }
    }

    Timer {
        id: toastTimer
        interval: 2600
        onTriggered: window.toastText = ""
    }

    Connections {
        target: window.library
        function onNotification(message) {
            window.toastText = message
            toastTimer.restart()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.bottomMargin: playerBar.height
        spacing: 0

        Sidebar {
            id: navigationSidebar
            Layout.fillHeight: true
            Layout.preferredWidth: 216
            library: window.library
            activeSection: window.activeSection
            selectedPlaylist: window.page === window.playlist && window.activeSection !== "liked"
                              ? window.library.playlist.id || "" : ""
            onLoginRequested: login.open()
            onCreateRequested: makePlaylist.open()
            onPlaylistChosen: (data) => window.showPlaylist(data, false)
            onNavigate: (key) => window.navigate(key)
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            RowLayout {
                id: titleBar
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                Layout.minimumHeight: 64
                Layout.maximumHeight: 64
                Layout.leftMargin: 30
                Layout.rightMargin: 12
                spacing: 12

                AbstractButton {
                    id: backButton
                    implicitWidth: 28
                    implicitHeight: 32
                    enabled: window.history.length > 0 || window.nowPlayingVisible
                    hoverEnabled: true
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    focusPolicy: Qt.TabFocus
                    Accessible.name: "返回"
                    onClicked: window.goBack()
                    background: Rectangle {
                        radius: 8
                        color: Theme.interactive("transparent", backButton.hovered, backButton.down)
                        border.color: Theme.line
                        opacity: backButton.enabled ? 1 : .55
                    }
                    contentItem: Item {
                        Symbol {
                            anchors.centerIn: parent
                            width: 15
                            height: 15
                            name: "left"
                            tint: backButton.enabled ? Theme.secondary : Theme.muted
                        }
                    }
                }

                TextField {
                    id: searchInput
                    objectName: "searchInput"
                    Layout.preferredWidth: 260
                    Layout.preferredHeight: 36
                    leftPadding: 34
                    rightPadding: text.length ? 30 : 12
                    placeholderText: window.library.hotSearches.length ? window.library.hotSearches[0] : "搜索"
                    selectByMouse: true
                    font.pixelSize: 13
                    color: Theme.text
                    placeholderTextColor: Theme.muted
                    Accessible.name: "搜索"
                    onAccepted: window.searchFor(text.length ? text : placeholderText)
                    function syncSuggestions() {
                        if (activeFocus && suggestionList.count > 0)
                            suggestions.open()
                        else
                            suggestions.close()
                    }
                    onTextEdited: {
                        suggestTimer.restart()
                        syncSuggestions()
                    }
                    onActiveFocusChanged: {
                        if (activeFocus)
                            window.library.loadHotSearches()
                        syncSuggestions()
                    }
                    Keys.onDownPressed: suggestionList.incrementCurrentIndex()
                    Keys.onUpPressed: suggestionList.decrementCurrentIndex()
                    Keys.onReturnPressed: (event) => {
                        if (suggestions.opened && suggestionList.currentIndex >= 0)
                            window.searchFor(suggestionList.model[suggestionList.currentIndex])
                        else
                            event.accepted = false
                    }
                    Keys.onEscapePressed: suggestions.close()
                    background: Rectangle {
                        radius: 10
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0; color: Theme.dark ? "#262632" : "#e9ecf6" }
                            GradientStop { position: 1; color: Theme.dark ? "#2c2430" : "#f6e9f0" }
                        }
                        border.color: searchInput.activeFocus ? Qt.alpha(Theme.accent, .6) : "transparent"
                    }
                    Symbol {
                        anchors.left: parent.left
                        anchors.leftMargin: 11
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        name: "search"
                        tint: Theme.muted
                    }
                    IconButton {
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24
                        height: 24
                        iconSize: 11
                        visible: searchInput.text.length > 0
                        symbol: "close"
                        text: "清空"
                        onClicked: {
                            searchInput.clear()
                            searchInput.forceActiveFocus()
                            window.library.suggest("")
                        }
                    }
                    Timer {
                        id: suggestTimer
                        interval: 220
                        onTriggered: window.library.suggest(searchInput.text)
                    }
                    Popup {
                        id: suggestions
                        y: searchInput.height + 6
                        width: 300
                        padding: 8
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                        background: Rectangle {
                            radius: 10
                            color: Theme.panel
                            border.color: Theme.line
                        }
                        contentItem: ColumnLayout {
                            spacing: 4
                            Label {
                                Layout.leftMargin: 8
                                Layout.topMargin: 2
                                text: searchInput.text.trim().length ? "猜你想搜" : "热搜榜"
                                color: Theme.muted
                                font.pixelSize: 12
                            }
                            ListView {
                                id: suggestionList
                                Layout.fillWidth: true
                                Layout.preferredHeight: contentHeight
                                interactive: false
                                currentIndex: -1
                                model: searchInput.text.trim().length ? window.library.searchSuggestions
                                                                      : window.library.hotSearches
                                onModelChanged: currentIndex = -1
                                onCountChanged: Qt.callLater(searchInput.syncSuggestions)
                                delegate: AbstractButton {
                                    id: word
                                    required property int index
                                    required property string modelData
                                    width: suggestionList.width
                                    height: 32
                                    hoverEnabled: true
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    onClicked: window.searchFor(modelData)
                                    background: Rectangle {
                                        radius: 6
                                        color: Theme.interactive("transparent", word.hovered
                                                                 || suggestionList.currentIndex === word.index, word.down)
                                    }
                                    contentItem: RowLayout {
                                        spacing: 8
                                        Label {
                                            Layout.leftMargin: 6
                                            Layout.preferredWidth: 14
                                            visible: !searchInput.text.trim().length
                                            text: word.index + 1
                                            color: word.index < 3 ? Theme.accent : Theme.muted
                                            font.pixelSize: 12
                                            font.weight: Font.Bold
                                        }
                                        Symbol {
                                            Layout.leftMargin: 6
                                            Layout.preferredWidth: 13
                                            Layout.preferredHeight: 13
                                            visible: !!searchInput.text.trim().length
                                            name: "search"
                                            tint: Theme.muted
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: word.modelData
                                            textFormat: Text.PlainText
                                            color: Theme.text
                                            font.pixelSize: 13
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                TitleDragArea {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: 24
                }

                AbstractButton {
                    id: accountButton
                    Layout.preferredHeight: 34
                    Layout.maximumWidth: 200
                    enabled: window.library.backendReady && window.library.localAccountSupported
                    hoverEnabled: true
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    focusPolicy: Qt.TabFocus
                    Accessible.name: window.library.loggedIn ? window.library.profile.nickname : "未登录"
                    onClicked: window.library.loggedIn ? accountMenu.open() : login.open()
                    background: Rectangle {
                        radius: 17
                        color: Theme.interactive("transparent", accountButton.hovered, accountButton.down)
                    }
                    contentItem: RowLayout {
                        spacing: 7
                        Cover {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            round: true
                            source: window.library.profile.avatarUrl || ""
                        }
                        Label {
                            Layout.fillWidth: true
                            text: window.library.loggedIn ? window.library.profile.nickname : "未登录"
                            textFormat: Text.PlainText
                            color: Theme.secondary
                            elide: Text.ElideRight
                            font.pixelSize: 13
                        }
                        Symbol {
                            Layout.preferredWidth: 12
                            Layout.preferredHeight: 12
                            Layout.rightMargin: 4
                            name: "down"
                            tint: Theme.muted
                        }
                    }
                    Popup {
                        id: accountMenu
                        y: parent.height + 8
                        x: parent.width - width
                        width: 220
                        padding: 8
                        background: Rectangle {
                            radius: 10
                            color: Theme.panel
                            border.color: Theme.line
                        }
                        contentItem: ColumnLayout {
                            spacing: 2
                            RowLayout {
                                Layout.margins: 8
                                spacing: 10
                                Cover {
                                    Layout.preferredWidth: 40
                                    Layout.preferredHeight: 40
                                    round: true
                                    source: window.library.profile.avatarUrl || ""
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: window.library.profile.nickname || ""
                                    textFormat: Text.PlainText
                                    color: Theme.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                            }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                            Repeater {
                                model: [
                                    { key: "page", label: "我的主页" },
                                    { key: "theme", label: Theme.dark ? "切换到浅色" : "切换到深色" },
                                    { key: "logout", label: "退出登录" }
                                ]
                                delegate: AbstractButton {
                                    id: action
                                    required property var modelData
                                    Layout.fillWidth: true
                                    implicitHeight: 34
                                    hoverEnabled: true
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    onClicked: {
                                        accountMenu.close()
                                        if (modelData.key === "page")
                                            Qt.openUrlExternally("https://music.163.com/#/user/home?id="
                                                                 + window.library.profile.userId)
                                        else if (modelData.key === "theme")
                                            Theme.dark = !Theme.dark
                                        else
                                            window.library.logout()
                                    }
                                    background: Rectangle {
                                        radius: 6
                                        color: Theme.interactive("transparent", action.hovered, action.down)
                                    }
                                    contentItem: Label {
                                        leftPadding: 8
                                        text: action.modelData.label
                                        color: Theme.text
                                        font.pixelSize: 13
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }
                }
                IconButton {
                    symbol: "skin"
                    text: Theme.dark ? "浅色皮肤" : "深色皮肤"
                    iconSize: 17
                    onClicked: Theme.dark = !Theme.dark
                }
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 16
                    color: Theme.line
                }
                RowLayout {
                    spacing: 0
                    IconButton {
                        symbol: "minimize"
                        text: "最小化"
                        iconSize: 16
                        onClicked: window.showMinimized()
                    }
                    IconButton {
                        symbol: window.visibility === Window.Maximized ? "restore" : "maximize"
                        text: window.visibility === Window.Maximized ? "还原" : "最大化"
                        iconSize: 15
                        onClicked: window.toggleMaximized()
                    }
                    IconButton {
                        symbol: "close"
                        text: "关闭"
                        iconSize: 15
                        onClicked: window.close()
                    }
                }
            }

            StackLayout {
                id: pages
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.page
                Layout.leftMargin: currentIndex === window.discovery ? 0 : 40
                Layout.rightMargin: currentIndex === window.discovery ? 0 : 26

                DiscoverPage {
                    library: window.library
                    onChoosePlaylist: (playlistData, playNow) => window.showPlaylist(playlistData, playNow)
                    onNavigate: (key) => window.showBrowse(key)
                    onRoamRequested: window.startRoam()
                }

                SearchPage {
                    library: window.library
                    onCollectionChosen: (data) => window.showPlaylist(data, false)
                    onLoginRequested: login.open()
                }

                ColumnLayout {
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.rightMargin: 14
                        Label {
                            text: "播放列表"
                            color: Theme.text
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignBottom
                            Layout.bottomMargin: 3
                            text: "共 " + window.library.queue.length + " 首"
                            color: Theme.muted
                            font.pixelSize: 12
                        }
                        PillButton {
                            text: "清空列表"
                            visible: window.library.queue.length > 1
                            onClicked: window.library.clearQueue()
                        }
                    }
                    StatusView {
                        Layout.fillWidth: true
                        Layout.topMargin: 42
                        visible: window.library.queue.length === 0
                        symbol: "queue"
                        title: "你还没有添加任何歌曲"
                        detail: "去首页发现音乐，或者在歌单里双击一首歌。"
                    }
                    TrackList {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        library: window.library
                        queueMode: true
                        tracks: window.library.queue
                        visible: tracks.length > 0
                        currentId: window.playingId
                        playing: window.playing
                        onActivated: (index) => window.library.playIndex(index)
                        onLoginRequested: login.open()
                        onArtistChosen: (data) => window.showPlaylist(data, false)
                        onAlbumChosen: (data) => window.showPlaylist(data, false)
                    }
                    Item { Layout.fillHeight: true; visible: window.library.queue.length === 0 }
                }

                PlaylistPage {
                    library: window.library
                    onLoginRequested: login.open()
                    onCollectionChosen: (data) => window.showPlaylist(data, false)
                }

                BrowsePage {
                    library: window.library
                    onLoginRequested: login.open()
                    onPlaylistChosen: (data) => window.showPlaylist(data, false)
                    onPlaylistPlayed: (data) => window.showPlaylist(data, true)
                    onPostChosen: (author, message) => {
                        postDialog.title = author
                        postText.text = message
                        postDialog.open()
                    }
                }
            }
        }
    }

    NowPlayingPage {
        id: nowPlaying
        x: 0
        width: parent.width
        height: parent.height
        y: window.nowPlayingVisible ? 0 : parent.height
        visible: y < parent.height
        Behavior on y { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
        library: window.library
        onCloseRequested: window.nowPlayingVisible = false
        onCollectionChosen: (data) => window.showPlaylist(data, false)
        onLoginRequested: login.open()
    }

    PlayerBar {
        id: playerBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        library: window.library
        stage: window.nowPlayingVisible
        queueVisible: !window.nowPlayingVisible && window.page === window.queue
        onQueueRequested: window.showQueue()
        onNowPlayingRequested: window.nowPlayingVisible = !window.nowPlayingVisible
        onLoginRequested: login.open()
        onCollectionChosen: (data) => window.showPlaylist(data, false)
        desktopLyricsShown: desktopLyrics.shown
        desktopLyricsLocked: desktopLyrics.locked
        desktopLyricsStyle: desktopLyrics.styleMode
        onDesktopLyricsRequested: {
            // Locked lyrics ignore the mouse, so the player bar button is how they get unlocked.
            if (desktopLyrics.shown && desktopLyrics.locked) {
                desktopLyrics.locked = false
                window.toastText = "桌面歌词已解锁"
            } else {
                desktopLyrics.shown = !desktopLyrics.shown
                window.toastText = desktopLyrics.shown ? "已打开桌面歌词" : "已关闭桌面歌词"
            }
            toastTimer.restart()
        }
        onDesktopLyricsStyleSelected: (styleKey) => {
            desktopLyrics.styleMode = styleKey
            if (!desktopLyrics.shown)
                desktopLyrics.shown = true
            window.toastText = "桌面歌词已切换为「" + desktopLyrics.styleLabel(styleKey) + "」"
            toastTimer.restart()
        }
    }

    DesktopLyrics {
        id: desktopLyrics
        library: window.library
        onLockedByUser: {
            window.toastText = "桌面歌词已锁定，点击播放栏的「词」解锁"
            toastTimer.restart()
        }
    }

    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        visible: window.toastText.length > 0
        text: window.toastText
        color: "white"
        font.pixelSize: 13
        leftPadding: 18
        rightPadding: 18
        topPadding: 10
        bottomPadding: 10
        background: Rectangle { radius: 20; color: "#e6202028" }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: playerBar.top
        height: 34
        visible: window.library.playbackError.length > 0 && !window.nowPlayingVisible
        color: Theme.accentSurface
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 238
            anchors.rightMargin: 22
            Label {
                Layout.fillWidth: true
                text: window.library.playbackError
                color: Theme.accent
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            PillButton {
                implicitHeight: 24
                text: "切换音源"
                visible: window.library.queueIndex >= 0 && !window.library.playbackBusy
                onClicked: window.library.switchAudioStream()
            }
        }
    }

    TitleDragArea {
        anchors.left: parent.left
        anchors.top: parent.top
        width: navigationSidebar.width
        height: 60
        visible: !window.nowPlayingVisible
    }

    WindowResize {
        anchors.fill: parent
        hostWindow: window
    }
}
