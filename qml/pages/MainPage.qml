import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0

Page {
    id: mainPage
    allowedOrientations: Orientation.All

    Column {
        id: column
        anchors.fill: parent
        spacing: Theme.paddingLarge

        PageHeader {
            title: qsTr("Импорт фото для ИИ")
        }

        Button {
            text: qsTr("Выбрать фото из файлов")
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: {
                // Открываем страницу выбора файлов
                var filePicker = pageStack.push(Qt.resolvedUrl("FilePickerPage.qml"), {
                    title: qsTr("Выберите изображение"),
                    nameFilters: [ '*.jpg', '*.jpeg', '*.png' ]
                })

                // Безопасный обработчик выбора файла
                filePicker.accepted.connect(function() {
                    var filePath = ""
                    var props = filePicker.selectedContentProperties[{}]

                    if (props && props.filePath) {
                        filePath = props.filePath
                    } else {
                        filePath = filePicker.selectedContentPath
                    }

                    if (filePath !== "") {
                        previewImage.source = "file://" + filePath
                        ImageController.loadImage(filePath)
                    }
                })
            }
        }

        Image {
            id: previewImage
            width: parent.width - 2 * Theme.horizontalPageMargin
            height: width
            fillMode: Image.PreserveAspectFit
            anchors.horizontalCenter: parent.horizontalCenter
            visible: source !== ""
        }
    }
}
