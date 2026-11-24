pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

// Cover cards. "poster" cards carry their title on a tinted band like the web client's
// playlist tiles; plain cards put the title under the cover; "round" is for artists.
Item {
    id: grid
    required property var entries
    property bool singleRow: true
    property int minimumColumnWidth: 168
    property int maximumColumns: 8
    property string look: "poster"     // poster | plain | round
    property bool showPlays: true
    signal chosen(var entry)
    signal played(var entry)
    readonly property int columnSpacing: 16
    readonly property int rowSpacing: look === "poster" ? 16 : 20
    readonly property int columns: Math.max(2, Math.min(maximumColumns, Math.floor((width + columnSpacing) / minimumColumnWidth)))
    readonly property real cell: Math.max(0, Math.floor((width - (columns - 1) * columnSpacing) / columns))
    readonly property real cardHeight: look === "poster" ? cell + 44 : look === "round" ? cell * .78 + 52 : cell + 48
    // lazy: the grid fills its parent and scrolls by itself, creating cards on demand.
    property bool lazy: false
    property Component header: null
    readonly property alias view: view
    implicitHeight: lazy ? 0 : cells.height
    implicitWidth: 600

    component Card: AbstractButton {
        id: card
        required property var modelData
        property int index
        readonly property bool poster: grid.look === "poster"
        readonly property bool round: grid.look === "round"
        readonly property string caption: modelData.description || modelData.creatorName
            || (modelData.trackCount ? modelData.trackCount + (modelData.kind === "radio" ? " 期节目" : " 首") : "")
        width: grid.cell
        height: grid.cardHeight
        padding: 0
        hoverEnabled: true
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        focusPolicy: Qt.TabFocus
        Accessible.name: modelData.title || ""
        onClicked: grid.chosen(modelData)
        Timer {
            id: warm
            interval: 180
            onTriggered: if (typeof prefetcher !== "undefined") prefetcher.prefetchCollection(card.modelData)
        }
        onHoveredChanged: hovered ? warm.restart() : warm.stop()
        background: Rectangle {
            radius: 10
            color: card.poster ? Theme.stage : "transparent"
            border.width: card.visualFocus ? 2 : 0
            border.color: Theme.accent
            clip: true
        }
        contentItem: Item {
            Item {
                id: frame
                width: card.round ? parent.width * .78 : parent.width
                height: width
                anchors.horizontalCenter: parent.horizontalCenter
                Cover {
                    anchors.fill: parent
                    round: card.round
                    edge: 10
                    source: card.modelData.art || ""
                }
                Rectangle {
                    id: art
                    anchors.fill: parent
                    radius: 10
                    visible: !!card.modelData.glyph
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.lighter(card.modelData.tone || Theme.accent, 1.25) }
                        GradientStop { position: 1; color: card.modelData.tone || Theme.accent }
                    }
                    Symbol {
                        anchors.centerIn: parent
                        width: parent.width * .34
                        height: width
                        name: card.modelData.glyph || "music"
                        tint: "#f2ffffff"
                        visible: !card.modelData.badge
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: !!card.modelData.badge
                        text: card.modelData.badge || ""
                        color: "white"
                        font.pixelSize: parent.width * .36
                        font.weight: Font.Bold
                    }
                    Label {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: 10
                        text: card.modelData.heading || ""
                        color: "white"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                    }
                }
                Rectangle {
                    visible: card.poster
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 12
                    color: band.color
                }
                Row {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 3
                    visible: grid.showPlays && !card.round && Number(card.modelData.playCount) > 0
                    Symbol {
                        width: 11
                        height: 11
                        anchors.verticalCenter: parent.verticalCenter
                        name: "headphones"
                        tint: "white"
                    }
                    Label {
                        text: Theme.count(card.modelData.playCount)
                        color: "white"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        style: Text.Raised
                        styleColor: "#66000000"
                    }
                }
                Rectangle {
                    visible: (card.hovered || playButton.hovered) && !card.round && card.modelData.kind !== "sceneCategory"
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 10
                    width: 36
                    height: 36
                    radius: 18
                    color: "#f2ffffff"
                    Symbol {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: 1
                        width: 18
                        height: 18
                        name: "play"
                        tint: Theme.accent
                    }
                    HoverHandler { id: playButton; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: grid.played(card.modelData) }
                }
            }
            Rectangle {
                id: band
                visible: card.poster
                anchors.top: frame.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                radius: 10
                color: card.modelData.glyph ? Qt.darker(card.modelData.tone || Theme.accent, 1.5) : Theme.dark ? "#33333d" : "#6f7480"
                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 9
                    anchors.rightMargin: 9
                    anchors.bottomMargin: 4
                    text: card.modelData.title || ""
                    textFormat: Text.PlainText
                    color: "white"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }
            Column {
                visible: !card.poster
                anchors.top: frame.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 2
                Label {
                    width: parent.width
                    text: card.modelData.title || ""
                    textFormat: Text.PlainText
                    color: card.hovered ? Theme.accent : Theme.text
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    horizontalAlignment: card.round ? Text.AlignHCenter : Text.AlignLeft
                }
                Label {
                    width: parent.width
                    text: card.caption
                    textFormat: Text.PlainText
                    visible: text.length > 0
                    color: Theme.muted
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    horizontalAlignment: card.round ? Text.AlignHCenter : Text.AlignLeft
                }
            }
        }
    }

    // Small shelves (home page rows) build their few cards directly.
    Grid {
        id: cells
        visible: !grid.lazy
        width: grid.width
        columns: grid.columns
        columnSpacing: grid.columnSpacing
        rowSpacing: grid.rowSpacing
        Repeater {
            model: grid.lazy ? [] : grid.singleRow ? grid.entries.slice(0, grid.columns) : grid.entries
            delegate: Card {}
        }
    }

    // Whole pages of cards create only the rows in view (plus a small buffer).
    GridView {
        id: view
        visible: grid.lazy
        anchors.fill: parent
        clip: true
        rightMargin: -grid.columnSpacing
        cellWidth: grid.cell + grid.columnSpacing
        cellHeight: grid.cardHeight + grid.rowSpacing
        cacheBuffer: Math.round(cellHeight * 2)
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        model: grid.lazy ? grid.entries : []
        header: grid.header
        ScrollBar.vertical: ScrollBar {}
        delegate: Card {}
    }
}
