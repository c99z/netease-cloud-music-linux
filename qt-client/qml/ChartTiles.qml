pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Square cover tiles for the charts that have no top-three preview.
Item {
    id: tiles
    required property var charts
    signal chosen(var chart)
    readonly property int columns: Math.max(3, Math.min(8, Math.floor((width + 16) / 150)))
    readonly property real cell: Math.max(0, Math.floor((width - (columns - 1) * 16) / columns))
    implicitHeight: cells.height
    implicitWidth: 600
    Grid {
        id: cells
        width: tiles.width
        columns: tiles.columns
        spacing: 16
    Repeater {
        model: tiles.charts
        delegate: AbstractButton {
            id: tile
            required property var modelData
            width: tiles.cell
            height: tiles.cell
            hoverEnabled: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            focusPolicy: Qt.TabFocus
            Accessible.name: modelData.title
            onClicked: tiles.chosen(modelData)
            Timer {
                id: warm
                interval: 180
                onTriggered: if (typeof prefetcher !== "undefined") prefetcher.prefetchCollection(tile.modelData)
            }
            onHoveredChanged: hovered ? warm.restart() : warm.stop()
            background: Rectangle {
                radius: 10
                color: "transparent"
                border.width: tile.visualFocus ? 2 : 0
                border.color: Theme.accent
            }
            contentItem: Cover {
                edge: 10
                source: tile.modelData.tile || tile.modelData.art || ""
                opacity: tile.hovered ? .88 : 1
            }
        }
    }
}
}
