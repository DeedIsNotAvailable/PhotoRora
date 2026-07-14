cmake_minimum_required(VERSION 3.5)
project(ru.omgtu.PhotoRora CXX)

include(GNUInstallDirs)
include(FindPkgConfig)

find_package(Qt5 COMPONENTS Core Qml Gui Quick LinguistTools REQUIRED)

pkg_search_module(AURORA auroraapp REQUIRED)
pkg_check_modules(ONNXRUNTIME REQUIRED IMPORTED_TARGET onnxruntime)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}/lib")

set(PROJECT_SHORT_NAME PhotoRora)

file(GLOB TS_FILES "translations/*.ts")
qt5_add_translation(QM_FILES ${TS_FILES})

set(SOURCES
    src/main.cpp
    src/imagecontroller.cpp
    src/onnxworker.cpp
)

set(HEADERS
    src/aiimageprovider.h
    src/imagecontroller.h
    src/onnxworker.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${QM_FILES})
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${AURORA_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Quick
    ${AURORA_LDFLAGS}
    PkgConfig::ONNXRUNTIME
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY qml
    DESTINATION share/${PROJECT_NAME}
)
install(FILES data/bgremoval.onnx
              data/candy-9.onnx
              data/mosaic-9.onnx
              data/AnimeGANv2_Paprika.onnx
              data/AnimeGANv2_Shinkai.onnx
    DESTINATION share/${PROJECT_NAME}/lib
)

foreach(_qm_file IN LISTS QM_FILES)
    get_filename_component(_qm_name "${_qm_file}" NAME)
    string(REPLACE "${PROJECT_SHORT_NAME}" "${PROJECT_NAME}" _full_qm_name "${_qm_name}")
    install(FILES ${_qm_file} DESTINATION share/${PROJECT_NAME}/translations RENAME ${_full_qm_name})
endforeach()

set(ICON_SIZES 86x86 108x108 128x128 172x172)
foreach(_size IN LISTS ICON_SIZES)
    install(FILES icons/${_size}/${PROJECT_NAME}.png DESTINATION share/icons/hicolor/${_size}/apps)
endforeach()

install(FILES ${PROJECT_NAME}.desktop
    DESTINATION share/applications
)

file(GLOB_RECURSE ICON_FILES "icons/*.png")
file(GLOB_RECURSE QML_FILES "qml/*.qml")

add_custom_target(distfiles
    SOURCES
        ${PROJECT_NAME}.desktop
        rpm/ru.omgtu.photorora.spec
        ${QML_FILES}
        ${TS_FILES}
        ${ICON_FILES}
        data/bgremoval.onnx
        data/candy-9.onnx
        data/mosaic-9.onnx
        data/AnimeGANv2_Paprika.onnx
        data/AnimeGANv2_Shinkai.onnx
        ${HEADERS}
)

file(WRITE "${CMAKE_BINARY_DIR}/QtCreatorDeployment.txt"
    "${CMAKE_INSTALL_PREFIX}\n"
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}:bin\n"
)
