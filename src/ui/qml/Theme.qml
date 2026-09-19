pragma Singleton
import QtQuick

// Design tokens from docs/raspberry-milkshake-visual-profile/.../tokens/raspberry-milkshake.tokens.json.
// tests/ui_theme_test.cpp checks that the values below match that file.
QtObject {
    readonly property color canvas: "#18181C"
    readonly property color surface: "#222228"
    readonly property color surfaceRaised: "#2B2B32"
    readonly property color border: "#3D3F46"
    readonly property color text: "#F2F3F5"
    readonly property color textMuted: "#9DA1AA"
    readonly property color raspberry: "#E84F8C"
    readonly property color milk: "#FFB8D2"
    readonly property color raspberrySoft: "#F47CA9"
    readonly property color signal: "#7AA2F7"
    readonly property color focus: "#FFB8D2"

    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space6: 24
    readonly property int space8: 32

    readonly property int radiusControl: 6
    readonly property int radiusPanel: 10
    readonly property int radiusApp: 16

    readonly property int durationFast: 160
    readonly property int durationBase: 240
    readonly property int durationSlow: 360
    // ease-liquid: cubic-bezier(.22, 1, .36, 1)
    readonly property var easingCurve: [0.22, 1, 0.36, 1, 1, 1]

    // The brand desktop sizes scaled about 1.7x for a finger-operated 7 inch panel (about 8.3 px/mm).
    readonly property string fontFamily: "Inter"
    readonly property int fontControl: 22
    readonly property int fontMeta: 20
    readonly property int fontTitle: 34
    readonly property int targetMin: 72

    // Freezes the vortex and live dot; set from config when available.
    property bool reducedMotion: false
}
