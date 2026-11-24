pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Text tabs with a short red underline. Each tab is { key, label, count }.
RowLayout {
    id: strip
    property var tabs: []
    property string current
    property int textSize: 15
    signal chosen(string key)
    spacing: 22

    Repeater {
        model: strip.tabs
        delegate: AbstractButton {
            id: tab
            required property var modelData
            readonly property bool selected: strip.current === modelData.key
            implicitHeight: 34
            hoverEnabled: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            focusPolicy: Qt.TabFocus
            Accessible.name: modelData.label
            onClicked: strip.chosen(modelData.key)
            contentItem: RowLayout {
                spacing: 3
                Label {
                    text: tab.modelData.label
                    color: tab.selected ? Theme.text : tab.hovered ? Theme.text : Theme.secondary
                    font.pixelSize: strip.textSize
                    font.weight: tab.selected ? Font.Bold : Font.Normal
                }
                Label {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 3
                    visible: text.length > 0
                    text: tab.modelData.count !== undefined && tab.modelData.count !== "" ? String(tab.modelData.count) : ""
                    color: Theme.muted
                    font.pixelSize: 10
                }
            }
            background: Item {
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: 16
                    height: 3
                    radius: 1.5
                    color: Theme.accent
                    visible: tab.selected
                }
            }
        }
    }
    Item { Layout.fillWidth: true }
}
