TARGET = ru.omgtu.PhotoRora
CONFIG += auroraapp
PKGCONFIG += auroraapp

SOURCES += src/main.cpp \
    src/imagecontroller.cpp
HEADERS += \
    src/imagecontroller.h

DISTFILES += rpm/ru.omgtu.PhotoRora.spec \
    qml/pages/SystemFilePickerPage.qml \
    qml/pages/GalleryPickerPage.qml
AURORAAPP_ICONS = 86x86 108x108 128x128 172x172
CONFIG += auroraapp_i18n
TRANSLATIONS += translations/ru.omgtu.PhotoRora.ts \
                translations/ru.omgtu.PhotoRora-ru.ts
