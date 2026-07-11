import QtQuick 2.6
import Sailfish.Pickers 1.0

ImagePickerPage {
    id: pickerPage

    signal fileSelected(string filePath)

    onSelectedContentPropertiesChanged: {
        if (selectedContentProperties && selectedContentProperties.filePath) {
            fileSelected(selectedContentProperties.filePath)
        }
    }
}
