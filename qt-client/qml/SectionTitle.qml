import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    property string text
    property bool linked: false
    property bool playable: false
    signal opened()
    signal played()
    spacing: 8

    AbstractButton {
        id: link
        enabled: root.linked
        hoverEnabled: true
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        focusPolicy: root.linked ? Qt.TabFocus : Qt.NoFocus
        Accessible.name: root.text
        onClicked: root.opened()
        contentItem: RowLayout {
            spacing: 2
            Label {
                text: root.text
                textFormat: Text.PlainText
                color: link.hovered && root.linked ? Theme.accent : Theme.text
                font.pixelSize: 18
                font.weight: Font.Medium
            }
            Symbol {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                visible: root.linked
                name: "chevron"
                tint: link.hovered ? Theme.accent : Theme.text
            }
        }
        background: Item {}
    }
    PillButton {
        visible: root.playable
        implicitHeight: 24
        leftPadding: 8
        rightPadding: 10
        symbol: "play"
        text: "播放"
        onClicked: root.played()
    }
    Item { Layout.fillWidth: true }
}
