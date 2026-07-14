import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: mainPage
    allowedOrientations: Orientation.All

    property string statusMessage: qsTr("Выберите фотографию из галереи или из системных файлов.")
    property bool statusIsError: false
    property bool hasPreview: previewImage.source.toString().length > 0
    property var backgroundPalette: ["#f4efe7", "#e8eef5", "#f1d7d7", "#2c3440", "#0f6d5b"]
    property var stylePalette: [
        { id: "candy", title: qsTr("Candy") },
        { id: "mosaic", title: qsTr("Mosaic") }
    ]

    function applySelectedFile(filePath) {
        if (!filePath || filePath === "") {
            statusMessage = qsTr("Не удалось получить путь к выбранному изображению.")
            statusIsError = true
            return
        }

        previewImage.source = "file://" + filePath
        statusMessage = qsTr("Фотография выбрана. Импортируем её в приложение...")
        statusIsError = false
        ImageController.loadImage(filePath)
    }

    function openGalleryPicker() { pageStack.push(galleryPickerComponent) }
    function openFilePicker() { pageStack.push(filePickerComponent) }

    Connections {
        target: ImageController

        onImageLoadedSuccessfully: {
            mainPage.statusMessage = qsTr("Изображение успешно импортировано.")
            mainPage.statusIsError = false
        }

        onErrorOccurred: {
            mainPage.statusMessage = message
            mainPage.statusIsError = true
        }

        onContourReady: {
            previewImage.source = "image://ai_provider/result?rand=" + Math.random()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Выбрать из галереи")
                enabled: !ImageController.isProcessing && !ImageController.canUndo
                onClicked: mainPage.openGalleryPicker()
            }
            MenuItem {
                text: qsTr("Выбрать из файлов")
                enabled: !ImageController.isProcessing && !ImageController.canUndo
                onClicked: mainPage.openFilePicker()
            }
        }

        Column {
            id: contentColumn
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("Работа с изображениями") }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: ImageController.isProcessing
                size: BusyIndicatorSize.Medium
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: ImageController.isProcessing
                enabled: ImageController.isProcessing
                text: qsTr("Отменить обработку")
                onClicked: ImageController.cancelProcessing()
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
                    border.color: "#4d6f8ca1"
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
                            font.pixelSize: Theme.fontSizeLarge
                            color: Theme.highlightColor
                            text: ImageController.aiResult !== "" ? ImageController.aiResult : qsTr("Редактируйте с удовольствием!")
                        }

                        Rectangle {
                            width: parent.width
                            height: Math.min(mainPage.width - Theme.paddingLarge * 4, 420)
                            radius: Theme.paddingMedium
                            color: "#0d1821"
                            border.width: 1
                            border.color: "#4f7f8f"
                            clip: true

                            Image {
                                id: previewImage
                                anchors.fill: parent
                                anchors.margins: Theme.paddingSmall
                                fillMode: Image.PreserveAspectFit
                                visible: mainPage.hasPreview
                                smooth: true
                            }

                            Column {
                                anchors.centerIn: parent
                                width: parent.width - Theme.paddingLarge * 2
                                spacing: Theme.paddingSmall
                                visible: !mainPage.hasPreview
                                z: 1

                                Label {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                    color: Theme.highlightColor
                                    font.pixelSize: Theme.fontSizeLarge
                                    text: qsTr("Здесь появится предпросмотр")
                                }

                                Label {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                    color: Theme.secondaryHighlightColor
                                    font.pixelSize: Theme.fontSizeSmall
                                    text: qsTr("Поддерживаемые форматы: JPG, PNG, BMP, WEBP")
                                }
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: Theme.paddingMedium
                            visible: mainPage.hasPreview

                            Button {
                                width: (parent.width - Theme.paddingMedium * 2) / 3
                                text: qsTr("Отмена")
                                enabled: ImageController.canUndo && !ImageController.isProcessing
                                onClicked: ImageController.undo()
                            }

                            Button {
                                width: (parent.width - Theme.paddingMedium * 2) / 3
                                text: qsTr("Сброс")
                                enabled: ImageController.canUndo && !ImageController.isProcessing
                                onClicked: ImageController.resetToOriginal()
                            }

                            Button {
                                width: (parent.width - Theme.paddingMedium * 2) / 3
                                text: qsTr("Сохранить")
                                enabled: ImageController.canUndo && !ImageController.isProcessing
                                onClicked: ImageController.exportResult()
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

            SectionHeader {
                visible: mainPage.hasPreview
                text: qsTr("Инструменты")
            }

            Rectangle {
                visible: mainPage.hasPreview
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin
                radius: Theme.paddingLarge
                color: "#11202b"
                border.width: 1
                border.color: "#3f5f6f"
                height: toolsColumn.height + Theme.paddingLarge * 2

                Column {
                    id: toolsColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.paddingLarge
                    spacing: Theme.paddingLarge

                    Column {
                        width: parent.width
                        spacing: Theme.paddingSmall

                        Label {
                            width: parent.width
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeMedium
                            text: qsTr("Фон")
                        }

                        Label {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: qsTr("Сегментация объекта и работа с задним планом")
                        }

                        Grid {
                            id: backgroundToolsGrid
                            width: parent.width
                            columns: 2
                            spacing: Theme.paddingMedium

                            Button {
                                width: (backgroundToolsGrid.width - backgroundToolsGrid.spacing) / 2
                                text: qsTr("Убрать фон")
                                enabled: !ImageController.isProcessing
                                onClicked: ImageController.triggerBackgroundRemoval()
                            }

                            Button {
                                width: (backgroundToolsGrid.width - backgroundToolsGrid.spacing) / 2
                                text: qsTr("Фон + blur")
                                enabled: !ImageController.isProcessing
                                onClicked: ImageController.triggerBackgroundBlur()
                            }

                            Button {
                                width: (backgroundToolsGrid.width - backgroundToolsGrid.spacing) / 2
                                text: qsTr("Фон + цвет")
                                enabled: !ImageController.isProcessing
                                onClicked: ImageController.triggerBackgroundColor()
                            }
                        }

                        Label {
                            width: parent.width
                            color: Theme.secondaryHighlightColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: qsTr("Цвет фона")
                        }

                        Row {
                            width: parent.width
                            spacing: Theme.paddingSmall

                            Repeater {
                                model: mainPage.backgroundPalette

                                Rectangle {
                                    width: Theme.itemSizeSmall
                                    height: Theme.itemSizeSmall
                                    radius: width / 2.5
                                    color: modelData
                                    border.width: ImageController.backgroundColorHex === modelData ? 4 : 2
                                    border.color: ImageController.backgroundColorHex === modelData ? Theme.highlightColor : "#6f8796"
                                    opacity: ImageController.isProcessing ? 0.5 : 1.0

                                    MouseArea {
                                        anchors.fill: parent
                                        enabled: !ImageController.isProcessing
                                        onClicked: ImageController.setBackgroundColor(modelData)
                                    }
                                }
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.paddingSmall

                        Label {
                            width: parent.width
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeMedium
                            text: qsTr("Улучшение")
                        }

                        Label {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: qsTr("Быстрая автокоррекция контраста и яркости")
                        }

                        Button {
                            width: parent.width
                            text: qsTr("Улучшить")
                            enabled: !ImageController.isProcessing
                            onClicked: ImageController.triggerEnhancement()
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.paddingSmall

                        Label {
                            width: parent.width
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeMedium
                            text: qsTr("Стиль")
                        }

                        Label {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: qsTr("Выберите художественный пресет и примените нейросетевую стилизацию")
                        }

                        Row {
                            width: parent.width
                            spacing: Theme.paddingSmall

                            Repeater {
                                model: mainPage.stylePalette

                                Rectangle {
                                    width: (parent.width - Theme.paddingSmall) / 2
                                    height: Theme.itemSizeMedium
                                    radius: Theme.paddingMedium
                                    color: ImageController.selectedStyleId === modelData.id ? "#2f4f61" : "#1a2f3c"
                                    border.width: 1
                                    border.color: ImageController.selectedStyleId === modelData.id ? Theme.highlightColor : "#577487"
                                    opacity: ImageController.isProcessing ? 0.5 : 1.0

                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.title
                                        color: Theme.primaryColor
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        enabled: !ImageController.isProcessing
                                        onClicked: ImageController.setSelectedStyle(modelData.id)
                                    }
                                }
                            }
                        }

                        Button {
                            width: parent.width
                            text: qsTr("Стиль")
                            enabled: !ImageController.isProcessing
                            onClicked: ImageController.triggerStyleTransfer()
                        }
                    }
                }
            }
        }
    }

    Component { id: galleryPickerComponent; GalleryPickerPage { onFileSelected: mainPage.applySelectedFile(filePath) } }
    Component { id: filePickerComponent; SystemFilePickerPage { onFileSelected: mainPage.applySelectedFile(filePath) } }
}
