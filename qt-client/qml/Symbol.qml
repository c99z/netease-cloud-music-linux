import QtQuick

Canvas {
    id: symbol
    property string name: "disc"
    property color tint: Theme.secondary
    property bool filled: false
    onFilledChanged: requestPaint()
    implicitWidth: 20
    implicitHeight: 20
    onNameChanged: requestPaint()
    onTintChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const c = getContext("2d")
        c.reset()
        c.clearRect(0, 0, width, height)
        c.scale(width / 24, height / 24)
        c.strokeStyle = tint
        c.fillStyle = tint
        c.lineWidth = 1.65
        c.lineJoin = "round"
        c.lineCap = "round"

        function line(x1, y1, x2, y2) {
            c.beginPath()
            c.moveTo(x1, y1)
            c.lineTo(x2, y2)
            c.stroke()
        }
        function circle(x, y, r) {
            c.beginPath()
            c.arc(x, y, r, 0, 2 * Math.PI)
            c.stroke()
        }
        function triangle(x, y, w, h) {
            c.beginPath()
            c.moveTo(x, y - h / 2)
            c.lineTo(x + w, y)
            c.lineTo(x, y + h / 2)
            c.closePath()
            c.fill()
        }

        switch (name) {
        case "search":
            circle(10.3, 10.3, 5.7)
            line(14.5, 14.5, 20, 20)
            break
        case "disc":
            circle(12, 12, 9)
            circle(12, 12, 2.3)
            c.beginPath()
            c.arc(12, 12, 6, 3.5, 4.7)
            c.stroke()
            c.beginPath()
            c.arc(12, 12, 6, 0.35, 1.6)
            c.stroke()
            break
        case "queue":
            line(4, 6, 20, 6)
            line(4, 12, 15, 12)
            line(4, 18, 12, 18)
            triangle(16, 16.5, 5, 6)
            break
        case "play":
            triangle(8, 12, 10, 13)
            break
        case "pause":
            c.fillRect(7, 5, 3.5, 14)
            c.fillRect(13.5, 5, 3.5, 14)
            break
        case "next":
            triangle(6, 12, 10, 11)
            c.fillRect(17, 6.5, 2, 11)
            break
        case "previous":
            c.translate(24, 0)
            c.scale(-1, 1)
            triangle(6, 12, 10, 11)
            c.fillRect(17, 6.5, 2, 11)
            break
        case "volume":
        case "muted":
            c.beginPath()
            c.moveTo(3, 9)
            c.lineTo(7, 9)
            c.lineTo(12, 5)
            c.lineTo(12, 19)
            c.lineTo(7, 15)
            c.lineTo(3, 15)
            c.closePath()
            c.stroke()
            if (name === "muted") {
                line(16, 9, 21, 15)
                line(21, 9, 16, 15)
            } else {
                c.beginPath()
                c.arc(12, 12, 5, -0.75, 0.75)
                c.stroke()
                c.beginPath()
                c.arc(12, 12, 8.5, -0.75, 0.75)
                c.stroke()
            }
            break
        case "home":
            c.beginPath()
            c.moveTo(3, 11)
            c.lineTo(12, 4)
            c.lineTo(21, 11)
            c.lineTo(19, 11)
            c.lineTo(19, 20)
            c.lineTo(5, 20)
            c.lineTo(5, 11)
            c.closePath()
            c.stroke()
            line(12, 15, 12, 20)
            break
        case "collection":
            c.strokeRect(5, 8, 14, 12)
            line(7, 4, 17, 4)
            line(12, 11, 12, 17)
            line(12, 11, 16, 10)
            break
        case "podcast":
            circle(12, 11, 2.3)
            line(12, 13.5, 12, 20)
            for (const radius of [5, 8.5]) {
                c.beginPath()
                c.arc(12, 11, radius, Math.PI * .72, Math.PI * 1.28)
                c.stroke()
                c.beginPath()
                c.arc(12, 11, radius, -.88, .88)
                c.stroke()
            }
            break
        case "people":
        case "user":
            circle(name === "people" ? 9 : 12, 8, 3)
            c.beginPath()
            c.arc(name === "people" ? 9 : 12, 20, 5.5, Math.PI, 2 * Math.PI)
            c.stroke()
            if (name === "people") {
                c.beginPath()
                c.arc(16, 8, 2.4, -1.1, 1.1)
                c.stroke()
                c.beginPath()
                c.arc(16.5, 20, 4.2, 4.7, 6.2)
                c.stroke()
            }
            break
        case "heart":
            c.beginPath()
            c.moveTo(12, 20)
            c.bezierCurveTo(8, 17, 3, 13, 3, 9)
            c.bezierCurveTo(3, 4, 9, 3, 12, 7)
            c.bezierCurveTo(15, 3, 21, 4, 21, 9)
            c.bezierCurveTo(21, 13, 16, 17, 12, 20)
            c.closePath()
            if (filled)
                c.fill()
            c.stroke()
            break
        case "history":
            circle(12, 12, 8.5)
            line(12, 7, 12, 12)
            line(12, 12, 16, 14)
            break
        case "star":
            c.beginPath()
            for (let index = 0; index < 10; ++index) {
                const angle = -Math.PI / 2 + index * Math.PI / 5
                const radius = index % 2 === 0 ? 9 : 4.2
                const x = 12 + Math.cos(angle) * radius
                const y = 12 + Math.sin(angle) * radius
                if (index === 0) c.moveTo(x, y)
                else c.lineTo(x, y)
            }
            c.closePath()
            c.stroke()
            break
        case "up":
            line(6, 15, 12, 9)
            line(12, 9, 18, 15)
            break
        case "down":
            line(6, 9, 12, 15)
            line(12, 15, 18, 9)
            break
        case "plus":
            line(5, 12, 19, 12)
            line(12, 5, 12, 19)
            break
        case "output":
            c.strokeRect(5, 5, 14, 11)
            line(9, 20, 15, 20)
            line(12, 16, 12, 20)
            break
        case "trash":
            line(5, 7, 19, 7)
            line(9, 4, 15, 4)
            line(7, 7, 8, 20)
            line(17, 7, 16, 20)
            line(8, 20, 16, 20)
            line(11, 10, 11, 17)
            line(14, 10, 14, 17)
            break
        case "refresh":
            c.beginPath()
            c.arc(12, 12, 7, -0.65, 4.9)
            c.stroke()
            line(18, 5, 18, 10)
            line(18, 10, 13, 10)
            break
        case "chevron":
            line(9, 5, 16, 12)
            line(16, 12, 9, 19)
            break
        case "back":
            line(10, 6, 4, 12)
            line(4, 12, 10, 18)
            line(4, 12, 20, 12)
            break
        case "minimize":
            line(6, 13, 18, 13)
            break
        case "maximize":
            c.strokeRect(6, 6, 12, 12)
            break
        case "restore":
            c.strokeRect(5, 9, 10, 10)
            line(9, 9, 9, 5)
            line(9, 5, 19, 5)
            line(19, 5, 19, 15)
            line(19, 15, 15, 15)
            break
        case "close":
            line(6, 6, 18, 18)
            line(6, 18, 18, 6)
            break
        case "more":
            for (const x of [6, 12, 18]) {
                c.beginPath()
                c.arc(x, 12, 1.5, 0, 2 * Math.PI)
                c.fill()
            }
            break
        case "comment":
            c.beginPath()
            c.moveTo(4.5, 5.5)
            c.lineTo(19.5, 5.5)
            c.lineTo(19.5, 16)
            c.lineTo(11, 16)
            c.lineTo(7, 19.5)
            c.lineTo(7, 16)
            c.lineTo(4.5, 16)
            c.closePath()
            c.stroke()
            line(8.5, 9.5, 15.5, 9.5)
            line(8.5, 12.5, 13, 12.5)
            break
        case "collect":
            c.strokeRect(5, 8, 14, 11)
            line(7.5, 5, 16.5, 5)
            line(12, 11, 12, 16)
            line(9.5, 13.5, 14.5, 13.5)
            break
        case "collected":
            c.strokeRect(5, 8, 14, 11)
            line(7.5, 5, 16.5, 5)
            line(9, 13.5, 11.3, 15.8)
            line(11.3, 15.8, 15.2, 11.6)
            break
        case "lock":
            c.strokeRect(6, 11, 12, 9)
            c.beginPath()
            c.moveTo(8.5, 11)
            c.lineTo(8.5, 8)
            c.arc(12, 8, 3.5, Math.PI, 2 * Math.PI)
            c.lineTo(15.5, 11)
            c.stroke()
            line(12, 14.5, 12, 16.5)
            break
        case "check":
            line(5, 12.5, 10, 17.5)
            line(10, 17.5, 19, 7)
            break
        case "dislike":
            c.beginPath()
            c.moveTo(12, 20)
            c.bezierCurveTo(8, 17, 3, 13, 3, 9)
            c.bezierCurveTo(3, 4, 9, 3, 12, 7)
            c.bezierCurveTo(15, 3, 21, 4, 21, 9)
            c.bezierCurveTo(21, 13, 16, 17, 12, 20)
            c.closePath()
            c.stroke()
            line(5, 20, 19, 4)
            break
        case "loop":
            c.beginPath()
            c.moveTo(17, 7)
            c.lineTo(8, 7)
            c.arc(8, 11, 4, -Math.PI / 2, Math.PI / 2, true)
            c.stroke()
            c.beginPath()
            c.moveTo(7, 17)
            c.lineTo(16, 17)
            c.arc(16, 13, 4, Math.PI / 2, -Math.PI / 2, true)
            c.stroke()
            line(15, 4.5, 17.5, 7)
            line(17.5, 7, 15, 9.5)
            line(9, 14.5, 6.5, 17)
            line(6.5, 17, 9, 19.5)
            break
        case "loopOne":
            c.beginPath()
            c.moveTo(17, 7)
            c.lineTo(8, 7)
            c.arc(8, 11, 4, -Math.PI / 2, Math.PI / 2, true)
            c.stroke()
            c.beginPath()
            c.moveTo(7, 17)
            c.lineTo(16, 17)
            c.arc(16, 13, 4, Math.PI / 2, -Math.PI / 2, true)
            c.stroke()
            line(11, 11, 12.3, 10)
            line(12.3, 10, 12.3, 14.2)
            break
        case "shuffle":
            line(4, 7, 8, 7)
            line(8, 7, 15, 17)
            line(15, 17, 20, 17)
            line(4, 17, 8, 17)
            line(8, 17, 10, 14.2)
            line(13, 9.8, 15, 7)
            line(15, 7, 20, 7)
            line(17.7, 4.7, 20, 7)
            line(20, 7, 17.7, 9.3)
            line(17.7, 14.7, 20, 17)
            line(20, 17, 17.7, 19.3)
            break
        case "sequence":
            line(4, 7, 16, 7)
            line(4, 12, 16, 12)
            line(4, 17, 12, 17)
            line(19, 6, 19, 18)
            line(16.5, 15.5, 19, 18)
            line(19, 18, 21.5, 15.5)
            break
        case "skin":
            c.beginPath()
            c.moveTo(9, 4.5)
            c.lineTo(4, 8)
            c.lineTo(6, 11.5)
            c.lineTo(8, 10.5)
            c.lineTo(8, 19.5)
            c.lineTo(16, 19.5)
            c.lineTo(16, 10.5)
            c.lineTo(18, 11.5)
            c.lineTo(20, 8)
            c.lineTo(15, 4.5)
            c.bezierCurveTo(14, 7, 10, 7, 9, 4.5)
            c.closePath()
            c.stroke()
            break
        case "left":
            line(14.5, 5.5, 8, 12)
            line(8, 12, 14.5, 18.5)
            break
        case "headphones":
            c.beginPath()
            c.arc(12, 13, 7.5, Math.PI, 2 * Math.PI)
            c.stroke()
            c.fillRect(3.5, 13, 3.6, 6)
            c.fillRect(16.9, 13, 3.6, 6)
            break
        case "trendUp":
            triangle(0, 0, 0, 0)
            c.beginPath()
            c.moveTo(12, 6)
            c.lineTo(18, 17)
            c.lineTo(6, 17)
            c.closePath()
            c.fill()
            break
        case "trendDown":
            c.beginPath()
            c.moveTo(12, 18)
            c.lineTo(18, 7)
            c.lineTo(6, 7)
            c.closePath()
            c.fill()
            break
        case "link":
            c.beginPath()
            c.arc(8.5, 15.5, 3.8, 0.4, 4.3)
            c.stroke()
            c.beginPath()
            c.arc(15.5, 8.5, 3.8, 3.55, 7.45)
            c.stroke()
            line(9.5, 14.5, 14.5, 9.5)
            break
        case "music":
            line(10, 6, 10, 17)
            line(10, 6, 19, 4)
            line(19, 4, 19, 15)
            circle(7, 17, 3)
            circle(16, 15, 3)
            break
        case "style":
            c.strokeRect(4, 4, 7, 7)
            c.strokeRect(13, 4, 7, 7)
            c.strokeRect(4, 13, 7, 7)
            circle(16.5, 16.5, 3.5)
            break
        case "opacity":
            c.beginPath()
            c.moveTo(12, 3.5)
            c.bezierCurveTo(15.5, 8.2, 18.5, 11.4, 18.5, 14.5)
            c.arc(12, 14.5, 6.5, 0, Math.PI, false)
            c.bezierCurveTo(5.5, 11.4, 8.5, 8.2, 12, 3.5)
            c.closePath()
            c.stroke()
            c.beginPath()
            c.arc(12, 14.5, 4.2, 0, Math.PI, false)
            c.closePath()
            c.fill()
            break
        }
    }
}
