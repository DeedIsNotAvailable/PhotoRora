import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0

Page {
    id: mainPage
    allowedOrientations: Orientation.All

    property string statusMessage: qsTr("Choose a photo from the gallery or from system files.")
    property bool statusIsError: false

    function applySelectedFile(picker) {
        var filePath = ""
        var props = picker.selectedContentProperties[{}]

        if (props && props.filePath) {
            filePath = props.filePath
        } else if (picker.selectedContentPath) {
            filePath = picker.selectedContentPath
        }

        if (filePath === "") {
            statusMessage = qsTr("Could not get the path to the selected image.")
            statusIsError = true
            return
        }

        previewImage.source = "file://" + filePath
        statusMessage = qsTr("Photo selected. Importing into the application...")
        statusIsError = false
        ImageController.loadImage(filePath)
    }

    function openImagePicker() {
        var picker = pageStack.push(Qt.resolvedUrl("GalleryPickerPage.qml"))
        picker.accepted.connect(function() {
            applySelectedFile(picker)
        })
    }

    function openFilePicker() {
        var picker = pageStack.push(Qt.resolvedUrl("SystemFilePickerPage.qml"))
        picker.accepted.connect(function() {
            applySelectedFile(picker)
        })
    }

    Connections {
        target: ImageController

        onImageLoadedSuccessfully: {
            mainPage.statusMessage = qsTr("Image imported successfully. You can continue working with it.")
            mainPage.statusIsError = false
        }

        onErrorOccurred: {
            mainPage.statusMessage = message
            mainPage.statusIsError = true
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Choose from gallery")
                onClicked: mainPage.openImagePicker()
            }
            MenuItem {
                text: qsTr("Choose from files")
                onClicked: mainPage.openFilePicker()
            }
        }

        Column {
            id: contentColumn
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Photo import")
            }

            Label {
                width: parent.width - Theme.horizontalPageMargin * 2
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Prepare an image for further offline editing")
            }

            Item {
                width: parent.width
                height: previewCard.height

                Rectangle {
                    id: previewCard
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: Theme.horizontalPageMargin
                    anchors.rightMargin: Theme.horizontalPageMargin
                    radius: Theme.paddingLarge
                    color: "#162734"
                    border.width: 2
                    border.color: "#336f8ca1"
                    height: previewColumn.height + Theme.paddingLarge * 2

                    Column {
                        id: previewColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.paddingLarge
                        spacing: Theme.paddingMedium

                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            font.pixelSize: Theme.fontSizeExtraLarge
                            color: Theme.highlightColor
                            text: qsTr("Your next edit starts with a clean import")
                        }

                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.secondaryColor
                            text: qsTr("Open a photo from the gallery for the main scenario or use files as a fallback.")
                        }

                        Rectangle {
                            width: parent.width
                            height: Math.min(mainPage.width - Theme.paddingLarge * 4, 420)
                            radius: Theme.paddingMedium
                            color: "#0d1821"
                            border.width: 1
                            border.color: "#334f7f8f"
                            clip: true

                            Image {
                                id: previewImage
                                anchors.fill: parent
                                anchors.margins: Theme.paddingSmall
                                fillMode: Image.PreserveAspectFit
                                visible: source !== ""
                                smooth: true
                            }

                            Column {
                                anchors.centerIn: parent
                                width: parent.width - Theme.paddingLarge * 2
                                spacing: Theme.paddingSmall
                                visible: !previewImage.visible

                                Label {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    color: Theme.secondaryHighlightColor
                                    font.pixelSize: Theme.fontSizeLarge
                                    text: qsTr("Preview will appear here")
                                }

                                Label {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                    color: Theme.secondaryColor
                                    font.pixelSize: Theme.fontSizeSmall
                                    text: qsTr("Supported formats: JPG, PNG, BMP, WEBP")
                                }
                            }
                        }

                        Label {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            color: statusIsError ? Theme.errorColor : Theme.secondaryHighlightColor
                            font.pixelSize: Theme.fontSizeSmall
                            text: statusMessage
                        }
                    }
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Choose from gallery")
                onClicked: mainPage.openImagePicker()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Choose from files")
                onClicked: mainPage.openFilePicker()
            }
        }
    }
}
