import QtQuick

// An image that also handles SVG through the app's own SVG provider (see svg_image_provider.hpp).
// Relative locations are resolved against this file, which sits next to the assets folder.
Image {
    id: root
    property url location: ""
    source: {
        const resolved = Qt.resolvedUrl(location).toString()
        if (location.toString() === "") return ""
        return resolved.toLowerCase().endsWith(".svg") ? "image://svg/" + encodeURIComponent(resolved) : resolved
    }
}
