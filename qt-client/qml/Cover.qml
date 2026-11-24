import QtQuick
import QtQuick.Window
import QtQuick.Effects

Item {
    id: root
    property url source
    property int edge: 6
    property bool round: false
    readonly property real corner: round ? Math.min(width, height) / 2 : edge

    Rectangle {
        id: plate
        anchors.fill: parent
        radius: root.corner
        color: Theme.placeholder
        Symbol {
            anchors.centerIn: parent
            name: "music"
            width: Math.min(parent.width * .4, 30)
            height: width
            tint: Theme.muted
            opacity: .55
            visible: picture.status !== Image.Ready
        }
    }
    // NetEase's image CDN resizes on request: ask for roughly the displayed size (x2 for
    // HiDPI) instead of the 400px default, so lists of small thumbnails load much faster.
    readonly property int wanted: {
        const edge = Math.max(width, height) * Math.max(1, Screen.devicePixelRatio) * 1.5
        return edge <= 80 ? 80 : edge <= 140 ? 140 : edge <= 240 ? 240 : edge <= 400 ? 400 : 600
    }
    readonly property url sized: {
        const text = String(source)
        if (!/^https?:\/\/p\d+\.music\.126\.net\//.test(text))
            return source
        return text.replace(/([?&])param=\d+y\d+/, "$1param=" + wanted + "y" + wanted)
    }
    Image {
        id: picture
        anchors.fill: parent
        source: root.width > 0 ? root.sized : ""
        sourceSize.width: Math.min(800, Math.max(96, Math.ceil(root.width * 2)))
        sourceSize.height: Math.min(800, Math.max(96, Math.ceil(root.height * 2)))
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        retainWhileLoading: true
        visible: false
    }
    Rectangle {
        id: shape
        anchors.fill: parent
        radius: root.corner
        visible: false
        layer.enabled: true
    }
    MultiEffect {
        anchors.fill: parent
        source: picture
        maskEnabled: true
        maskSource: shape
        visible: picture.status === Image.Ready
    }
}
