pragma Singleton
import QtQuick
import QtCore

QtObject {
    id: theme
    property bool dark: false
    readonly property Settings store: Settings {
        property alias darkTheme: theme.dark
    }

    readonly property color window: dark ? "#13131a" : "#f7f9fc"
    readonly property color rail: dark ? "#1a1a21" : "#f0f3f6"
    readonly property color bar: dark ? "#1b1b23" : "#fafafa"
    readonly property color panel: dark ? "#1f1f27" : "#ffffff"
    readonly property color raised: dark ? "#272730" : "#ffffff"
    readonly property color hover: dark ? "#2a2a34" : "#e9edf2"
    readonly property color line: dark ? "#2d2d38" : "#e4e8ed"
    readonly property color field: dark ? "#22222b" : "#ffffff"
    readonly property color placeholder: dark ? "#2a2a33" : "#e8ebf0"
    readonly property color bufferedAudio: dark ? "#55555f" : "#cfd4dc"
    readonly property color text: dark ? "#f0f0f3" : "#283248"
    readonly property color secondary: dark ? "#b4b4c0" : "#5c6679"
    readonly property color muted: dark ? "#82828f" : "#8f97a6"
    readonly property color accent: "#fc3d49"
    readonly property color accentHover: "#ff5a64"
    readonly property color accentPressed: "#e02f3b"
    readonly property color ink: "#ffffff"
    readonly property color accentSurface: dark ? "#3a1f26" : "#ffe8ea"
    readonly property color danger: "#fc3d49"
    readonly property color success: "#3fbf7f"
    readonly property color gold: "#e5a13a"
    readonly property color stage: "#2b2422"
    readonly property string font: "Noto Sans CJK SC"

    // Fill for anything clickable: the resting colour, washed lighter (on dark
    // surfaces) or darker (on light ones) while hovered or pressed. The wash is
    // translucent so it reads on any background, including the now-playing stage.
    function interactive(base, hovered: bool, pressed: bool, onDark): color {
        const darkSurface = onDark === undefined ? dark : onDark
        const clear = base === "transparent" || base.a === 0
        const strength = pressed ? (darkSurface ? .2 : .12) : hovered ? (darkSurface ? .13 : .07) : 0
        const wash = darkSurface ? Qt.rgba(1, 1, 1, strength) : Qt.rgba(0, 0, 0, strength)
        if (clear)
            return wash
        return strength > 0 ? Qt.tint(base, wash) : base
    }

    function time(ms: real): string {
        let seconds = Math.max(0, Math.floor(ms / 1000))
        return String(Math.floor(seconds / 60)).padStart(2, "0") + ":" + String(seconds % 60).padStart(2, "0")
    }

    function count(value: real): string {
        const n = Number(value) || 0
        if (n >= 100000000)
            return (n / 100000000).toFixed(1).replace(/\.0$/, "") + "亿"
        if (n >= 10000)
            return (n / 10000).toFixed(1).replace(/\.0$/, "") + "万"
        return String(n)
    }
}
