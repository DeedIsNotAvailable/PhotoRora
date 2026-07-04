TARGET = ru.template.PhotoRora

CONFIG += \
    auroraapp

PKGCONFIG += \

SOURCES += \
    src/main.cpp \

HEADERS += \

DISTFILES += \
    rpm/ru.template.PhotoRora.spec \

AURORAAPP_ICONS = 86x86 108x108 128x128 172x172

CONFIG += auroraapp_i18n

TRANSLATIONS += \
    translations/ru.template.PhotoRora.ts \
    translations/ru.template.PhotoRora-ru.ts \
