pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Chart cards listing the top three songs with their movement.
GridLayout {
    id: strip
    required property var charts
    signal chosen(var chart)
    signal played(var chart)
    columns: width >= 880 ? 3 : width >= 560 ? 2 : 1
    columnSpacing: 16
    rowSpacing: 16

    Repeater {
        model: strip.charts
        delegate: AbstractButton {
            id: card
            required property var modelData
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.preferredHeight: 128
            padding: 14
            hoverEnabled: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            focusPolicy: Qt.TabFocus
            Accessible.name: modelData.title
            onClicked: strip.chosen(modelData)
            Timer {
                id: warm
                interval: 180
                onTriggered: if (typeof prefetcher !== "undefined") prefetcher.prefetchCollection(card.modelData)
            }
            onHoveredChanged: hovered ? warm.restart() : warm.stop()
            background: Rectangle {
                radius: 10
                color: Theme.panel
                border.color: card.visualFocus ? Theme.accent : card.hovered ? Theme.line : "transparent"
            }
            contentItem: ColumnLayout {
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: card.modelData.title
                        color: Theme.text
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }
                    Label {
                        text: card.modelData.description || ""
                        color: Theme.muted
                        font.pixelSize: 10
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    Item {
                        Layout.preferredWidth: 66
                        Layout.preferredHeight: 66
                        Cover {
                            anchors.fill: parent
                            edge: 6
                            source: card.modelData.art || ""
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 30
                            height: 30
                            radius: 15
                            color: "#f2ffffff"
                            visible: card.hovered
                            Symbol {
                                anchors.centerIn: parent
                                anchors.horizontalCenterOffset: 1
                                width: 15
                                height: 15
                                name: "play"
                                tint: Theme.accent
                            }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: strip.played(card.modelData) }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        Repeater {
                            model: card.modelData.preview
                            delegate: RowLayout {
                                id: rank
                                required property int index
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 8
                                Label {
                                    Layout.preferredWidth: 8
                                    text: rank.index + 1
                                    color: Theme.text
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
                                }
                                Label {
                                    Layout.fillWidth: true
                                    textFormat: Text.StyledText
                                    text: "<font color='" + Theme.text + "'>" + String(rank.modelData.title).replace(/&/g, "&amp;").replace(/</g, "&lt;")
                                          + "</font>" + (rank.modelData.artist ? " - " + String(rank.modelData.artist).replace(/&/g, "&amp;").replace(/</g, "&lt;") : "")
                                    elide: Text.ElideRight
                                    color: Theme.muted
                                    font.pixelSize: 12
                                }
                                Item {
                                    Layout.preferredWidth: 14
                                    Layout.preferredHeight: 14
                                    Label {
                                        anchors.centerIn: parent
                                        visible: rank.modelData.trend === "new" || rank.modelData.trend === "flat"
                                        text: rank.modelData.trend === "new" ? "新" : "–"
                                        color: rank.modelData.trend === "new" ? Theme.success : Theme.muted
                                        font.pixelSize: 11
                                    }
                                    Symbol {
                                        anchors.centerIn: parent
                                        width: 9
                                        height: 9
                                        visible: rank.modelData.trend === "up" || rank.modelData.trend === "down"
                                        name: rank.modelData.trend === "up" ? "trendUp" : "trendDown"
                                        tint: rank.modelData.trend === "up" ? Theme.accent : Theme.success
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
