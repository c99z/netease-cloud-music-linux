import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string title
    property string detail
    property string actionText
    property bool busy: false
    property string symbol: "disc"
    signal action()
    spacing: 10

    Item { Layout.preferredHeight: 12 }
    Item {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 44
        Layout.preferredHeight: 44
        BusyIndicator {
            anchors.centerIn: parent
            width: 34
            height: 34
            running: root.busy
            visible: running
            palette.dark: Theme.accent
        }
        Symbol {
            anchors.centerIn: parent
            name: root.symbol
            tint: Theme.muted
            width: 32
            height: 32
            visible: !root.busy
        }
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        text: root.title
        color: Theme.secondary
        font.pixelSize: 14
    }
    Label {
        Layout.fillWidth: true
        Layout.maximumWidth: 420
        Layout.alignment: Qt.AlignHCenter
        horizontalAlignment: Text.AlignHCenter
        visible: text.length > 0
        text: root.detail
        color: Theme.muted
        wrapMode: Text.WordWrap
        font.pixelSize: 12
    }
    PillButton {
        Layout.alignment: Qt.AlignHCenter
        visible: root.actionText.length > 0
        text: root.actionText
        onClicked: root.action()
    }
    Item { Layout.preferredHeight: 12 }
}
