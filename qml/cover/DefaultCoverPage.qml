import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    objectName: "defaultCover"

    CoverTemplate {
        objectName: "applicationCover"
        primaryText: "PR"
        secondaryText: qsTr("Работай с изображением!")
        icon {
            source: Qt.resolvedUrl("../icons/PhotoRora.svg")
            sourceSize { width: icon.width; height: icon.height }
        }
    }
}
