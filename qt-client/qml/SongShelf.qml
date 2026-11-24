pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Compact song or program rows laid out in columns, as on the web home page.
GridLayout {
    id: shelf
    required property var items
    property bool songs: true
    property string currentId
    signal chosen(int index)
    columns: width >= 980 ? 3 : width >= 620 ? 2 : 1
    rowSpacing: 4
    columnSpacing: 20
    flow: GridLayout.TopToBottom
    rows: Math.max(1, Math.ceil(items.length / columns))

    Repeater {
        model: shelf.items
        delegate: AbstractButton {
            id: row
            required property int index
            required property var modelData
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.preferredHeight: 62
            padding: 6
            hoverEnabled: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            focusPolicy: Qt.TabFocus
            Accessible.name: modelData.title
            onClicked: shelf.chosen(index)
            background: Rectangle {
                radius: 8
                color: row.hovered ? Theme.panel : "transparent"
                border.color: row.visualFocus ? Theme.accent : row.hovered && !Theme.dark ? Theme.line : "transparent"
            }
            contentItem: RowLayout {
                spacing: 10
                Item {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    Cover {
                        anchors.fill: parent
                        edge: 6
                        source: row.modelData.art || ""
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: "#55000000"
                        visible: row.hovered
                        Symbol {
                            anchors.centerIn: parent
                            width: 18
                            height: 18
                            name: "play"
                            tint: "white"
                        }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Label {
                        Layout.fillWidth: true
                        text: row.modelData.title
                        textFormat: Text.PlainText
                        color: row.modelData.id === shelf.currentId ? Theme.accent : Theme.text
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        SongTag {
                            label: row.modelData.remark || ""
                            visible: shelf.songs && label.length > 0 && row.width > 260
                        }
                        SongTag { label: "SQ"; tone: Theme.gold; visible: shelf.songs && row.modelData.sq === true }
                        SongTag { label: "VIP"; visible: shelf.songs && row.modelData.vip === true }
                        SongTag { label: "原唱"; visible: shelf.songs && row.modelData.original === true }
                        SongTag {
                            label: row.modelData.label || ""
                            tone: "#5b7fc7"
                            visible: !shelf.songs && label.length > 0
                        }
                        Label {
                            Layout.fillWidth: true
                            text: shelf.songs ? row.modelData.artist || "" : row.modelData.description || ""
                            textFormat: Text.PlainText
                            color: Theme.muted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }
}
