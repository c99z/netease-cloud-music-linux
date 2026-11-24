import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    required property Library library
    property bool waitingForLogin: false
    anchors.centerIn: Overlay.overlay
    modal: true
    width: 350
    padding: 19
    closePolicy: Popup.CloseOnEscape
    background: Rectangle {
        radius: 14
        color: Theme.panel
        border.color: Theme.line
    }
    onOpened: {
        waitingForLogin = !library.loggedIn
        if (waitingForLogin)
            library.beginQrLogin()
    }
    onClosed: {
        waitingForLogin = false
        library.closeQrLogin()
    }
    Connections {
        target: dialog.library
        function onAccountChanged() {
            if (dialog.opened && dialog.waitingForLogin && dialog.library.loggedIn)
                dialog.close()
        }
    }

    contentItem: ColumnLayout {
        spacing: 15
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: dialog.library.loggedIn ? "网易云音乐账号" : "扫码登录"
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            IconButton {
                symbol: "close"
                text: "关闭"
                onClicked: dialog.close()
            }
        }
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 205
            Layout.preferredHeight: width
            radius: 10
            color: "white"
            visible: !dialog.library.loggedIn
            Image {
                anchors.fill: parent
                anchors.margins: 10
                source: dialog.library.loginImage
                fillMode: Image.PreserveAspectFit
                smooth: false
            }
            BusyIndicator {
                anchors.centerIn: parent
                running: dialog.library.loginBusy
                visible: running
                palette.highlight: Theme.ink
            }
            Symbol {
                anchors.centerIn: parent
                name: "user"
                tint: Theme.ink
                visible: !dialog.library.loginBusy && !dialog.library.loginImage.toString().length
            }
        }
        Cover {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 72
            Layout.preferredHeight: 72
            source: dialog.library.profile.avatarUrl || ""
            visible: dialog.library.loggedIn
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: dialog.library.loggedIn ? dialog.library.profile.nickname : dialog.library.loginStatus
            wrapMode: Text.Wrap
            color: Theme.text
            font.pixelSize: 13
        }
        Label {
            Layout.fillWidth: true
            visible: !dialog.library.loggedIn
            horizontalAlignment: Text.AlignHCenter
            text: "用手机端确认登录后，客户端会同步你的歌单。"
            wrapMode: Text.Wrap
            color: Theme.muted
            font.pixelSize: 11
        }
        Button {
            id: refreshButton
            Layout.alignment: Qt.AlignHCenter
            text: dialog.library.loggedIn ? "退出登录" : "刷新二维码"
            enabled: !dialog.library.loginBusy
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            onClicked: {
                if (dialog.library.loggedIn) {
                    dialog.library.logout()
                    dialog.close()
                } else {
                    dialog.library.beginQrLogin()
                }
            }
            palette.buttonText: Theme.text
            background: Rectangle {
                implicitWidth: 96
                implicitHeight: 34
                radius: 8
                color: Theme.interactive(Theme.raised, refreshButton.hovered, refreshButton.down)
                border.color: refreshButton.activeFocus ? Theme.accent : Theme.line
            }
        }
    }
}
