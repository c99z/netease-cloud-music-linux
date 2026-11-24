pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var shelves
    property string currentId
    property var skipKinds: []
    signal collectionChosen(var item)
    signal collectionPlayed(var item)
    signal sectionChosen(string section)
    signal tracksChosen(var tracks, int index)
    spacing: 26

    Repeater {
        model: root.shelves.filter(shelf => !root.skipKinds.includes(shelf.kind))
        delegate: ColumnLayout {
            id: shelf
            required property var modelData
            readonly property bool songs: modelData.kind === "tracks"
            readonly property string route: modelData.route || ""
            Layout.fillWidth: true
            spacing: 12

            SectionTitle {
                Layout.fillWidth: true
                text: shelf.modelData.title
                linked: !!shelf.route
                playable: shelf.songs
                onOpened: root.sectionChosen(shelf.route)
                onPlayed: root.tracksChosen(shelf.modelData.items, -1)
            }
            CollectionGrid {
                Layout.fillWidth: true
                visible: shelf.modelData.kind === "playlists"
                entries: visible ? shelf.modelData.items : []
                singleRow: true
                onChosen: (entry) => root.collectionChosen(entry)
                onPlayed: (entry) => root.collectionPlayed(entry)
            }
            RankingStrip {
                Layout.fillWidth: true
                visible: shelf.modelData.kind === "chartRows"
                charts: visible ? shelf.modelData.items : []
                onChosen: (entry) => root.collectionChosen(entry)
                onPlayed: (entry) => root.collectionPlayed(entry)
            }
            ChartTiles {
                Layout.fillWidth: true
                visible: shelf.modelData.kind === "chartTiles"
                charts: visible ? shelf.modelData.items : []
                onChosen: (entry) => root.collectionChosen(entry)
            }
            SongShelf {
                Layout.fillWidth: true
                visible: shelf.songs || shelf.modelData.kind === "programs"
                items: visible ? shelf.modelData.items : []
                songs: shelf.songs
                currentId: root.currentId
                onChosen: (index) => {
                    if (shelf.songs)
                        root.tracksChosen(shelf.modelData.items, index)
                    else
                        root.collectionChosen(shelf.modelData.items[index])
                }
            }
        }
    }
}
