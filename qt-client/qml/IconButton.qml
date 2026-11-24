import QtQuick
import QtQuick.Controls

AbstractButton {
    id: control
    required property string symbol
    property bool featured: false
    property bool selected: false
    property bool filled: false
    property color tint: Theme.secondary
    property int iconSize: 18
    property string glyph            // short text instead of a symbol, e.g. "词"
    // Hover/press wash is a translucent overlay, so it shows on any background.
    // Set onDark for buttons drawn over a dark surface regardless of theme.
    property bool onDark: Theme.dark
    implicitWidth: featured ? 40 : 32
    implicitHeight: implicitWidth
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: text
    HoverHandler { cursorShape: Qt.PointingHandCursor }

    background: Rectangle {
        radius: control.featured ? width / 2 : 8
        color: {
            if (control.featured)
                return control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent
            return Theme.interactive("transparent", control.hovered || control.visualFocus,
                                     control.down, control.onDark)
        }
        Behavior on color { ColorAnimation { duration: 90 } }
        border.width: control.visualFocus ? 1 : 0
        border.color: Theme.accent
        opacity: control.enabled ? 1 : 0.45
    }
    contentItem: Item {
        Label {
            anchors.centerIn: parent
            visible: control.glyph.length > 0
            text: control.glyph
            color: control.featured ? Theme.ink : control.selected ? Theme.accent : control.tint
            opacity: control.enabled ? 1 : 0.35
            font.pixelSize: control.iconSize - 3
            font.weight: Font.DemiBold
        }
        Symbol {
            anchors.centerIn: parent
            visible: control.glyph.length === 0
            width: control.iconSize
            height: width
            name: control.symbol
            filled: control.filled
            tint: control.featured ? Theme.ink : control.selected ? Theme.accent : control.tint
            opacity: control.enabled ? 1 : 0.35
        }
    }
    ToolTip.visible: hovered && text.length > 0
    ToolTip.delay: 600
    ToolTip.text: text
}
