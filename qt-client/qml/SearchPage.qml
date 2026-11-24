pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

ColumnLayout {
    id: page
    required property Library library
    signal collectionChosen(var data)
    signal loginRequested()
    readonly property bool songs: library.searchType === "1"
    readonly property int count: songs ? library.searchResults.length : library.searchItems.length
    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Label {
            text: page.library.searchTerm
            textFormat: Text.PlainText
            color: Theme.text
            font.pixelSize: 20
            font.weight: Font.Bold
        }
        Label {
            Layout.alignment: Qt.AlignBottom
            Layout.bottomMargin: 3
            text: "的相关搜索如下"
            color: Theme.muted
            font.pixelSize: 12
        }
        Item { Layout.fillWidth: true }
        PillButton {
            visible: page.songs && page.library.searchResults.length > 0
            primary: true
            symbol: "play"
            text: "播放全部"
            onClicked: page.library.playAll(page.library.searchResults)
        }
    }
    TabStrip {
        Layout.fillWidth: true
        tabs: [
            { key: "1", label: "单曲" }, { key: "1000", label: "歌单" }, { key: "100", label: "歌手" },
            { key: "10", label: "专辑" }, { key: "1009", label: "播客" }
        ]
        current: page.library.searchType
        onChosen: (key) => page.library.search(page.library.searchTerm, key)
    }
    StatusView {
        Layout.fillWidth: true
        Layout.topMargin: 38
        symbol: "search"
        visible: page.library.searchBusy || page.count === 0
        busy: page.library.searchBusy
        title: busy ? "正在搜索…" : page.library.searchError.length ? "搜索暂不可用"
              : page.library.searchTerm.length ? "没有找到相关结果" : "输入关键词开始搜索"
        detail: page.library.searchError
        actionText: page.library.searchError.length ? "重试" : ""
        onAction: page.library.search(page.library.searchTerm, page.library.searchType)
    }
    TrackList {
        objectName: "searchResults"
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: page.songs && tracks.length > 0
        library: page.library
        tracks: page.library.searchResults
        currentId: page.library.currentTrack.id || ""
        playing: page.library.media.playbackState === MediaPlayer.PlayingState
        onActivated: (index) => page.library.playCollection(tracks, index)
        onLoginRequested: page.loginRequested()
        onArtistChosen: (data) => page.collectionChosen(data)
        onAlbumChosen: (data) => page.collectionChosen(data)
    }
    CollectionGrid {
        id: cards
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.rightMargin: 14
        visible: !page.songs && page.library.searchItems.length > 0
        lazy: true
        singleRow: false
        look: page.library.searchType === "100" ? "round" : page.library.searchType === "1000" ? "poster" : "plain"
        entries: page.songs ? [] : page.library.searchItems
        onChosen: (data) => page.collectionChosen(data)
        onPlayed: (data) => page.collectionChosen(data)
    }
    Item { Layout.fillHeight: true; visible: page.count === 0 }
}
