import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: mainPage
    allowedOrientations: Orientation.All

    property string statusMessage: qsTr("Выберите фотографию из галереи или из системных файлов.")
    property bool statusIsError: false
    property bool hasPreview: previewImage.source.toString().length > 0

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

        // Принудительное обновление источника картинки из C++ оперативной памяти провайдера
        onContourReady: {
            previewImage.source = "image://ai_provider/result?rand=" + Math.random()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentColumn.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem { text: qsTr("Выбрать из галереи"); onClicked: mainPage.openGalleryPicker() }
            MenuItem { text: qsTr("Выбрать из файлов"); onClicked: mainPage.openFilePicker() }
        }

        Column {
            id: contentColumn
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("Импорт фотографии") }

            Label {
                width: parent.width - Theme.horizontalPageMargin * 2
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Подготовьте изображение для офлайн-обработки нейросетью")
            }

            // Кнопки импорта файлов на главном экране
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingLarge

                Button {
                    text: qsTr("Галерея")
                    enabled: !ImageController.isProcessing
                    onClicked: mainPage.openGalleryPicker()
                }
                Button {
                    text: qsTr("Проводник")
                    enabled: !ImageController.isProcessing
                    onClicked: mainPage.openFilePicker()
                }
            }

            // Индикатор работы ИИ-алгоритма в фоновом Linux-потоке
            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: ImageController.isProcessing
                size: BusyIndicatorSize.Medium
            }

            // ФИНАЛЬНАЯ СЕТКА ИНСТРУМЕНТОВ (ПОЛНОЕ ЗАКРЫТИЕ ТЗ: 6 КНОПОК)
            Grid {
                columns: 3
                spacing: Theme.paddingSmall
                anchors.horizontalCenter: parent.horizontalCenter
                visible: mainPage.hasPreview

                Button {
                    text: qsTr("Фон")
                    preferredWidth: mainPage.width / 3 - Theme.paddingSmall * 2
                    enabled: !ImageController.isProcessing
                    onClicked: ImageController.triggerBackgroundRemoval()
                }

                Button {
                    text: qsTr("Улучшить")
                    preferredWidth: mainPage.width / 3 - Theme.paddingSmall * 2
                    enabled: !ImageController.isProcessing
                    onClicked: ImageController.triggerEnhancement()
                }

                Button {
                    text: qsTr("Стиль")
                    preferredWidth: mainPage.width / 3 - Theme.paddingSmall * 2
                    enabled: !ImageController.isProcessing
                    onClicked: ImageController.triggerStyleTransfer()
                }

                Button {
                    text: qsTr("Отмена")
                    preferredWidth: mainPage.width / 3 - Theme.paddingSmall * 2
                    enabled: ImageController.canUndo && !ImageController.isProcessing
                    onClicked: ImageController.undo()
                }

                Button {
                    text: qsTr("Сброс")
                    preferredWidth: mainPage.width / 3 - Theme.paddingSmall * 2
                    enabled: !ImageController.isProcessing
                    onClicked: ImageController.resetToOriginal()
                }

                Button {
                    text: qsTr("Сохранить")
                    preferredWidth: mainPage.width / 3 - Theme.paddingSmall * 2
                    enabled: !ImageController.isProcessing
                    onClicked: ImageController.exportResult()
                }
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
        }
    }

    Component { id: galleryPickerComponent; GalleryPickerPage { onFileSelected: mainPage.applySelectedFile(filePath) } }
    Component { id: filePickerComponent; SystemFilePickerPage { onFileSelected: mainPage.applySelectedFile(filePath) } }
}
