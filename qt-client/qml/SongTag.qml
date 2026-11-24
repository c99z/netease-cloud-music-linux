import QtQuick
import QtQuick.Controls

Rectangle {
    id: tag
    required property string label
    property color tone: Theme.accent
    implicitWidth: caption.implicitWidth + 6
    implicitHeight: 14
    radius: 3
    color: "transparent"
    border.color: Qt.alpha(tone, .55)
    Label {
        id: caption
        anchors.centerIn: parent
        text: tag.label
        color: tag.tone
        font.pixelSize: 9
    }
}
