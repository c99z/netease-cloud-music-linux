import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The web client's rounded buttons: a red primary, a grey secondary and filter chips.
AbstractButton {
    id: control
    property bool primary: false
    property bool selected: false
    property bool chip: false
    property string symbol
    property bool filledSymbol: true
    implicitHeight: chip ? 28 : 32
    leftPadding: chip ? 16 : symbol.length ? 12 : 16
    rightPadding: chip ? 16 : 16
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: text
    opacity: enabled ? 1 : .5
    HoverHandler { cursorShape: Qt.PointingHandCursor }

    background: Rectangle {
        radius: control.chip ? height / 2 : 8
        color: control.primary ? (control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent)
             : Theme.interactive(control.selected ? Theme.accentSurface : control.chip ? Theme.panel : Theme.hover,
                                 control.hovered, control.down)
        border.width: control.chip || control.visualFocus ? 1 : 0
        Behavior on color { ColorAnimation { duration: 90 } }
        border.color: control.visualFocus ? Theme.accent : control.selected ? Theme.accentSurface : Theme.line
    }
    contentItem: RowLayout {
        spacing: 5
        Symbol {
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            visible: control.symbol.length > 0
            name: control.symbol
            filled: control.filledSymbol
            tint: label.color
        }
        Label {
            id: label
            text: control.text
            color: control.primary ? Theme.ink : control.selected ? Theme.accent : Theme.text
            font.pixelSize: control.chip ? 12 : 13
            font.weight: control.primary ? Font.Medium : Font.Normal
            verticalAlignment: Text.AlignVCenter
        }
    }
}
