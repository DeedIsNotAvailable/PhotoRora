import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0

FilePickerPage {
    id: pickerPage
    signal fileSelected(string filePath)

    title: qsTr("Выберите файл")
    nameFilters: [ "*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp" ]

    onSelectedContentPropertiesChanged: {
        if (selectedContentProperties && selectedContentProperties.filePath) {
            fileSelected(selectedContentProperties.filePath)
        }
    }
}
