import QtQuick 2.0
import QtQuick.Window 2.0
import QtQuick.Controls 2.0

/** Button that supports an automatically scaled image.
    The image source should be a SVG file with size 22x22. */
Button {
    id: root

    property alias imageSource: image.source

    focusPolicy: Qt.NoFocus
    contentItem: Image {
        id: image

        sourceSize {
            // Try to use 22x22, unless high DPI
            width: Screen.pixelDensity < 5 ? 22 : 5 * Screen.pixelDensity
            height: Screen.pixelDensity < 5 ? 22 : 5 * Screen.pixelDensity
        }
        fillMode: Image.PreserveAspectFit
        opacity: root.enabled ? 1 : 0.4
    }
    background: Rectangle {
        anchors.fill: root
        visible: down
        color: theme.backgroundButtonPressed
    }
}
