pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: resize
    required property Window hostWindow
    readonly property int borderSize: 5
    visible: hostWindow.visibility === Window.Windowed

    component EdgeHandle: MouseArea {
        id: handle
        required property int edge
        readonly property bool horizontal: edge === Qt.TopEdge || edge === Qt.BottomEdge
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: {
            const edges = edgesAt(mouseX)
            if (edges === Qt.LeftEdge || edges === Qt.RightEdge)
                return Qt.SizeHorCursor
            if (edges === Qt.TopEdge || edges === Qt.BottomEdge)
                return Qt.SizeVerCursor
            return edges === (Qt.TopEdge | Qt.LeftEdge) || edges === (Qt.BottomEdge | Qt.RightEdge)
                ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
        }

        function edgesAt(x: real): int {
            if (horizontal && x < resize.borderSize * 2)
                return edge | Qt.LeftEdge
            if (horizontal && x >= width - resize.borderSize * 2)
                return edge | Qt.RightEdge
            return edge
        }

        onPressed: (event) => resize.hostWindow.startSystemResize(edgesAt(event.x))
    }

    EdgeHandle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: resize.borderSize
        edge: Qt.TopEdge
    }
    EdgeHandle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: resize.borderSize
        edge: Qt.BottomEdge
    }
    EdgeHandle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: resize.borderSize
        anchors.bottomMargin: resize.borderSize
        width: resize.borderSize
        edge: Qt.LeftEdge
    }
    EdgeHandle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: resize.borderSize
        anchors.bottomMargin: resize.borderSize
        width: resize.borderSize
        edge: Qt.RightEdge
    }
}
