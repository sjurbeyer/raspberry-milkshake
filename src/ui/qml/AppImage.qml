import QtQuick

// An image that also handles SVG through the app's own SVG provider (see svg_image_provider.hpp).
Image {
    id: root
    property url location: ""
    source: {
        const s = location.toString()
        if (s === "") return ""
        return s.toLowerCase().endsWith(".svg") ? "image://svg/" + encodeURIComponent(s) : s
    }
}
