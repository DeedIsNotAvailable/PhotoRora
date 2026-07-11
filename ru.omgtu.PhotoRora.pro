TARGET = ru.omgtu.PhotoRora
CONFIG += auroraapp
PKGCONFIG += auroraapp

# Убедитесь, что здесь стоят обратные слэши для переноса строк!
SOURCES += src/main.cpp \
    src/imagecontroller.cpp \
    src/onnxworker.cpp

HEADERS += \
    src/aiimageprovider.h \
    src/imagecontroller.h \
    src/onnxworker.h

DISTFILES += rpm/ru.omgtu.PhotoRora.spec \
    qml/pages/SystemFilePickerPage.qml \
    qml/pages/GalleryPickerPage.qml

AURORAAPP_ICONS = 86x86 108x108 128x128 172x172
CONFIG += auroraapp_i18n
TRANSLATIONS += translations/ru.omgtu.PhotoRora.ts \
                translations/ru.omgtu.PhotoRora-ru.ts


# Автоматическое подключение библиотек Conan в Аврора SDK 5.2+
exists(conanbuildinfo.pri) {
    include(conanbuildinfo.pri)
    CONFIG += conan
}

# Оставляем только заголовочные файлы для компиляции C++ кода
INCLUDEPATH += $$PWD/onnx/include

# Упаковываем ТОЛЬКО файл ИИ-модели (на него линтер не ругается)
target_model.files = data/model.onnx
target_model.path = /usr/share/ru.omgtu.PhotoRora/lib
INSTALLS += target_model
