pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: rail
    required property Library library
    property string activeSection
    property string selectedPlaylist
    property bool showCreated: true
    property bool showSaved: true
    signal navigate(string key)
    signal playlistChosen(var playlist)
    signal loginRequested()
    signal createRequested()

    color: Theme.rail

    component NavRow: AbstractButton {
        id: nav
        required property string glyph
        property bool selected: false
        implicitHeight: 36
        hoverEnabled: true
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        property string section
        onHoveredChanged: if (hovered && section.length && typeof prefetcher !== "undefined") prefetcher.prefetchSection(section)
        focusPolicy: Qt.TabFocus
        Accessible.name: text
        background: Rectangle {
            radius: 8
            color: nav.selected ? Theme.accent : Theme.interactive("transparent", nav.hovered, nav.down)
            border.width: nav.visualFocus ? 1 : 0
            border.color: Theme.accent
        }
        contentItem: RowLayout {
            spacing: 9
            Item { Layout.preferredWidth: 4 }
            Symbol {
                Layout.preferredWidth: 17
                Layout.preferredHeight: 17
                name: nav.glyph
                filled: nav.glyph === "heart"
                tint: nav.selected ? "white" : Theme.secondary
            }
            Label {
                Layout.fillWidth: true
                text: nav.text
                color: nav.selected ? "white" : Theme.text
                font.pixelSize: 14
                font.weight: nav.selected ? Font.Medium : Font.Normal
                elide: Text.ElideRight
            }
        }
    }

    component PlaylistRow: AbstractButton {
        id: row
        required property var playlist
        property bool selected: false
        implicitHeight: 44
        hoverEnabled: true
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        Timer {
            id: warm
            interval: 180
            onTriggered: if (typeof prefetcher !== "undefined") prefetcher.prefetchCollection(row.playlist)
        }
        onHoveredChanged: hovered ? warm.restart() : warm.stop()
        focusPolicy: Qt.TabFocus
        Accessible.name: playlist.title
        background: Rectangle {
            radius: 8
            color: row.selected ? Theme.accent : Theme.interactive("transparent", row.hovered, row.down)
            border.width: row.visualFocus ? 1 : 0
            border.color: Theme.accent
        }
        contentItem: RowLayout {
            spacing: 8
            Item { Layout.preferredWidth: 2 }
            Cover {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                edge: 5
                source: row.playlist.art
            }
            Label {
                Layout.fillWidth: true
                text: row.playlist.title
                textFormat: Text.PlainText
                color: row.selected ? "white" : Theme.text
                font.pixelSize: 12
                lineHeight: .95
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
            Item { Layout.preferredWidth: 2 }
        }
    }

    component GroupHeader: RowLayout {
        id: group
        property string text
        property bool expanded: true
        property bool addable: false
        signal toggled()
        signal added()
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 0
        AbstractButton {
            id: toggle
            Layout.fillWidth: true
            implicitHeight: 30
            hoverEnabled: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            focusPolicy: Qt.TabFocus
            Accessible.name: group.text
            onClicked: group.toggled()
            background: Item {}
            contentItem: RowLayout {
                spacing: 4
                Item { Layout.preferredWidth: 4 }
                Label {
                    text: group.text
                    color: toggle.hovered ? Theme.text : Theme.muted
                    font.pixelSize: 12
                }
                Symbol {
                    Layout.preferredWidth: 11
                    Layout.preferredHeight: 11
                    name: group.expanded ? "up" : "down"
                    tint: Theme.muted
                }
                Item { Layout.fillWidth: true }
            }
        }
        IconButton {
            visible: group.addable
            symbol: "plus"
            text: "创建歌单"
            iconSize: 13
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            onClicked: group.added()
        }
    }

    component Divider: Rectangle {
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        Layout.topMargin: 10
        Layout.bottomMargin: 8
        Layout.preferredHeight: 1
        color: Theme.line
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 6
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            Layout.leftMargin: 6
            spacing: 8
            Image {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                source: "qrc:/branding/assets/netease-logo.png"
                fillMode: Image.PreserveAspectFit
                mipmap: true
            }
            Label {
                Layout.fillWidth: true
                text: "网易云音乐"
                color: Theme.text
                font.pixelSize: 18
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: scroll.availableWidth - 8
                spacing: 2
                Repeater {
                    model: [
                        {key:"recommend", title:"推荐", glyph:"home"},
                        {key:"essential", title:"精选", glyph:"collection"},
                        {key:"podcasts", title:"播客", glyph:"podcast"},
                        {key:"roam", title:"漫游", glyph:"disc"},
                        {key:"following", title:"关注", glyph:"comment"}
                    ]
                    delegate: NavRow {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.title
                        glyph: modelData.glyph
                        section: modelData.key
                        selected: rail.activeSection === modelData.key
                        onClicked: rail.navigate(modelData.key)
                    }
                }

                Divider {}

                Repeater {
                    model: [
                        {key:"liked", title:"我喜欢的音乐", glyph:"heart"},
                        {key:"recent", title:"最近播放", glyph:"history"},
                        {key:"myPodcasts", title:"我的播客", glyph:"podcast"},
                        {key:"collections", title:"我的收藏", glyph:"star"}
                    ]
                    delegate: NavRow {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.title
                        glyph: modelData.glyph
                        selected: rail.activeSection === modelData.key
                        onClicked: rail.navigate(modelData.key)
                    }
                }

                Divider {}

                GroupHeader {
                    text: "创建的歌单" + (rail.library.loggedIn ? " " + rail.library.createdPlaylists.length : "")
                    expanded: rail.showCreated
                    addable: true
                    onToggled: rail.showCreated = !rail.showCreated
                    onAdded: rail.library.loggedIn ? rail.createRequested() : rail.loginRequested()
                }
                Repeater {
                    model: rail.showCreated ? rail.library.createdPlaylists : []
                    delegate: PlaylistRow {
                        required property var modelData
                        Layout.fillWidth: true
                        playlist: modelData
                        selected: rail.selectedPlaylist === modelData.id
                        onClicked: rail.playlistChosen(modelData)
                    }
                }
                NavRow {
                    Layout.fillWidth: true
                    visible: rail.showCreated && !rail.library.loggedIn
                    glyph: "user"
                    text: "登录后同步歌单"
                    onClicked: rail.loginRequested()
                }

                Divider { visible: rail.library.savedPlaylists.length > 0 }
                GroupHeader {
                    visible: rail.library.savedPlaylists.length > 0
                    text: "收藏的歌单 " + rail.library.savedPlaylists.length
                    expanded: rail.showSaved
                    onToggled: rail.showSaved = !rail.showSaved
                }
                Repeater {
                    model: rail.showSaved ? rail.library.savedPlaylists : []
                    delegate: PlaylistRow {
                        required property var modelData
                        Layout.fillWidth: true
                        playlist: modelData
                        selected: rail.selectedPlaylist === modelData.id
                        onClicked: rail.playlistChosen(modelData)
                    }
                }
                Item { Layout.preferredHeight: 10 }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 7
            Layout.preferredHeight: 28
            visible: !rail.library.backendReady
            spacing: 7
            Rectangle {
                Layout.preferredWidth: 5
                Layout.preferredHeight: 5
                radius: 3
                color: Theme.muted
            }
            Label {
                Layout.fillWidth: true
                text: rail.library.backendStatus
                color: Theme.muted
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }
    }
}
